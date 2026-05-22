#include "Bot.hpp"
#include "BotViz.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <ctime>

namespace bot {

namespace {
// Catch the earliest tick a divergence appears. With the file logger below
// (no rate limit, every drift > this threshold is recorded), we want to see
// micro-drift that compounds — alternating-input bugs typically show up as
// a handful of small early divergences before the path visibly diverges.
constexpr float kDivergenceLogDist    = 0.05f;
constexpr float kDivergenceLogDistSq  = kDivergenceLogDist * kDivergenceLogDist;

// Diagnostic divergence log at a fixed path so the user (running GD direct
// through Steam, no live geode log access) can share it post-session.
// Truncated on every PlayLayer init — one file per level entry. Single
// engine-thread access, so no mutex; flushed every line so a crash loses at
// most the in-flight write.
constexpr char const* kDivLogPath =
    "/Users/jedd/Desktop/GeometryDash/Autosolver/divergence.log";
FILE* g_divLog = nullptr;

void divlogOpenTruncate() {
    if (g_divLog) { std::fclose(g_divLog); g_divLog = nullptr; }
    g_divLog = std::fopen(kDivLogPath, "w");
    if (!g_divLog) return;
    std::time_t t = std::time(nullptr);
    std::fprintf(g_divLog, "# autosolver divergence log — session %s",
                 std::ctime(&t));
    std::fprintf(g_divLog,
        "# fields: tick=240Hz physics frame; idx=offset into plan; "
        "pStart=plan start frame; pred=(x,y) sim post-tick prediction; "
        "actual=(x,y) real post-tick position; d=(dx,dy) dist=sqrt; "
        "yVel=real player; simYVel=sim's yVel for the same tick; "
        "dYVel=simYVel-yVel (positive=sim faster upward); "
        "gravity/speed=real player; gnd=g1/g2/g3/g4; "
        "btnWant=plan input THIS tick; btnHeld=real button state; "
        "plan=binary context, [x] marks THIS tick\n");
    std::fflush(g_divLog);
}

void divlogClose() {
    if (g_divLog) { std::fclose(g_divLog); g_divLog = nullptr; }
}

void divlogf(char const* fmt, ...) {
    if (!g_divLog) return;
    std::va_list args;
    va_start(args, fmt);
    std::vfprintf(g_divLog, fmt, args);
    va_end(args);
    std::fputc('\n', g_divLog);
    std::fflush(g_divLog);
}
}



Bot& Bot::get() {
    static Bot s;
    return s;
}

void Bot::onPlayLayerInit(PlayLayer* pl) {
    m_pl          = pl;
    m_frame       = 0;
    m_visualFrame = 0;
    m_injecting   = false;
    m_lastHeld    = false;
    m_inSearch    = false;
    m_levelReady  = false;
    m_best        = BestPath{};
    m_candidateTraces.clear();
    BotViz::get().onPlayLayerInit(pl);
    divlogOpenTruncate();
}

void Bot::onPlayLayerReset() {
    m_frame       = 0;
    m_visualFrame = 0;
    m_injecting   = false;
    m_lastHeld    = false;
    m_inSearch    = false;
    m_best        = BestPath{};
    m_candidateTraces.clear();
    BotViz::get().clear();
}

void Bot::onPlayLayerQuit() {
    BotViz::get().onPlayLayerQuit();
    m_pl         = nullptr;
    m_frame      = 0;
    m_injecting  = false;
    m_lastHeld   = false;
    m_inSearch   = false;
    m_levelReady = false;
    m_best       = BestPath{};
    m_candidateTraces.clear();
    divlogClose();
}

void Bot::setEnabled(bool e) {
    if (m_enabled == e) return;
    m_enabled = e;
    if (!e) BotViz::get().clear();
}

void Bot::setDebugViz(DebugViz v) {
    m_viz = v;
    if (v == DebugViz::Off) BotViz::get().clear();
}

void Bot::setShowBotTraj(bool v) {
    if (m_showBotTraj == v) return;
    m_showBotTraj = v;
    if (!v) BotViz::get().clear();
}

bool Bot::inputForCurrentFrame() const {
    if (!m_enabled) return false;
    if (m_best.empty()) return m_lastHeld;

    int64_t const idx = m_frame - m_best.planStart;
    if (idx < 0 || idx >= static_cast<int64_t>(m_best.plan.size())) {
        return m_lastHeld;
    }
    return m_best.plan[static_cast<size_t>(idx)];
}

// P2 in 2P-mode levels has its own plan2; in regular dual (single-player level
// with a dual portal active) plan2 is empty and we fall back to lastHeld so
// P2 mirrors what P1 last set this tick.
//
// Off-by-one: this is called from BotPlayerObjectHook's P2 branch which runs
// AFTER P1's advanceFrame (engine call order: P1::update → P1 super →
// advanceFrame → P2::update). So m_frame already names the next tick (T+1)
// when this runs. Subtract one to read the same plan index P1 used for this
// tick, so plan and plan2 stay aligned under the same `idx`.
bool Bot::inputForCurrentFrameP2() const {
    if (!m_enabled) return false;
    if (m_best.empty() || m_best.plan2.empty()) return m_lastHeld;

    int64_t const idx = (m_frame - 1) - m_best.planStart;
    if (idx < 0 || idx >= static_cast<int64_t>(m_best.plan2.size())) {
        return m_lastHeld;
    }
    return m_best.plan2[static_cast<size_t>(idx)];
}

namespace {
// Compare cached samples[idx] against actual; if drift exceeds the user's
// divergence threshold, returns true and logs once for that tick.
bool checkDiscardFlip(int64_t frame,
                      bool                                      lastHeld,
                      cocos2d::CCPoint                          actualPos,
                      std::vector<cocos2d::CCPoint> const&      samples,
                      int64_t                                   planStart,
                      float                                     threshold,
                      char const*                               playerLabel) {
    if (samples.empty()) return false;
    int64_t const idx = frame - planStart;
    if (idx < 0 || static_cast<size_t>(idx) >= samples.size()) return false;
    auto const pred = samples[static_cast<size_t>(idx)];
    float const dx  = pred.x - actualPos.x;
    float const dy  = pred.y - actualPos.y;
    float const distSq = dx * dx + dy * dy;
    float const thrSq  = threshold * threshold;
    if (distSq <= thrSq) return false;
    geode::log::warn(
        "[plan-switch-discarded {}] tick={} pred=({:.2f},{:.2f}) actual=({:.2f},{:.2f}) "
        "dist={:.3f} > thr={:.2f} — keeping lastHeld={} instead of flip",
        playerLabel, frame,
        pred.x, pred.y, actualPos.x, actualPos.y,
        std::sqrt(distSq), threshold, lastHeld);
    return true;
}
}

bool Bot::shouldDiscardP1Flip(cocos2d::CCPoint actualPos) const {
    return checkDiscardFlip(m_frame, m_lastHeld, actualPos,
                            m_best.samples, m_best.planStart,
                            m_divergenceThreshold, "P1");
}

bool Bot::shouldDiscardP2Flip(cocos2d::CCPoint actualPos) const {
    // Only meaningful in 2P-mode where P2 has its own plan + samples2 — in
    // mirror mode P2's "plan" is just lastHeld so there's no flip to discard.
    if (m_best.plan2.empty() || m_best.samples2.empty()) return false;
    // Same off-by-one as inputForCurrentFrameP2: P2's hook runs after P1's
    // advanceFrame, so m_frame already names T+1. Subtract one to index
    // samples2 at the same tick P1 just used.
    return checkDiscardFlip(m_frame - 1, m_lastHeld, actualPos,
                            m_best.samples2, m_best.planStart,
                            m_divergenceThreshold, "P2");
}

void Bot::commitBest(BestPath&& nb) {
    m_best = std::move(nb);
}

bool Bot::stepVisualFrameAndShouldSearch() {
    int64_t const f = m_visualFrame++;
    return m_searchInterval <= 1 || (f % m_searchInterval) == 0;
}

// Called after PlayerObject::update from BotPlayerObjectHook. m_frame at entry
// names the tick that JUST ran (input from plan[m_frame - planStart]); the
// player is now at its post-tick position. The simulator's prediction for that
// position is samples[idx + 1] (samples[0] = pre-tick, samples[k+1] = post-tick
// of plan[k]).
//
// Divergence > kDivergenceLogDist almost certainly means a sim/reality
// discrepancy we want to fix (state-copy gap, button-mirror miss, etc., per
// the D1/D2/D3 audit list). Log player physics fields alongside so the source
// is identifiable from the trace.
void Bot::advanceFrame() {
    if (m_pl && m_pl->m_player1 && !m_best.empty()) {
        int64_t const idx = m_frame - m_best.planStart;
        size_t const predIdx = static_cast<size_t>(idx + 1);
        if (idx >= 0 && predIdx < m_best.samples.size()) {
            auto const pred   = m_best.samples[predIdx];
            auto const actual = m_pl->m_player1->getPosition();
            float const dx = pred.x - actual.x;
            float const dy = pred.y - actual.y;
            float const distSq = dx * dx + dy * dy;
            if (distSq > kDivergenceLogDistSq) {
                auto* p = m_pl->m_player1;
                bool const want = inputForCurrentFrame();
                bool const held = p->buttonDown(PlayerButton::Jump);

                // Plan binary context [idx-5 .. idx+5], with [.] bracketing
                // THIS tick's input. Lets us spot alternating-input patterns
                // (e.g. "0101[0]1010") just by eyeballing the log.
                char planCtx[24] = {0};
                size_t ctxLen = 0;
                int64_t const planSize = static_cast<int64_t>(m_best.plan.size());
                int64_t const lo = std::max<int64_t>(idx - 5, 0);
                int64_t const hi = std::min<int64_t>(idx + 5, planSize - 1);
                for (int64_t i = lo; i <= hi && ctxLen < sizeof(planCtx) - 4; ++i) {
                    if (i == idx) planCtx[ctxLen++] = '[';
                    planCtx[ctxLen++] = m_best.plan[static_cast<size_t>(i)] ? '1' : '0';
                    if (i == idx) planCtx[ctxLen++] = ']';
                }
                planCtx[ctxLen] = '\0';

                // Sim's yVel for the same tick (post-tick of plan[idx]).
                // samplesYVel is recorded in lockstep with samples — so the
                // index matching pred is the same predIdx. dyVel = sim - real,
                // i.e. positive means sim is higher upward.
                float simYVel  = 0.f;
                float dyVel    = 0.f;
                bool  haveYVel = false;
                if (predIdx < m_best.samplesYVel.size()) {
                    simYVel = m_best.samplesYVel[predIdx];
                    dyVel   = simYVel - p->m_yVelocity;
                    haveYVel = true;
                }

                divlogf("tick=%lld idx=%lld pStart=%lld "
                        "pred=(%.3f,%.3f) actual=(%.3f,%.3f) "
                        "d=(%+.4f,%+.4f) dist=%.4f "
                        "yVel=%.4f simYVel=%.4f dYVel=%+.4f%s "
                        "gravity=%.3f speed=%.3f "
                        "gnd=%d/%d/%d/%d btnWant=%d btnHeld=%d "
                        "jumpBuf=%d gravPortal=%d plan=%s",
                        (long long)m_frame, (long long)idx, (long long)m_best.planStart,
                        pred.x, pred.y, actual.x, actual.y,
                        dx, dy, std::sqrt(distSq),
                        p->m_yVelocity, simYVel, dyVel, haveYVel ? "" : "(noYVel)",
                        p->m_gravityMod, p->m_playerSpeed,
                        (int)p->m_isOnGround, (int)p->m_isOnGround2,
                        (int)p->m_isOnGround3, (int)p->m_isOnGround4,
                        (int)want, (int)held,
                        (int)p->m_jumpBuffered, (int)p->m_touchedGravityPortal,
                        planCtx);

                // Rate-limited geode log mirror — keeps live debugging usable
                // for runs where the user CAN see geode logs, without spamming
                // when running through Steam (file log is the source of truth).
                if ((m_frame - m_lastDivergenceLogFrame) >= 60) {
                    m_lastDivergenceLogFrame = m_frame;
                    geode::log::warn(
                        "[divergence] tick={} dist={:.3f} (full state in divergence.log)",
                        m_frame, std::sqrt(distSq));
                }
            }
        }
    }
    ++m_frame;
}

}
