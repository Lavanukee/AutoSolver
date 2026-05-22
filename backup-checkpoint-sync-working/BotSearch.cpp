#include "Bot.hpp"
#include "BotViz.hpp"
#include "PathBuilder.hpp"
#include "Trajectory.hpp"
#include "ShadowLayer.hpp"

#include <algorithm>
#include <climits>

using namespace geode::prelude;

namespace bot {

namespace {

// How many physics frames each candidate plan looks ahead per visual frame.
// Larger = more lookahead but more sim cost per candidate.
constexpr int HORIZON = 360;

struct Score {
    int   framesSurvived;
    float endX;
    bool died;
};

// Higher = better. Frames-survived dominates; endX is a tiebreaker for
// candidates that all reach the horizon.
bool isBetter(Score const& a, Score const& b) {
    if (a.framesSurvived != b.framesSurvived) return a.framesSurvived > b.framesSurvived;
    return a.endX > b.endX;
}

}

void Bot::runSearch() {
    if (!m_enabled || !m_pl || !m_pl->m_player1) {
        BotViz::get().clear();
        return;
    }
    // Don't run a search until the level has finished setting up. The very
    // first updateCamera tick fires from inside PlayLayer::setupHasCompleted
    // super, before engine state is fully wired — running search there has
    // historically blown the stack via re-entrant updateCamera paths.
    if (!m_levelReady) return;
    // Re-entry sentinel: independent of TrajectorySimulator::m_simulating.
    // If anything reaches Bot::runSearch while a search is already in flight,
    // bail rather than recursing. Scope guard ensures we always clear.
    if (m_inSearch) return;
    m_inSearch = true;
    struct ScopeReset { bool& f; ~ScopeReset() { f = false; } } _sr{ m_inSearch };

    // Shadow PlayLayer construction disabled — object-instantiation phase
    // mutates shared state that breaks the real PlayLayer's behavior (bot
    // regresses from x=3968 to x=326). Investigation parked; the simpler
    // PlayerCheckpoint-based state sync in TrajectorySimulator handles the
    // same divergence class without a second PlayLayer.
    // ShadowLayer::get().ensureCreated(m_pl);

    auto& trajSim = traj::TrajectorySimulator::get();
    auto* base    = m_pl->m_player1;
    auto* base2   = (m_pl->m_gameState.m_isDualMode && m_pl->m_player2)
                        ? m_pl->m_player2 : nullptr;
    int64_t F     = m_frame;

    // Candidates this frame come from the user-authored Path Builder paths,
    // filtered to ones whose gamemode mask matches the current player vehicle.
    // In 2P-mode dual, P1 and P2 can have different vehicles (one cube, one
    // ship), so P2 needs candidates filtered to ITS gamemode.
    uint8_t const gmBitP1 = currentGamemodeBit(base);
    std::vector<Candidate> candidates = PathBuilder::get().generateCandidates(HORIZON, gmBitP1);
    if (candidates.empty()) {
        m_candidateTraces.clear();
        BotViz::get().clear();
        return;
    }

    BestPath winner;
    Score    winnerScore { INT_MIN, -FLT_MAX, true };
    bool     haveWinner = false;
    size_t   winnerIdx  = 0;

    std::vector<TraceViz> traces;
    traces.reserve(candidates.size());

    // 2P-mode detection. Two flags can each independently mark a level as
    // 2P-mode (split-screen, independent inputs per player). Check both.
    bool const isTwoPlayerLevel =
           (m_pl->m_levelSettings && m_pl->m_levelSettings->m_twoPlayerMode)
        || (m_pl->m_level         && m_pl->m_level->m_twoPlayerMode);

    // 2P-mode fast path: fully INDEPENDENT P1 and P2 searches. Each player
    // is simulated alone (runPlan with base2=nullptr means only the first
    // base is simulated, and the per-tick check + sim death apply only to
    // that one player), so each gets its globally-best plan ignoring the
    // other's deaths. Joint survival = min(P1's framesSurvived, P2's). The
    // bot's input injector reads plan / plan2 independently in 2P-mode (see
    // BotPlayerObjectHook), so this matches reality.
    //
    // Skips the mirror search entirely — in 2P-mode P1 and P2 face different
    // obstacles, so the locally-best mirror plan is rarely a starting point
    // for joint survival, and the coordinate-descent iteration was getting
    // stuck on it. Mirror search still runs in non-2P (regular dual / solo)
    // levels below.
    if (isTwoPlayerLevel && base2) {
        uint8_t const gmBitP2 = currentGamemodeBit(base2);
        std::vector<Candidate> candidatesP2_owned;
        std::vector<Candidate> const& candidatesP2 =
            (gmBitP2 == gmBitP1)
                ? candidates
                : (candidatesP2_owned = PathBuilder::get().generateCandidates(HORIZON, gmBitP2),
                   candidatesP2_owned);

        bool   haveP1 = false;
        Score  scoreP1 { INT_MIN, -FLT_MAX, true };
        size_t idxP1  = 0;
        traj::PlanResult resP1;
        for (size_t i = 0; i < candidates.size(); ++i) {
            traj::PlanResult res = trajSim.runPlan(base, /*base2=*/nullptr, candidates[i].plan);
            Score s { res.framesSurvived,
                      res.positions.empty() ? 0.f : res.positions.back().x,
                      res.died };
            // Candidate trace for visualization (P1's path on left lane).
            TraceViz tv; tv.samples = res.positions; tv.color = candidates[i].color;
            traces.push_back(std::move(tv));
            if (!haveP1 || isBetter(s, scoreP1)) {
                haveP1 = true; scoreP1 = s; idxP1 = i;
                resP1  = std::move(res);
            }
        }

        bool   haveP2 = false;
        Score  scoreP2 { INT_MIN, -FLT_MAX, true };
        size_t idxP2  = 0;
        traj::PlanResult resP2;
        for (size_t i = 0; i < candidatesP2.size(); ++i) {
            traj::PlanResult res = trajSim.runPlan(base2, /*base2=*/nullptr, candidatesP2[i].plan);
            Score s { res.framesSurvived,
                      res.positions.empty() ? 0.f : res.positions.back().x,
                      res.died };
            TraceViz tv; tv.samples2 = res.positions; tv.color = candidatesP2[i].color;
            traces.push_back(std::move(tv));
            if (!haveP2 || isBetter(s, scoreP2)) {
                haveP2 = true; scoreP2 = s; idxP2 = i;
                resP2  = std::move(res);
            }
        }

        if (haveP1 && haveP2) {
            Plan const& p1Plan = candidates[idxP1].plan;
            Plan const& p2Plan = candidatesP2[idxP2].plan;
            int const joint = std::min(scoreP1.framesSurvived, scoreP2.framesSurvived);
            int const keep1 = std::min<int>(static_cast<int>(p1Plan.size()), scoreP1.framesSurvived);
            int const keep2 = std::min<int>(static_cast<int>(p2Plan.size()), scoreP2.framesSurvived);

            BestPath bp;
            bp.plan.assign(p1Plan.begin(), p1Plan.begin() + keep1);
            bp.plan2.assign(p2Plan.begin(), p2Plan.begin() + keep2);
            bp.planStart          = F;
            bp.lastSurvivingFrame = F + joint;
            bp.frameFound         = F;
            bp.endPos             = resP1.positions.empty() ? base->getPosition()
                                                            : resP1.positions.back();
            bp.died               = resP1.died || resP2.died;
            // samples = P1's path from P1-alone sim; samples2 = P2's path
            // from P2-alone sim (which is in resP2.positions, not positions2,
            // because base2 was nullptr during that runPlan call).
            bp.samples            = std::move(resP1.positions);
            bp.samples2           = std::move(resP2.positions);
            bp.samplesYVel        = std::move(resP1.yVels);
            bp.samples2YVel       = std::move(resP2.yVels);
            winner = std::move(bp);
            haveWinner = true;
            winnerScore = Score { joint,
                                  winner.samples.empty() ? 0.f : winner.samples.back().x,
                                  bp.died };
            winnerIdx = idxP1;
        }

        // Skip mirror Stage 1 + coordinate-descent stages 2-4 below — the
        // independent searches above produced the committed plan already.
        setCandidateTraces(std::move(traces));
        bool diverged = false;
        if (haveWinner && !m_best.empty()) {
            float thr = m_divergenceThreshold;
            int64_t idx = F - m_best.planStart;
            auto checkDrift = [&](std::vector<cocos2d::CCPoint> const& samples,
                                  cocos2d::CCPoint cur) {
                if (idx < 0 || idx >= static_cast<int64_t>(samples.size())) return;
                auto exp = samples[static_cast<size_t>(idx)];
                float dx = exp.x - cur.x;
                float dy = exp.y - cur.y;
                if (dx * dx + dy * dy > thr * thr) diverged = true;
            };
            checkDrift(m_best.samples, base->getPosition());
            if (!diverged && base2 && !m_best.samples2.empty()) {
                checkDrift(m_best.samples2, base2->getPosition());
            }
        }
        if (haveWinner && (m_best.empty() || diverged
                           || winner.lastSurvivingFrame > m_best.lastSurvivingFrame)) {
            commitBest(std::move(winner));
        }
        BotViz::get().render();
        return;
    }

    // Stage 1: mirror search. Same plan applied to both players (correct for
    // solo levels and for regular dual where P1/P2 face the same obstacles).
    // For 2P-mode levels this is the baseline that stage 2 tries to improve on.
    for (size_t i = 0; i < candidates.size(); ++i) {
        auto& c = candidates[i];
        traj::PlanResult res = trajSim.runPlan(base, base2, c.plan);
        Score s {
            res.framesSurvived,
            res.positions.empty() ? 0.f : res.positions.back().x,
            res.died,
        };

        // Capture this candidate's per-frame positions for visualization in
        // its author-chosen color, regardless of whether it wins this frame.
        // In dual mode, both player paths are already trimmed to the plan's
        // effective survival inside runPlan.
        TraceViz tv;
        tv.samples  = res.positions;
        tv.samples2 = res.positions2;
        tv.color    = c.color;
        traces.push_back(std::move(tv));

        if (!haveWinner || isBetter(s, winnerScore)) {
            haveWinner  = true;
            winnerScore = s;
            winnerIdx   = i;

            BestPath bp;
            int keep = std::min<int>(static_cast<int>(c.plan.size()), res.framesSurvived);
            bp.plan.assign(c.plan.begin(), c.plan.begin() + keep);
            bp.planStart          = F;
            bp.lastSurvivingFrame = F + res.framesSurvived;
            bp.frameFound         = F;
            bp.endPos             = res.positions.empty() ? base->getPosition() : res.positions.back();
            bp.died               = res.died;
            bp.samples            = std::move(res.positions);
            bp.samples2           = std::move(res.positions2);
            bp.samplesYVel        = std::move(res.yVels);
            bp.samples2YVel       = std::move(res.yVels2);
            winner = std::move(bp);
        }
    }

    setCandidateTraces(std::move(traces));

    // Replace cache when:
    //   1. there's no current best, OR
    //   2. the new winner outlasts the current best (strictly later last frame), OR
    //   3. the real player has drifted from the current best's predicted path
    //      by more than the user-set threshold — the cached plan was built
    //      from a stale starting state, so committing this frame's winner
    //      gets us back onto a path rooted in current reality.
    bool diverged = false;
    if (haveWinner && !m_best.empty()) {
        float thr = m_divergenceThreshold;
        int64_t idx = F - m_best.planStart;
        auto checkDrift = [&](std::vector<cocos2d::CCPoint> const& samples,
                              cocos2d::CCPoint cur) {
            if (idx < 0 || idx >= static_cast<int64_t>(samples.size())) return;
            auto exp = samples[static_cast<size_t>(idx)];
            float dx = exp.x - cur.x;
            float dy = exp.y - cur.y;
            if (dx * dx + dy * dy > thr * thr) diverged = true;
        };
        checkDrift(m_best.samples, base->getPosition());
        if (!diverged && base2 && !m_best.samples2.empty()) {
            checkDrift(m_best.samples2, base2->getPosition());
        }
    }

    if (haveWinner && (m_best.empty() || diverged
                       || winner.lastSurvivingFrame > m_best.lastSurvivingFrame)) {
        commitBest(std::move(winner));
    }

    BotViz::get().render();
}

}
