#include "Bot.hpp"
#include "Trajectory.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

namespace {
inline bot::Bot& bot_() { return bot::Bot::get(); }
inline traj::TrajectorySimulator& sim() { return traj::TrajectorySimulator::get(); }
}

class $modify(BotPlayLayerHook, PlayLayer) {
    // setupHasCompleted runs from inside PlayLayer::init (via
    // processCreateObjectsFromSetup) and itself triggers the first
    // updateCamera tick. Initializing here ensures Bot state is fresh
    // BEFORE the engine fires its setup-tick updateCamera, so runSearch
    // doesn't dereference a freed prior PlayLayer (macOS heap-reuse can
    // land the new PL at the same address, defeating pointer-equality).
    void setupHasCompleted() {
        bot_().onPlayLayerInit(this);
        PlayLayer::setupHasCompleted();
        // Flip levelReady AFTER super so the engine's setup-tick updateCamera
        // (which fires from inside processCreateObjectsFromSetup, before super
        // returns) sees levelReady == false and skips runSearch.
        bot_().setLevelReady(true);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        bot_().onPlayLayerReset();
    }

    void onQuit() {
        bot_().onPlayLayerQuit();
        PlayLayer::onQuit();
    }
};

class $modify(BotBGLHook, GJBaseGameLayer) {
    // Run candidate search per visual frame. Trajectory viz also hooks
    // updateCamera in Hooks.cpp; Geode chains them so both run.
    //
    // Gate: this hook fires for every GJBaseGameLayer subclass — including
    // LevelEditorLayer (which does NOT have a PlayerObject we can simulate)
    // and brand-new PlayLayer instances calling updateCamera from their own
    // setupHasCompleted before our PlayLayer::init hook has run
    // onPlayLayerInit. In both cases the bot's cached m_pl is stale or
    // points to a freed PlayLayer, and dereferencing it crashes. Run the
    // search ONLY when `this` is the same layer we registered.
    void updateCamera(float dt) {
        auto& b = bot_();
        // Reentrancy guard: runSearch → runPlan → checkCollisions →
        // collisionCheckObjects can trigger an inner updateCamera (engine
        // path through trigger/camera-effect objects activated during sim).
        // Without this, runSearch re-enters itself unboundedly and overflows
        // the stack. isSimulating() is true exactly inside runPlan/runBranch.
        if (!sim().isSimulating()
            && static_cast<GJBaseGameLayer*>(b.playLayer()) == this) {
            if (b.stepVisualFrameAndShouldSearch()) {
                b.runSearch();
            }
        }
        GJBaseGameLayer::updateCamera(dt);
    }

    // Suppress real keyboard/mouse jump events while the bot is enabled,
    // unless the bot itself synthesized this event (m_injecting = true) or
    // a sim is running (which has its own button injection). Same gate as
    // updateCamera — never swallow inputs in the editor or stale layers.
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& b = bot_();
        if (static_cast<GJBaseGameLayer*>(b.playLayer()) == this
            && b.enabled() && !b.isInjecting() && !sim().isSimulating()
            && button == 1) {
            return;
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }
};

// Per-physics-tick (240Hz) injection point.
//
// Inject BEFORE super and bypass the engine's button queue by calling
// pushButton/releaseButton directly on the player — same mechanism the
// simulator uses on its sim PlayerObject. handleButton routes through
// GJBaseGameLayer::m_queuedButtons, which the engine drains at the START
// of each tick (before checkCollisions); a handleButton call from inside
// the tick (whether pre- or post-super of PlayerObject::update) misses
// that drain and lands one tick late. pushButton/releaseButton mutate the
// player's state directly, so the button is in effect for the same tick's
// checkCollisions+update — matching the simulator's runPlan ordering.
//
// m_frame at entry equals T (advanceFrame at the end of T-1 brought it
// from T-1 to T), so plan[0] applies to tick planStart in reality just as
// it does in simulator iter 0.
class $modify(BotPlayerObjectHook, PlayerObject) {
    void update(float dt) {
        auto& b = bot_();
        auto* pl = b.playLayer();
        bool const isP1 = pl && (this == pl->m_player1);
        bool const isP2 = pl && (this == pl->m_player2);
        bool const live = pl && !sim().isSimulating() && b.levelReady() && b.enabled();

        if (live && isP1) {
            bool const want = b.inputForCurrentFrame();
            if (want != b.lastHeld()) {
                // Plan-switch divergence guard: if reality has drifted from
                // the cached plan's predicted position for THIS tick, the
                // flip-decision is rooted in stale state — drop it and let
                // the next 60Hz search rebase the plan from current reality.
                if (b.shouldDiscardP1Flip(this->getPosition())) {
                    // Hold lastHeld; the guard already logged.
                } else {
                    if (want) this->pushButton(PlayerButton::Jump);
                    else      this->releaseButton(PlayerButton::Jump);
                    b.setLastHeld(want);
                }
            }
        } else if (live && isP2 && pl->m_gameState.m_isDualMode) {
            // P2 input runs in P2's OWN hook (PRE-super) so pushButton lands
            // immediately before P2's collision+update pass — surviving
            // anything the engine might touch between P1's super and P2's
            // super (queue draining, per-player state resets).
            //
            // inputForCurrentFrameP2 returns plan2[idx] in 2P-mode (where the
            // search produces independent P1/P2 plans), or m_lastHeld in
            // regular dual (where P2 mirrors P1). Use it directly rather than
            // reading lastHeld here so 2P-mode levels get their independent
            // P2 input. m_frame has advanced past P1's tick by now (advanceFrame
            // fires at end of P1's super), so we explicitly index off P1's
            // last-applied tick: lastHeld is canonical for the mirror case,
            // and plan2 is indexed by the same m_frame as plan but the lookup
            // is done via inputForCurrentFrameP2 which uses the same idx math.
            // Net effect: regular dual mirrors via lastHeld (unchanged), 2P
            // mode reads plan2[m_frame - planStart].
            bool const want = b.inputForCurrentFrameP2();
            if (this->buttonDown(PlayerButton::Jump) != want) {
                if (b.shouldDiscardP2Flip(this->getPosition())) {
                    // Drop the flip; only meaningful in 2P-mode where P2 has
                    // its own plan2 + samples2 to diverge against.
                } else {
                    if (want) this->pushButton(PlayerButton::Jump);
                    else      this->releaseButton(PlayerButton::Jump);
                }
            }
        }

        PlayerObject::update(dt);

        if (isP1 && live) b.advanceFrame();
    }
};
