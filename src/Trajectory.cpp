#include "Trajectory.hpp"

#include <cmath>

using namespace geode::prelude;

namespace traj {

namespace {

// Captures the slice of GJGameState that triggers / portal handlers / camera
// commands mutate during a sim run, then writes it back at scope exit. Without
// this, two classes of bug bite us:
//
//  1. Real-game leakage. A sim crossing a mode-switch portal flips
//     m_isDualMode globally; a sim crossing a camera-zoom trigger moves the
//     real camera; a sim hitting a flip portal toggles m_levelFlipping. The
//     real player sees gameplay area changes it never crossed.
//
//  2. Trajectory divergence. The bot scores many candidates per visual frame
//     via runPlan; later candidates start from layer state already mutated by
//     earlier ones, so their predictions don't match what a fresh real frame
//     would do. Symptom: orange line shows survival, ground truth shows death.
//
// Snapshot scope is per-runBranch and per-runPlan: each is a cohesive sim run
// where mutations should persist DURING (so a portal crossed at frame 10
// affects frame 11+ of THAT run), but be wiped between runs. m_gameState
// holds nearly all the layer-wide gameplay/camera state we care about; we
// snapshot the trivially-copyable POD subset (skipping containers like
// m_tweenActions which are non-trivial to deep-copy and which trigger gating
// in TrajEffectHook already prevents from being touched during sim).
struct LayerStateSnapshot {
    PlayLayer* pl{nullptr};

    float            cameraZoom, targetCameraZoom;
    cocos2d::CCPoint cameraOffset;
    cocos2d::CCPoint cameraPosition, cameraPosition2;
    float            cameraAngle, targetCameraAngle;
    int              cameraEdge0, cameraEdge1, cameraEdge2, cameraEdge3;
    bool             cameraShakeEnabled;
    float            cameraShakeFactor;
    cocos2d::CCPoint cameraStepDiff;

    bool             isDualMode;
    unsigned int     dualRelated;

    float            levelFlipping;
    bool             gravityRelated;
    float            portalY;
    GameObject*      lastActivatedPortal1;
    GameObject*      lastActivatedPortal2;

    float            timeWarp, queuedTimeWarp, timeWarpRelated;
    int              currentChannel, rotateChannel;

    // m_speedObjects is a CCArray that the engine appends to via
    // addToSpeedObjects when ANY player (sim or real) crosses a speed-mod
    // portal. Once an entry is in this array, the engine consults it on
    // every subsequent tick to compute player speed — meaning if a sim
    // adds an entry, the real player picks up that speed change too.
    // Snapshot the array's pointer-set so we can restore it cleanly.
    // Pointers only — items are owned elsewhere; we're not refcounting.
    std::vector<cocos2d::CCObject*> speedObjects;

