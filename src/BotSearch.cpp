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
            winner = std::move(bp);
        }
    }

    // Stage 2: 2P-mode independent P2 search. Mirror's joint survival is
    // capped by whichever player dies first under the same input. If P2 was
    // the limiter (different obstacles per side, common in 2P-mode levels),
    // an independent plan2 can keep P2 alive longer — and joint survival
    // grows because P1 is no longer constrained to "die together with P2".
    // Algorithm: P1 fixed to mirror's full winning plan, search every
    // candidate as plan2. Replace winner only on STRICT improvement (a tied
    // plan2 isn't worth the search churn).
    bool const isTwoPlayerLevel = m_pl->m_levelSettings
                               && m_pl->m_levelSettings->m_twoPlayerMode;
    if (isTwoPlayerLevel && haveWinner && base2) {
        // candidatesP2 may differ from candidates if P1/P2 have different
        // gamemodes (e.g. one cube + one ship). Reuse if same gamemode.
        uint8_t const gmBitP2 = currentGamemodeBit(base2);
        std::vector<Candidate> const& candidatesP2 =
            (gmBitP2 == gmBitP1)
                ? candidates
                : PathBuilder::get().generateCandidates(HORIZON, gmBitP2);

        // Use the FULL winning P1 plan (not winner.plan, which was trimmed to
        // mirror's framesSurvived — stage 2 needs all HORIZON entries to have
        // a chance at extending joint survival beyond mirror's death frame).
        Plan const& p1Full = candidates[winnerIdx].plan;
        int    bestSurvival = winnerScore.framesSurvived;
        size_t bestP2Idx    = static_cast<size_t>(-1);
        traj::PlanResult bestRes;

        for (size_t i = 0; i < candidatesP2.size(); ++i) {
            traj::PlanResult res = trajSim.runPlan(base, base2, p1Full, candidatesP2[i].plan);
            if (res.framesSurvived > bestSurvival) {
                bestSurvival = res.framesSurvived;
                bestP2Idx    = i;
                bestRes      = res;
            }
        }

        if (bestP2Idx != static_cast<size_t>(-1)) {
            Plan const& p2Full = candidatesP2[bestP2Idx].plan;
            int keep1 = std::min<int>(static_cast<int>(p1Full.size()), bestSurvival);
            int keep2 = std::min<int>(static_cast<int>(p2Full.size()), bestSurvival);
            winner.plan.assign(p1Full.begin(), p1Full.begin() + keep1);
            winner.plan2.assign(p2Full.begin(), p2Full.begin() + keep2);
            winner.lastSurvivingFrame = F + bestSurvival;
            winner.died               = bestRes.died;
            winner.endPos             = bestRes.positions.empty()
                                            ? base->getPosition()
                                            : bestRes.positions.back();
            winner.samples            = std::move(bestRes.positions);
            winner.samples2           = std::move(bestRes.positions2);
            winnerScore.framesSurvived = bestSurvival;
            winnerScore.endX           = winner.samples.empty() ? 0.f : winner.samples.back().x;
            winnerScore.died           = bestRes.died;
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
