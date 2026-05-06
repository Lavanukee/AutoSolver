#pragma once

#include <Geode/Geode.hpp>
#include <vector>
#include <cstdint>

namespace bot {

using Plan = std::vector<bool>;

enum class DebugViz { Off, Dots, Lines };

// The bot's currently committed plan. Inputs are keyed by ABSOLUTE physics
// frame number — frame 0 is the first physics tick after a level start.
//
// Memory slots the user asked to be intentionally allocated:
//   - lastSurvivingFrame  — best distance of best path (in absolute frames)
//   - frameFound          — frame at which this best path was committed
//   - plan                — queued inputs for the best path
struct BestPath {
    Plan                              plan;
    int64_t                           planStart           = 0;
    int64_t                           lastSurvivingFrame  = -1;
    int64_t                           frameFound          = -1;
    cocos2d::CCPoint                  endPos              {};
    bool                              died                = false;
    // P1 per-frame positions; samples2 populated only in dual mode.
    std::vector<cocos2d::CCPoint>     samples;
    std::vector<cocos2d::CCPoint>     samples2;

    bool empty() const { return plan.empty(); }
    bool covers(int64_t absFrame) const {
        return !plan.empty()
            && planStart <= absFrame
            && absFrame < planStart + static_cast<int64_t>(plan.size());
    }
};

// One path's trajectory captured this visual frame. BotViz draws all of
// these (faint, in their author-chosen colors) before drawing the orange
// best-path overlay on top. Refreshed every search tick.
struct TraceViz {
    // samples = P1 path; samples2 = P2 path (empty in solo mode). Both are
    // already trimmed to the plan's effective survival (min of the two
    // players' lifetimes), so the visual ends where the plan stops being
    // viable for either player.
    std::vector<cocos2d::CCPoint> samples;
    std::vector<cocos2d::CCPoint> samples2;
    cocos2d::ccColor3B            color { 160, 160, 160 };
};

class Bot {
public:
    static Bot& get();

    // Lifecycle
    void onPlayLayerInit(PlayLayer* pl);
    void onPlayLayerReset();
    void onPlayLayerQuit();

    PlayLayer* playLayer() const { return m_pl; }

    // Settings-driven
    void setEnabled(bool e);
    bool enabled() const { return m_enabled; }
    void setDebugViz(DebugViz v);
    DebugViz debugViz() const { return m_viz; }

    // Per-transition input offsets, in physics frames. Negative = inject
    // sooner than the simulated plan dictates (compensates for the engine
    // reading button state on the tick AFTER it's set). Applied lazily in
    // inputForCurrentFrame() so the cached plan + samples stay in sync with
    // what the simulator predicted.
    void setHoldOffset(int v)    { m_holdOffset    = v; }
    void setReleaseOffset(int v) { m_releaseOffset = v; }
    int  holdOffset()    const   { return m_holdOffset; }
    int  releaseOffset() const   { return m_releaseOffset; }

    void  setDivergenceThreshold(float v) { m_divergenceThreshold = v < 0.f ? 0.f : v; }
    float divergenceThreshold() const     { return m_divergenceThreshold; }

    // Frame counter (incremented once per physics tick by the input hook)
    int64_t currentFrame() const { return m_frame; }
    void advanceFrame() { ++m_frame; }

    // Input injection state — flipped by the bot when it calls handleButton
    // so the suppression hook lets the synthesized event through.
    bool isInjecting() const { return m_injecting; }
    void setInjecting(bool v) { m_injecting = v; }
    bool lastHeld() const { return m_lastHeld; }
    void setLastHeld(bool v) { m_lastHeld = v; }

    // Read the input the cached plan has queued for the current absolute frame.
    bool inputForCurrentFrame() const;

    BestPath const& bestPath() const { return m_best; }

    // Refreshed each search tick — one entry per candidate plan, holding the
    // sim's per-frame world positions and the path's author-chosen color so
    // BotViz can draw all candidates at once (not just the winner).
    std::vector<TraceViz> const& candidateTraces() const { return m_candidateTraces; }
    void setCandidateTraces(std::vector<TraceViz>&& v) { m_candidateTraces = std::move(v); }

    // Per-visual-frame entry: runs candidate generation, scores, picks best.
    // Implementation lives in BotSearch.cpp.
    void runSearch();

    // Replace the cached best path. Called from search code.
    void commitBest(BestPath&& nb);

    // Flipped true at the end of our setupHasCompleted hook — i.e. AFTER the
    // engine's own setup-tick updateCamera fires. Gates runSearch so the
    // first search tick on a level only runs once the level is fully wired.
    void setLevelReady(bool v) { m_levelReady = v; }
    bool levelReady() const    { return m_levelReady; }

private:
    Bot() = default;
    Bot(const Bot&) = delete;
    Bot& operator=(const Bot&) = delete;

    PlayLayer* m_pl       = nullptr;
    bool       m_enabled  = false;
    DebugViz   m_viz      = DebugViz::Off;

    int64_t    m_frame     = 0;
    bool       m_injecting = false;
    bool       m_lastHeld  = false;

    int        m_holdOffset    = 0;
    int        m_releaseOffset = 0;
    float      m_divergenceThreshold = 1.f;

    BestPath              m_best;
    std::vector<TraceViz> m_candidateTraces;

    // Re-entry sentinel for runSearch. The canonical reentrancy loop (search
    // → runPlan → checkCollisions → engine updateCamera → search) is closed
    // by m_simulating gating runPlan, but a stack-overflow crash log from
    // 2026-05-03 still showed ~2780 frames deep on level entry, so we add a
    // hard guard at runSearch's entry too — independent of m_simulating —
    // to make the failure mode "skip this frame's search" instead of crash.
    bool m_inSearch    = false;
    bool m_levelReady  = false;
};

}
