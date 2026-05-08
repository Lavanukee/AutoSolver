#pragma once

#include <Geode/Geode.hpp>
#include <unordered_set>
#include <vector>

namespace traj {

// Result of running a discrete input plan on the sim player. Plan-driven path
// testing is what the bot search uses; trajectory visualization uses runBranch
// which holds/releases for the entire horizon.
struct PlanResult {
    // positions[0] is the start position; positions[i+1] is the sim's position
    // AFTER frame i's input was applied. size() == framesSurvived + 1.
    // positions2 is populated only in dual mode; framesSurvived is then the
    // MIN of P1 and P2 survival (the plan is "alive" only while BOTH are).
    std::vector<cocos2d::CCPoint> positions;
    std::vector<cocos2d::CCPoint> positions2;
    int  framesSurvived = 0;
    bool died = false;
};

class TrajectorySimulator {
public:
    static TrajectorySimulator& get();

    void onPlayLayerInit(PlayLayer* pl);
    void onPlayLayerReset();
    void onPlayLayerQuit();

    void setShowTrajectory(bool v);
    void setIterations(int v);
    void setPadsEnabled(bool v);
    void setOrbsEnabled(bool v);
    void setPortalsEnabled(bool v);

    bool wantsPads()    const { return m_pads;    }
    bool wantsOrbs()    const { return m_orbs;    }
    bool wantsPortals() const { return m_portals; }
    bool isShowing()    const { return m_show;    }

    void simulate();

    // Bot path testing: simulate one specific plan starting from `base1` (and
    // `base2` in dual mode — pass nullptr for solo). Both players step in
    // lockstep; the run stops at the first frame EITHER player dies, and
    // PlanResult::framesSurvived is the count of fully-survived frames (so a
    // plan that keeps both alive longer scores higher).
    //
    // `plan2` is the independent input sequence for P2 in 2P-mode levels; pass
    // an empty vector to make P2 mirror P1 (the correct behavior in regular
    // dual where P1 and P2 face the same obstacles).
    PlanResult runPlan(PlayerObject* base1, PlayerObject* base2,
                       std::vector<bool> const& plan,
                       std::vector<bool> const& plan2 = {});

    bool isSimulating() const { return m_simulating; }
    bool isSimPlayer(PlayerObject* p) const { return p && (p == m_simP1 || p == m_simP2); }
    bool markSimDeadIfSimPlayer(PlayerObject* p);
    bool isSimDead(PlayerObject* p) const;
    void clearSimDead(PlayerObject* p);
    void recordRealButton(bool down, bool isP1);
    void setFrameDelta(float dt);
    PlayLayer* playLayer() const { return m_pl; }

    // Sim-side activation tracking. activatedByPlayer is no-op'd for sim
    // (so the engine's per-object flags stay clean for the real player), which
    // means hasBeenActivatedByPlayer would otherwise return false forever and
    // the engine would re-fire orbs every iteration. We mirror activation in
    // a sim-local set, cleared per branch.
    void markActivated(EnhancedGameObject* obj);
    bool hasBeenActivated(EnhancedGameObject* obj) const;

    // Sim-local "destroyed" tracking. The engine's destroyObject mutates the
    // real level (removes from active arrays, hides the sprite, fires spawn
    // triggers) — none of which we can cheaply roll back. So during sim we
    // intercept destroyObject and only mark the object as destroyed-for-sim
    // here; the collisionCheckObjects filter then drops any sim-destroyed
    // object from the candidate set so the sim's physics treats the block as
    // gone (lets the sim "break through" it). The set persists across ticks
    // within a single runPlan/runBranch (so the sim doesn't re-collide with
    // a block it already destroyed), and is cleared at the start of each
    // runPlan/runBranch so subsequent searches start from a clean slate.
    // Real level state never changes.
    void markSimDestroyed(GameObject* obj);
    bool isSimDestroyed(GameObject* obj) const;
    void clearSimDestroyed();

    // Flipped true at the end of our setupHasCompleted hook (after the engine's
    // own setup-tick updateCamera has fired). Gates simulate() so the very
    // first updateCamera doesn't run sim against half-wired engine state.
    void setLevelReady(bool v) { m_levelReady = v; }
    bool levelReady() const    { return m_levelReady; }

private:
    TrajectorySimulator() = default;
    TrajectorySimulator(const TrajectorySimulator&) = delete;
    TrajectorySimulator& operator=(const TrajectorySimulator&) = delete;