    void capture(PlayLayer* p) {
        pl = p;
        if (!pl) return;
        auto& gs = pl->m_gameState;
        cameraZoom            = gs.m_cameraZoom;
        targetCameraZoom      = gs.m_targetCameraZoom;
        cameraOffset          = gs.m_cameraOffset;
        cameraPosition        = gs.m_cameraPosition;
        cameraPosition2       = gs.m_cameraPosition2;
        cameraAngle           = gs.m_cameraAngle;
        targetCameraAngle     = gs.m_targetCameraAngle;
        cameraEdge0           = gs.m_cameraEdgeValue0;
        cameraEdge1           = gs.m_cameraEdgeValue1;
        cameraEdge2           = gs.m_cameraEdgeValue2;
        cameraEdge3           = gs.m_cameraEdgeValue3;
        cameraShakeEnabled    = gs.m_cameraShakeEnabled;
        cameraShakeFactor     = gs.m_cameraShakeFactor;
        cameraStepDiff        = gs.m_cameraStepDiff;
        isDualMode            = gs.m_isDualMode;
        dualRelated           = gs.m_dualRelated;
        levelFlipping         = gs.m_levelFlipping;
        gravityRelated        = gs.m_gravityRelated;
        portalY               = gs.m_portalY;
        lastActivatedPortal1  = gs.m_lastActivatedPortal1;
        lastActivatedPortal2  = gs.m_lastActivatedPortal2;
        timeWarp              = gs.m_timeWarp;
        queuedTimeWarp        = gs.m_queuedTimeWarp;
        timeWarpRelated       = gs.m_timeWarpRelated;
        currentChannel        = gs.m_currentChannel;
        rotateChannel         = gs.m_rotateChannel;
        speedObjects.clear();
        if (auto* arr = pl->m_speedObjects) {
            speedObjects.reserve(arr->count());
            for (int i = 0; i < static_cast<int>(arr->count()); ++i) {
                speedObjects.push_back(arr->objectAtIndex(i));
            }
        }
    }
    void restore() {
        if (!pl) return;
        auto& gs = pl->m_gameState;
        gs.m_cameraZoom           = cameraZoom;
        gs.m_targetCameraZoom     = targetCameraZoom;
        gs.m_cameraOffset         = cameraOffset;
        gs.m_cameraPosition       = cameraPosition;
        gs.m_cameraPosition2      = cameraPosition2;
        gs.m_cameraAngle          = cameraAngle;
        gs.m_targetCameraAngle    = targetCameraAngle;
        gs.m_cameraEdgeValue0     = cameraEdge0;
        gs.m_cameraEdgeValue1     = cameraEdge1;
        gs.m_cameraEdgeValue2     = cameraEdge2;
        gs.m_cameraEdgeValue3     = cameraEdge3;
        gs.m_cameraShakeEnabled   = cameraShakeEnabled;
        gs.m_cameraShakeFactor    = cameraShakeFactor;
        gs.m_cameraStepDiff       = cameraStepDiff;
        gs.m_isDualMode           = isDualMode;
        gs.m_dualRelated          = dualRelated;
        gs.m_levelFlipping        = levelFlipping;
        gs.m_gravityRelated       = gravityRelated;
        gs.m_portalY              = portalY;
        gs.m_lastActivatedPortal1 = lastActivatedPortal1;
        gs.m_lastActivatedPortal2 = lastActivatedPortal2;
        gs.m_timeWarp             = timeWarp;
        gs.m_queuedTimeWarp       = queuedTimeWarp;
        gs.m_timeWarpRelated      = timeWarpRelated;
        gs.m_currentChannel       = currentChannel;
        gs.m_rotateChannel        = rotateChannel;
        if (auto* arr = pl->m_speedObjects) {
            arr->removeAllObjects();
            for (auto* obj : speedObjects) {
                if (obj) arr->addObject(obj);
            }
        }
    }
};

}  // namespace

TrajectorySimulator& TrajectorySimulator::get() {
    static TrajectorySimulator instance;
    return instance;
}

PlayerObject* TrajectorySimulator::createSimPlayer(PlayLayer* pl) {
    PlayerObject* p = PlayerObject::create(1, 1, pl, pl, true);
    p->setPosition({0.f, 105.f});
    p->setVisible(false);
    p->setID("trajectory-sim-player"_spr);
    pl->m_objectLayer->addChild(p);
    return p;
}

void TrajectorySimulator::adoptOwnRings(PlayerObject* sim, cocos2d::CCArray*& slot) {
    // Capture (or allocate) a private CCArray for the sim's m_touchingRings
    // so it can NEVER share the real player's array after copyAttributes.
    // We retain it so the sim PlayerObject's eventual destructor can release
    // one ref while our own ref keeps the array alive between sims.
    if (!sim) return;
    if (sim->m_touchingRings) {
        slot = sim->m_touchingRings;
    } else {
        slot = cocos2d::CCArray::create();
        sim->m_touchingRings = slot;
    }
    slot->retain();
}

cocos2d::CCDrawNode* TrajectorySimulator::ensureDrawNode() {
    if (m_drawNode || !m_pl) return m_drawNode;
    m_drawNode = CCDrawNode::create();
    m_drawNode->setID("trajectory-draw-node"_spr);
    m_drawNode->retain();
    auto* dbg = m_pl->m_debugDrawNode;
    if (dbg && dbg->getParent()) {
        dbg->getParent()->addChild(m_drawNode);
        m_drawNode->setZOrder(dbg->getZOrder());
    } else {
        m_pl->addChild(m_drawNode);
    }
    m_drawNode->setVisible(m_show);
    return m_drawNode;
}

void TrajectorySimulator::onPlayLayerInit(PlayLayer* pl) {
    // Defensive cleanup: if PlayLayer::onQuit didn't fire on the previous
    // level (Eclipse mod chain, scene transition shortcuts, crashes), our
    // m_drawNode is orphaned-but-retained and ensureDrawNode would skip
    // re-creation. Drop it explicitly so the new layer gets a fresh node.
    // m_simP1/m_simP2 weren't retained by us — their old parent destroyed
    // them, so we just null and recreate.
    if (m_drawNode) {
        m_drawNode->removeFromParent();
        m_drawNode->release();
        m_drawNode = nullptr;
    }
    // Defensive: if the previous level's onQuit didn't fire (Eclipse / scene
    // shortcuts / crashes), our retained rings refs are still held. The old
    // sim PlayerObjects were freed by their parent, so the array is already
    // down to refcount 1 (our ref) — release to free, then re-allocate per sim.
    if (m_simP1OwnRings) { m_simP1OwnRings->release(); m_simP1OwnRings = nullptr; }
    if (m_simP2OwnRings) { m_simP2OwnRings->release(); m_simP2OwnRings = nullptr; }
    m_simP1 = nullptr;
    m_simP2 = nullptr;

    m_pl = pl;
    m_simP1 = createSimPlayer(pl);
    adoptOwnRings(m_simP1, m_simP1OwnRings);
    m_simP2 = createSimPlayer(pl);
    adoptOwnRings(m_simP2, m_simP2OwnRings);
    ensureDrawNode();
    m_player1Pressed = false;
    m_player2Pressed = false;
    m_simulating = false;
    m_inSimulate = false;
    m_levelReady = false;
    m_simP1Dead = false;
    m_simP2Dead = false;
    m_activated.clear();
}

void TrajectorySimulator::onPlayLayerReset() {
    if (m_drawNode) m_drawNode->clear();
    m_player1Pressed = false;
    m_player2Pressed = false;
    m_simulating = false;
    m_inSimulate = false;
    m_simP1Dead = false;
    m_simP2Dead = false;
}

void TrajectorySimulator::onPlayLayerQuit() {
    if (m_drawNode) {
        m_drawNode->removeFromParent();
        m_drawNode->release();
        m_drawNode = nullptr;
    }
    if (m_simP1OwnRings) { m_simP1OwnRings->release(); m_simP1OwnRings = nullptr; }
    if (m_simP2OwnRings) { m_simP2OwnRings->release(); m_simP2OwnRings = nullptr; }
    m_simP1 = nullptr;
    m_simP2 = nullptr;
    m_pl = nullptr;
    m_simulating = false;
    m_inSimulate = false;
    m_levelReady = false;
    m_simP1Dead = false;
    m_simP2Dead = false;
}

void TrajectorySimulator::setShowTrajectory(bool v) {
    m_show = v;
    if (m_drawNode) {
        if (!v) m_drawNode->clear();
        m_drawNode->setVisible(v);
    }
}

void TrajectorySimulator::setIterations(int v) {
    if (v < 1) v = 1;
    if (v > 1000) v = 1000;
    m_iterations = v;
}

void TrajectorySimulator::setPadsEnabled(bool v)    { m_pads = v; }
void TrajectorySimulator::setOrbsEnabled(bool v)    { m_orbs = v; }
void TrajectorySimulator::setPortalsEnabled(bool v) { m_portals = v; }

void TrajectorySimulator::recordRealButton(bool down, bool isP1) {
    if (isP1) m_player1Pressed = down;
    else      m_player2Pressed = down;
}

void TrajectorySimulator::setFrameDelta(float dt) {
    if (!m_pl) return;
    float warp = m_pl->m_gameState.m_timeWarp;
    if (warp <= 0.f) warp = 1.f;
    m_frameDt = dt / warp;
}

bool TrajectorySimulator::markSimDeadIfSimPlayer(PlayerObject* p) {
    if (p == m_simP1) { m_simP1Dead = true; return true; }
    if (p == m_simP2) { m_simP2Dead = true; return true; }
    return false;
}

bool TrajectorySimulator::isSimDead(PlayerObject* p) const {
    if (p == m_simP1) return m_simP1Dead;
    if (p == m_simP2) return m_simP2Dead;
    return false;
}

void TrajectorySimulator::clearSimDead(PlayerObject* p) {
    if (p == m_simP1) m_simP1Dead = false;
    else if (p == m_simP2) m_simP2Dead = false;
}

void TrajectorySimulator::markActivated(EnhancedGameObject* obj) {
    if (!obj) return;
    // Diagnostic for orb false-hit (E4): when a sim activates an orb that's
    // visually far from its position, this trace pinpoints the culprit.
    // Bounded by m_activated dedup per branch + a global rate-limit so
    // candidate × frame × orb fan-out can't drown the log.
    if (m_simulating && m_activated.find(obj) == m_activated.end()) {
        auto* sim = m_simP1 ? m_simP1 : m_simP2;
        if (sim) {
            auto const sp = sim->getPosition();
            auto const op = obj->getPosition();
            float const dx = sp.x - op.x;
            float const dy = sp.y - op.y;
            float const dist = std::sqrt(dx * dx + dy * dy);
            if (dist > 30.f && --m_orbFarLogBudget <= 0) {
                m_orbFarLogBudget = 60;  // ≤1 line / ~1s at 60Hz search cadence
                geode::log::warn(
                    "[orb-far-activate] objType={} simPos=({:.1f},{:.1f}) "
                    "objPos=({:.1f},{:.1f}) dist={:.1f}",
                    static_cast<int>(obj->m_objectType), sp.x, sp.y, op.x, op.y, dist);
            }
        }
    }
    m_activated.insert(obj);
}

bool TrajectorySimulator::hasBeenActivated(EnhancedGameObject* obj) const {
    return obj && m_activated.find(obj) != m_activated.end();
}

void TrajectorySimulator::markSimDestroyed(GameObject* obj) {
    if (obj) m_simDestroyed.insert(obj);
}

bool TrajectorySimulator::isSimDestroyed(GameObject* obj) const {
    return obj && m_simDestroyed.find(obj) != m_simDestroyed.end();
}

void TrajectorySimulator::clearSimDestroyed() {
    m_simDestroyed.clear();
}

void TrajectorySimulator::clearSimRingState(PlayerObject* sim) {
    if (!sim) return;
    sim->m_dashRing = nullptr;
    // Full dash-state reset: nulling m_dashRing alone leaves m_isDashing/dash
    // angle/origin from a prior branch. The engine renders the dash raycast
    // line whenever m_isDashing is true regardless of m_dashRing, which made
    // the sim show a permanent attached-line visual after activating any dash
    // orb. copyAttributes is called before this, so we'll re-derive any state
    // the real player still legitimately holds on the next branch.
    sim->m_isDashing      = false;
    sim->m_dashX          = 0.0;
    sim->m_dashY          = 0.0;
    sim->m_dashAngle      = 0.0;
    sim->m_dashStartTime  = 0.0;

    sim->m_padRingRelated = false;
    sim->m_ringJumpRelated = false;
    sim->m_ringRelatedSet.clear();
    sim->m_stateRingJump = false;
    sim->m_stateRingJump2 = false;
    sim->m_touchedRing = false;
    sim->m_touchedCustomRing = false;
    // Restore the sim's PRIVATE m_touchingRings pointer. copyAttributes(base)
    // (called immediately before this in initSim/runBranch) shallow-copies
    // base's CCArray pointer onto the sim, so without this restore we'd share
    // the real player's array — and removeAllObjects below would clobber the
    // real player's view of which rings it's overlapping. With the restore,
    // the sim only ever touches its own retained array.
    auto* ownRings = (sim == m_simP1) ? m_simP1OwnRings
                  : (sim == m_simP2) ? m_simP2OwnRings : nullptr;
    if (ownRings) {
        sim->m_touchingRings = ownRings;
        ownRings->removeAllObjects();
    } else if (sim->m_touchingRings) {
        sim->m_touchingRings->removeAllObjects();
    }
    sim->m_touchedRings.clear();
    sim->m_jumpPadRelated.clear();
}

void TrajectorySimulator::clearPerTickRingOverlap(PlayerObject* sim) {
    if (!sim) return;
    // Only the "currently overlapping this tick" state. checkCollisions will
    // re-add via playerTouchedRing for orbs the sim is genuinely overlapping
    // right now. m_touchedRing/m_touchedCustomRing are per-tick edge-detect
    // bools the engine sets on first overlap; clearing them mirrors the
    // engine's own tick-start clear and keeps the bot from seeing stale
    // "touched a ring last tick" state when the sim has moved past.
    sim->m_touchedRing       = false;
    sim->m_touchedCustomRing = false;
    if (sim->m_touchingRings) {
        sim->m_touchingRings->removeAllObjects();
    }
}

void TrajectorySimulator::simulate() {
    if (!m_show || !m_pl || !m_simP1 || !m_simP2) return;
    if (!m_levelReady) return;
    if (m_inSimulate) return;
    m_inSimulate = true;
    struct ScopeReset { bool& f; ~ScopeReset() { f = false; } } _sr{ m_inSimulate };
    auto* draw = ensureDrawNode();
    if (!draw) return;
    draw->clear();

    m_simulating = true;
    simulateForPlayer(m_simP1, m_simP2, m_pl->m_player1, /*isPlayer2=*/false);
    if (m_pl->m_gameState.m_isDualMode && m_pl->m_player2) {
        simulateForPlayer(m_simP2, m_simP1, m_pl->m_player2, /*isPlayer2=*/true);
    }
    m_simulating = false;
}

void TrajectorySimulator::simulateForPlayer(PlayerObject* a, PlayerObject* b,
                                            PlayerObject* base, bool isP2) {
    runBranch(a, base, /*holdAtStart=*/true,  isP2);
    runBranch(b, base, /*holdAtStart=*/false, isP2);
}

void TrajectorySimulator::runBranch(PlayerObject* sim, PlayerObject* base,
                                    bool holdAtStart, bool /*isP2*/) {
    if (!sim || !base || !m_pl) return;

    sim->setVisible(false);
    sim->copyAttributes(base);
    sim->m_gravityMod = base->m_gravityMod;
    sim->m_isOnGround = base->m_isOnGround;
    clearSimRingState(sim);
    m_activated.clear();
    clearSimDestroyed();
    clearSimDead(sim);

    if (holdAtStart) sim->pushButton(PlayerButton::Jump);
    else             sim->releaseButton(PlayerButton::Jump);

    auto color = holdAtStart ? ccc4f(0.f, 1.f, 0.1f, 1.f)
                             : ccc4f(1.f, 0.f, 0.1f, 1.f);
    auto* draw = m_drawNode;

    // Snapshot layer state per-branch: hold-branch and release-branch must
    // both start from the SAME real-game state. Without restore between them,
    // mutations made during the hold sim (camera zoom, dual mode, level flip,
    // portal markers) carry into the release sim and skew its prediction.
    LayerStateSnapshot snap; snap.capture(m_pl);

    for (int i = 0; i < m_iterations; ++i) {
        cocos2d::CCPoint prev = sim->getPosition();
        sim->resetCollisionLog(true);
        clearPerTickRingOverlap(sim);
        m_pl->checkCollisions(sim, m_frameDt, false);
        if (isSimDead(sim)) break;

        sim->update(m_frameDt);

        draw->drawSegment(prev, sim->getPosition(), 0.65f, color);
    }

    snap.restore();
    drawHitboxAtEnd(sim, holdAtStart);
}

PlanResult TrajectorySimulator::runPlan(PlayerObject* base1, PlayerObject* base2,
                                        std::vector<bool> const& plan,
                                        std::vector<bool> const& plan2) {
    PlanResult result;
    if (!base1 || !m_pl || !m_simP1 || plan.empty()) return result;

    auto* simA = m_simP1;
    auto* simB = (base2 && m_simP2) ? m_simP2 : nullptr;

    auto initSim = [this](PlayerObject* sim, PlayerObject* base) {
        sim->setVisible(false);
        sim->copyAttributes(base);
        sim->m_gravityMod = base->m_gravityMod;
        sim->m_isOnGround = base->m_isOnGround;
        clearSimRingState(sim);
        clearSimDead(sim);
    };
    initSim(simA, base1);
    if (simB) initSim(simB, base2);
    m_activated.clear();
    clearSimDestroyed();

    // Snapshot per-runPlan: bot scoring calls runPlan many times per visual
    // frame to grade candidates; without restore, candidate N starts from
    // layer state already mutated by candidates 0..N-1 — that's the root
    // cause of the "orange line predicts survival, ground truth dies"
    // divergence. Mutations during this single plan's execution still
    // persist (a portal crossed mid-plan affects the rest of the plan).
    LayerStateSnapshot snap; snap.capture(m_pl);

    m_simulating = true;

    result.positions.reserve(plan.size() + 1);
    result.positions.push_back(simA->getPosition());
    if (simB) {
        result.positions2.reserve(plan.size() + 1);
        result.positions2.push_back(simB->getPosition());
    }

    // Independent P2 plan only meaningful when simB exists; otherwise P2
    // mirrors P1 (or doesn't exist at all).
    bool const useIndependentP2 = simB && !plan2.empty();

    bool prevA = false;
    bool prevB = false;
    bool firstA = true;
    bool firstB = true;
    for (size_t i = 0; i < plan.size(); ++i) {
        bool const wantA = plan[i];
        // P2 input: own plan if 2P-mode (clamp to plan2 length, then hold last
        // value); else mirror P1.
        bool wantB;
        if (useIndependentP2) {
            wantB = plan2[std::min(i, plan2.size() - 1)];
        } else {
            wantB = wantA;
        }

        if (firstA || wantA != prevA) {
            if (wantA) simA->pushButton(PlayerButton::Jump);
            else       simA->releaseButton(PlayerButton::Jump);
            prevA = wantA;
            firstA = false;
        }
        if (simB && (firstB || wantB != prevB)) {
            if (wantB) simB->pushButton(PlayerButton::Jump);
            else       simB->releaseButton(PlayerButton::Jump);
            prevB = wantB;
            firstB = false;
        }

        simA->resetCollisionLog(true);
        clearPerTickRingOverlap(simA);
        m_pl->checkCollisions(simA, m_frameDt, false);
        if (isSimDead(simA)) { result.died = true; break; }

        if (simB) {
            simB->resetCollisionLog(true);
            clearPerTickRingOverlap(simB);
            m_pl->checkCollisions(simB, m_frameDt, false);
            if (isSimDead(simB)) { result.died = true; break; }
        }

        simA->update(m_frameDt);
        if (simB) simB->update(m_frameDt);

        result.positions.push_back(simA->getPosition());
        if (simB) result.positions2.push_back(simB->getPosition());
        ++result.framesSurvived;
    }

    m_simulating = false;
    snap.restore();
    return result;
}

void TrajectorySimulator::drawHitboxAtEnd(PlayerObject* sim, bool holdBranch) {
    if (!sim || !m_drawNode) return;
    auto rect = sim->getObjectRect();
    cocos2d::CCPoint verts[4] = {
        {rect.getMinX(), rect.getMinY()},
        {rect.getMinX(), rect.getMaxY()},
        {rect.getMaxX(), rect.getMaxY()},
        {rect.getMaxX(), rect.getMinY()},
    };
    auto outline = holdBranch ? ccc4f(0.f, 1.f, 0.1f, 1.f)
                              : ccc4f(1.f, 0.f, 0.1f, 1.f);
    auto fill = ccc4f(0.f, 0.f, 0.f, 0.f);
    m_drawNode->drawPolygon(verts, 4, fill, 0.25f, outline);
}

}
