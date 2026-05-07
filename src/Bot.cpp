#include "Bot.hpp"
#include "BotViz.hpp"

#include <cmath>

namespace bot {

namespace {
// Log when the real player's post-tick position differs from the simulator's
// predicted post-tick position by more than this many world units. Squared so
// we can compare without sqrt.
constexpr float kDivergenceLogDist    = 0.5f;
constexpr float kDivergenceLogDistSq  = kDivergenceLogDist * kDivergenceLogDist;
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
            // Rate-limit at 240Hz: a level with active drift would log every
            // tick and tank performance through fmt formatting alone. One
            // log per ~60 ticks (4×/sec) is plenty to characterize divergence
            // patterns without dominating the frame budget.
            if (distSq > kDivergenceLogDistSq && (m_frame - m_lastDivergenceLogFrame) >= 60) {
                m_lastDivergenceLogFrame = m_frame;
                auto* p = m_pl->m_player1;
                bool const want = inputForCurrentFrame();
                bool const held = p->buttonDown(PlayerButton::Jump);
                geode::log::warn(
                    "[divergence] tick={} pred=({:.2f},{:.2f}) actual=({:.2f},{:.2f}) "
                    "delta=({:+.3f},{:+.3f}) dist={:.3f} "
                    "onGnd={}/{}/{}/{} yVel={:.3f} gravity={:.2f} speed={:.2f} "
                    "jumpBuf={} touchedGravPortal={} btnWant={} btnHeld={}",
                    m_frame, pred.x, pred.y, actual.x, actual.y,
                    dx, dy, std::sqrt(distSq),
                    p->m_isOnGround, p->m_isOnGround2, p->m_isOnGround3, p->m_isOnGround4,
                    p->m_yVelocity, p->m_gravityMod, p->m_playerSpeed,
                    p->m_jumpBuffered, p->m_touchedGravityPortal,
                    want, held);
            }
        }
    }
    ++m_frame;
}

}
