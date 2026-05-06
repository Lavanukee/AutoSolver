#include "Bot.hpp"
#include "BotViz.hpp"
#include "PathBuilder.hpp"
#include "Trajectory.hpp"

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

    auto& trajSim = traj::TrajectorySimulator::get();
    auto* base    = m_pl->m_player1;
    auto* base2   = (m_pl->m_gameState.m_isDualMode && m_pl->m_player2)
                        ? m_pl->m_player2 : nullptr;
    int64_t F     = m_frame;

    // Candidates this frame come from the user-authored Path Builder paths,
    // filtered to ones whose gamemode mask matches the current player vehicle.
    uint8_t gmBit = currentGamemodeBit(base);
    std::vector<Candidate> candidates = PathBuilder::get().generateCandidates(HORIZON, gmBit);
    if (candidates.empty()) {
        m_candidateTraces.clear();
        BotViz::get().clear();
        return;
    }

    BestPath winner;
    Score    winnerScore { INT_MIN, -FLT_MAX, true };
    bool     haveWinner = false;

    std::vector<TraceViz> traces;
    traces.reserve(candidates.size());

    for (auto& c : candidates) {
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
            haveWinner   = true;
            winnerScore  = s;

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