    PlayerObject* createSimPlayer(PlayLayer* pl);
    void simulateForPlayer(PlayerObject* simHold, PlayerObject* simRelease,
                           PlayerObject* base, bool isPlayer2);
    void runBranch(PlayerObject* sim, PlayerObject* base, bool holdAtStart, bool isPlayer2);
    void drawHitboxAtEnd(PlayerObject* sim, bool holdBranch);
    cocos2d::CCDrawNode* ensureDrawNode();
    // Take or allocate a private CCArray for the sim's m_touchingRings, retain
    // it, and store a back-reference so clearSimRingState can restore the sim
    // pointer after each copyAttributes call.
    void adoptOwnRings(PlayerObject* sim, cocos2d::CCArray*& slot);

    // copyAttributes carries the real player's transient orb/ring overlap state
    // onto the sim, which makes the sim believe it's already touching orbs the
    // real player is near. Reset those fields to give each branch a clean slate.
    void clearSimRingState(PlayerObject* sim);

    // Per-tick "current overlap" reset. Called BEFORE checkCollisions inside
    // every sim step so the engine's per-tick ring-overlap detection starts
    // from an empty array; any orb returned by collision is added freshly,
    // and stale orbs from prior ticks (where the sim was overlapping but no
    // longer is) are dropped. Without this, m_touchingRings accumulates
    // across ticks and sim's ringJump fires on orbs the sim has already
    // physically left — the "ghost orb activation" symptom.
    //
    // Narrower than clearSimRingState: leaves dash/ring-jump-state alone so
    // mid-flight dash physics stays continuous between ticks.
    void clearPerTickRingOverlap(PlayerObject* sim);

    PlayLayer*           m_pl{nullptr};
    PlayerObject*        m_simP1{nullptr};
    PlayerObject*        m_simP2{nullptr};
    cocos2d::CCDrawNode* m_drawNode{nullptr};

    // m_touchingRings on PlayerObject is a CCArray* — a pointer. The
    // PlayerObject constructor allocates one, but copyAttributes(base)
    // shallow-copies the field, so post-copy the sim and the real player
    // point to the SAME CCArray. clearSimRingState's removeAllObjects then
    // empties the array shared with the real player, and the engine's
    // collision pass for the sim refills it with sim-position overlaps —
    // leaking sim ring state into the real game. We hold each sim's
    // original array here (retained) and restore the pointer after every
    // copyAttributes so the sim only ever clobbers its own private array.
    cocos2d::CCArray*    m_simP1OwnRings{nullptr};
    cocos2d::CCArray*    m_simP2OwnRings{nullptr};

    bool m_show{false};
    int  m_iterations{300};
    bool m_pads{true};
    bool m_orbs{true};
    bool m_portals{true};

    bool  m_simulating{false};
    // simulate()-only re-entry sentinel — distinct from m_simulating because
    // m_simulating is also true during runPlan (which runs from BotSearch,
    // not from simulate). Mixing the two would let simulate() and runPlan()
    // no-op each other.
    bool  m_inSimulate{false};
    bool  m_levelReady{false};
    // Per-sim-player death flags. The engine cascades destroyPlayer to both
    // sim players in dual mode (one death kills both), so a single shared
    // flag would terminate the surviving player's branch with no obstacle
    // of its own. Tracking each independently lets each branch run until
    // its OWN sim player is the one being destroyed.
    bool  m_simP1Dead{false};
    bool  m_simP2Dead{false};
    bool  m_player1Pressed{false};
    bool  m_player2Pressed{false};
    float m_frameDt{1.f / 240.f};

    std::unordered_set<EnhancedGameObject*> m_activated;
    std::unordered_set<GameObject*>         m_simDestroyed;

    // Rate-limit budget for [orb-far-activate] log. Decremented on each
    // qualifying event (sim activates an orb >30 units away); when ≤0 we
    // log and reset. Without this the candidate × frame × orb fan-out from
    // a level full of false-hits can drown the log and tank perf.
    int m_orbFarLogBudget = 0;
};

}
