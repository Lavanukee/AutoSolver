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
    // Independent P2 plan for 2P-mode levels (LevelSettings::m_twoPlayerMode).
    // Empty in solo levels and in single-player-with-dual-portal levels — in
    // those cases P2 mirrors P1's plan, which is the correct behavior because
    // either player's death kills both, and the obstacles per side are the
    // same. In 2P-mode the level designer authored independent obstacles per
    // player, so P1 and P2 each need their own input sequence.
    Plan                              plan2;
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

    // Master gate for all bot visualization (candidate traces + best-path
    // overlay + dot/line transition markers). Independent from `enabled` so
    // the user can run the bot headlessly. Toggleable via X keybind and via
    // the "show-bot-trajectory" mod setting.
    void setShowBotTraj(bool v);
    bool showBotTraj() const { return m_showBotTraj; }

    void  setDivergenceThreshold(float v) { m_divergenceThreshold = v < 0.f ? 0.f : v; }
    float divergenceThreshold() const     { return m_divergenceThreshold; }

    // Search runs from updateCamera at the visual-frame rate (60 Hz). Setting
    // this to N>1 means the bot only searches every N visual frames; the
    // cached plan keeps executing on the off-frames. Input injection is
    // unaffected — that still runs at 240 Hz from PlayerObject::update.
    void setSearchInterval(int v) { m_searchInterval = v < 1 ? 1 : v; }
    int  searchInterval() const   { return m_searchInterval; }
    // Returns true if the current visual frame is a search frame; the hook
    // increments m_visualFrame each call regardless.
    bool stepVisualFrameAndShouldSearch();

    // Frame counter (incremented once per physics tick by the input hook)
    int64_t currentFrame() const { return m_frame; }
    // Implementation lives in Bot.cpp — runs the divergence detector before
    // bumping the counter (so m_frame still names the tick that just ran).
    void advanceFrame();

    // Input injection state — flipped by the bot when it calls handleButton
    // so the suppression hook lets the synthesized event through.
    bool isInjecting() const { return m_injecting; }
    void setInjecting(bool v) { m_injecting = v; }
    bool lastHeld() const { return m_lastHeld; }
    void setLastHeld(bool v) { m_lastHeld = v; }

    // Read the input the cached plan has queued for the current absolute frame.
    // P1 reads from plan; P2 reads from plan2 if non-empty (2P-mode), else
    // mirrors P1 via m_lastHeld. The hook calls these PRE-super so the input
    // lands the same tick the plan asserts.
    bool inputForCurrentFrame() const;
    bool inputForCurrentFrameP2() const;

    // Plan-switch divergence guard: returns true if a planned input flip THIS
    // tick should be discarded because the cached plan's predicted position
    // for THIS frame has diverged from the player's actual position by more
    // than the user-set threshold. Logs the discard. The next search tick
    // (60Hz) will rebase the plan from current real state, at which point
    // flips can resume.
    bool shouldDiscardP1Flip(cocos2d::CCPoint actualPos) const;
    bool shouldDiscardP2Flip(cocos2d::CCPoint actualPos) const;

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

    PlayLayer*   m_pl           = nullptr;
    bool         m_enabled      = false;
    DebugViz     m_viz          = DebugViz::Off;
    bool         m_showBotTraj  = true;

    int64_t    m_frame     = 0;
    bool       m_injecting = false;
    bool       m_lastHeld  = false;

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

    // Visual-frame counter (60 Hz, distinct from m_frame which is the 240 Hz
    // physics frame). Used only by the search-interval gate.
    int64_t m_visualFrame    = 0;
    int     m_searchInterval = 1;

    // Rate-limit token for [divergence] logs in advanceFrame. Without it,
    // any sustained drift would log every 240Hz tick and the fmt formatting
    // alone would tank the frame budget. Records the last m_frame at which
    // we logged; we re-log only after 60 ticks have passed (≤4 logs/sec).
    int64_t m_lastDivergenceLogFrame = -1000;
};

}
