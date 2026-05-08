#include "Trajectory.hpp"
#include "Pads.hpp"
#include "Portals.hpp"
#include "Orbs.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>
#include <Geode/modify/EnhancedGameObject.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/HardStreak.hpp>

using namespace geode::prelude;

namespace {

inline traj::TrajectorySimulator& sim() { return traj::TrajectorySimulator::get(); }

}

class $modify(TrajPlayLayerHook, PlayLayer) {
    // setupHasCompleted runs from inside PlayLayer::init (via
    // processCreateObjectsFromSetup) and itself triggers the first
    // updateCamera tick. If we waited until PlayLayer::init returned to
    // initialize sim state, that first updateCamera would simulate against
    // stale/dangling sim players from a prior level (macOS heap-reuse can
    // even land the new PlayLayer at the same address as the freed prior
    // one, defeating any pointer-equality gate). Initializing here ensures
    // sim state is fresh BEFORE the engine fires its setup-tick updateCamera.
    void setupHasCompleted() {
        sim().onPlayLayerInit(this);
        PlayLayer::setupHasCompleted();
        // Same rationale as BotHooks: defer simulate() until the engine's
        // setup-tick updateCamera has finished firing.
        sim().setLevelReady(true);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        sim().onPlayLayerReset();
    }

    void onQuit() {
        sim().onPlayLayerQuit();
        PlayLayer::onQuit();
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        if (gameObject != m_anticheatSpike && sim().markSimDeadIfSimPlayer(player)) return;
        PlayLayer::destroyPlayer(player, gameObject);
    }

    void playEndAnimationToPos(cocos2d::CCPoint p) {
        if (sim().isSimulating()) return;
        PlayLayer::playEndAnimationToPos(p);
    }

    void playPlatformerEndAnimationToPos(cocos2d::CCPoint p, bool instant) {
        if (sim().isSimulating()) return;
        PlayLayer::playPlatformerEndAnimationToPos(p, instant);
    }

    // Gravity portal visual particles. flipGravity already passes
    // noEffects=true for sim, but the engine occasionally calls
    // playGravityEffect on its own path; hook the most-derived virtual to
    // catch both routes.
    void playGravityEffect(bool flip) {
        if (sim().isSimulating()) return;
        PlayLayer::playGravityEffect(flip);
    }
};

class $modify(TrajBaseLayerHook, GJBaseGameLayer) {
    // Same gate as BotBGLHook::updateCamera — `this` may be a
    // LevelEditorLayer (playtest), or a brand-new PlayLayer mid-init that
    // hasn't yet reached our PlayLayer::init hook (and so hasn't set
    // m_pl/m_simP1/m_simP2). In either case our cached state is stale and
    // simulating crashes on the dangling pointers.
    void updateCamera(float dt) {
        auto& s = sim();
        // Reentrancy guard: while runPlan/runBranch are executing, the engine's
        // collisionCheckObjects path can trip an inner updateCamera (e.g. via
        // trigger objects or camera-modifier effects activated by the sim
        // crossing them). Without this gate, the bot/sim re-enters infinitely
        // and overflows the stack.
        if (!s.isSimulating() && static_cast<GJBaseGameLayer*>(s.playLayer()) == this) {
            s.simulate();
        }
        GJBaseGameLayer::updateCamera(dt);
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        if (button == 1) sim().recordRealButton(down, isPlayer1);
        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    // Sim-side breakable block / object-destroy handling.
    //
    // The engine's destroyObject mutates the real level (removes from active
    // arrays, hides sprites, fires spawn-remove triggers). None of that is
    // cheaply reversible per-runPlan, so we never let it run during sim. But
    // we still want sim physics to "feel" the block as broken — otherwise the
    // sim collides with a block the real player will break through, and the
    // bot picks worse paths than reality affords.
    //
    // Resolution: track the object in a sim-local destroyed set. The
    // collisionCheckObjects filter (above) drops sim-destroyed objects from
    // the candidate list, so the sim's collision pass treats the block as
    // gone — same physical effect as destruction, zero engine state change.
    // The set is cleared at the start of each runPlan/runBranch, so each
    // sim search starts from a clean view of the level.
    //
    // destroyPlayer is a separate, already-hooked path (PlayLayer's cascading-
    // dual suppression); this only catches GameObject destruction, which is
    // the breakable-block / similar-object route.
    void destroyObject(GameObject* object) {
        auto& s = sim();
        if (s.isSimulating()) {
            s.markSimDestroyed(object);
            return;
        }
        GJBaseGameLayer::destroyObject(object);
    }

    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* vec,
                               int objectsCount, float dt) {
        auto& s = sim();
        if (!s.isSimulating()) {
            GJBaseGameLayer::collisionCheckObjects(player, vec, objectsCount, dt);
            return;
        }

        // Spatial cull window. GD blocks are 30 units. The horizon is
        // 360 physics ticks at ~10.4 units/tick (speed 1) ≈ 3744 units, but
        // most plans terminate well inside that. 80 blocks (2400 units) ahead
        // is comfortably larger than typical sim reach and still trims the
        // fat from level-end-tail object lists. 30 blocks (900) behind keeps
        // the recent ground/wall set so retreat-style paths don't lose floor
        // collision. Wide objects (long platforms whose origin lies behind
        // the back edge) are kept via the right-edge test below.
        constexpr float kBlocks      = 30.f;
        constexpr float kAheadBlocks = 80.f;
        constexpr float kBackBlocks  = 30.f;
        constexpr float kAheadUnits  = kAheadBlocks * kBlocks;
        constexpr float kBackUnits   = kBackBlocks  * kBlocks;
        float const px       = player ? player->getPositionX() : 0.f;
        float const minX     = px - kBackUnits;
        float const maxX     = px + kAheadUnits;

        gd::vector<GameObject*> filtered;
#ifndef GEODE_IS_ANDROID
        filtered.reserve(objectsCount);
#endif
        for (int i = 0; i < objectsCount; ++i) {
            GameObject* obj = vec->at(i);
            // Sim-destroyed (breakable blocks etc. broken earlier in this
            // runPlan): drop so the sim's physics treats the block as gone.
            // Real level state is untouched — markSimDestroyed only writes
            // to TrajectorySimulator's per-run set, which clears at the
            // start of each runPlan/runBranch.
            if (s.isSimDestroyed(obj)) continue;
            auto t = obj->m_objectType;
            bool keep = true;
            if (traj::isPad(t))         keep = s.wantsPads();
            else if (traj::isOrb(t))    keep = s.wantsOrbs();
            else if (traj::isPortal(t)) keep = s.wantsPortals();
            // Coins (UserCoin = 31, SecretCoin = 22). Drop unconditionally for
            // sim. The engine's coin-collection path fires from inside this
            // collision pass, so removing the coin from the candidate set is
            // the only way to keep it from being collected: hooking the
            // collection function (collectedObject / pickupItem) doesn't
            // catch every code path. Sim physics doesn't care about coins, so
            // there's nothing to lose by skipping them.
            else if (t == GameObjectType::UserCoin
                  || t == GameObjectType::SecretCoin) keep = false;
            if (keep && player) {
                float const ox = obj->getPositionX();
                if (ox > maxX) {
                    keep = false;
                } else if (ox < minX) {
                    auto const r = obj->getObjectRect();
                    if (ox + r.size.width < minX) keep = false;
                }
            }
            if (keep) filtered.push_back(obj);
        }
        GJBaseGameLayer::collisionCheckObjects(player, &filtered,
                                               static_cast<int>(filtered.size()), dt);
    }

    void flipGravity(PlayerObject* p, bool flip, bool noEffects) {
        auto& s = sim();
        if (s.isSimulating()) {
            if (!s.isSimPlayer(p)) return;
            GJBaseGameLayer::flipGravity(p, flip, true);
            return;
        }
        GJBaseGameLayer::flipGravity(p, flip, noEffects);
    }

    bool canBeActivatedByPlayer(PlayerObject* player, EffectGameObject* effect) {
        auto& s = sim();
        if (s.isSimulating()) return s.isSimPlayer(player);
        return GJBaseGameLayer::canBeActivatedByPlayer(player, effect);
    }

    // (Coin / pickup collection: gated at the source via the collisionCheckObjects
    // filter above, which drops coin object types from the sim's collision set
    // entirely. Hooking the engine's coin-collect function isn't enough — coins
    // can be collected through multiple code paths that all originate from the
    // collision pass, so filtering at the input is the only complete fix.)

    // Mode-switch / teleport portals: sim NEEDS the actual transition so
    // post-portal physics is accurate (without it, sim hitbox/physics is
    // wrong and the next collision tick kills it — the "gamemode portals
    // treat sim like spike walls" regression). The state these mutate
    // (m_isDualMode, camera fields, m_lastActivatedPortal1/2, m_levelFlipping,
    // m_gravityRelated, ...) is snapshotted and restored at sim boundaries
    // by LayerStateSnapshot in Trajectory.cpp, so we can let super run
    // unconditionally without per-hook restore. Earlier per-hook restores of
    // m_lastActivatedPortal1/2 were actively harmful: they reset the marker
    // mid-sim, so the engine re-activated the same portal every tick the
    // sim overlapped it, exploding sim physics.

    // Toggle level flip (X-flip portals / Flip Y triggers). Force noEffects
    // for sim — the visual flip animation would persist on the real screen
    // because it's an action, outside the snapshot's reach. Physics-side
    // m_levelFlipping is restored by the snapshot.
    void toggleFlipped(bool flip, bool noEffects) {
        if (sim().isSimulating()) {
            GJBaseGameLayer::toggleFlipped(flip, true);
            return;
        }
        GJBaseGameLayer::toggleFlipped(flip, noEffects);
    }

    // Camera shake / move / rotation are kicked off via CCActions running
    // on the layer; the snapshot can revert m_cameraPosition / m_cameraAngle
    // values, but the actions themselves continue executing past sim end and
    // re-apply movements to the real game. Suppress at source during sim.
    void shakeCamera(float duration, float strength, float interval) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::shakeCamera(duration, strength, interval);
    }
    void moveCameraToPos(cocos2d::CCPoint pos) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::moveCameraToPos(pos);
    }
    void updateScreenRotation(float rot, bool add, bool convert, float duration,
                              int easing, float rate, int uniqueID, int controlID) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::updateScreenRotation(rot, add, convert, duration,
                                              easing, rate, uniqueID, controlID);
    }

    // Screen flash on portal/effect activations. No player arg.
    void playFlashEffect(float duration, int flashes, float unknown) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::playFlashEffect(duration, flashes, unknown);
    }

    // Particle factories. The state snapshot can revert m_camera/m_isDualMode/
    // etc., but it can't undo a CCParticleSystemQuad that's been added to the
    // scene tree — those keep emitting after sim ends. Suppress at source.
    // spawnParticle returns the particle pointer; callers in the engine may or
    // may not dereference it. Returning nullptr is the standard sim pattern;
    // any caller crash on null would be a pre-existing engine bug surfaced by
    // the suppression, not introduced by it.
    cocos2d::CCParticleSystemQuad* spawnParticle(char const* plist, int zOrder,
                                                 cocos2d::tCCPositionType positionType,
                                                 cocos2d::CCPoint position) {
        if (sim().isSimulating()) return nullptr;
        return GJBaseGameLayer::spawnParticle(plist, zOrder, positionType, position);
    }
    void spawnParticleTrigger(SpawnParticleGameObject* object) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::spawnParticleTrigger(object);
    }
    void spawnParticleTrigger(int particleID, cocos2d::CCPoint position,
                              float rotation, float scale) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::spawnParticleTrigger(particleID, position, rotation, scale);
    }
    void lightningFlash(cocos2d::CCPoint to, cocos2d::ccColor3B color) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::lightningFlash(to, color);
    }
    void lightningFlash(cocos2d::CCPoint from, cocos2d::CCPoint to,
                        cocos2d::ccColor3B color, float lineWidth, float duration,
                        int displacement, bool flash, float opacity) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::lightningFlash(from, to, color, lineWidth, duration,
                                        displacement, flash, opacity);
    }
    // playSpeedParticle fires from speed-mod portal traversal. Sim NEEDS the
    // speed change itself (handled in TrajEffectHook::triggerObject's
    // isSpeedMod exception), but the visual particle burst should not appear
    // on the real screen when sim crosses a speed portal.
    void playSpeedParticle(float timeMod) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::playSpeedParticle(timeMod);
    }

    // Camera-tween CCActions. Same problem class as shakeCamera/moveCameraToPos:
    // the snapshot reverts m_cameraPosition but the action keeps running and
    // re-applies the tween to the real game. Suppress at source.
    void cameraMoveX(float value, float duration, float rate, bool unused) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::cameraMoveX(value, duration, rate, unused);
    }
    void cameraMoveY(float value, float duration, float rate, bool force) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::cameraMoveY(value, duration, rate, force);
    }
    void updateCameraOffsetX(float offsetX, float duration, int easingType,
                             float easingRate, int uniqueID, int controlID) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::updateCameraOffsetX(offsetX, duration, easingType,
                                             easingRate, uniqueID, controlID);
    }
    void updateCameraOffsetY(float offsetY, float duration, int easingType,
                             float easingRate, int uniqueID, int controlID) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::updateCameraOffsetY(offsetY, duration, easingType,
                                             easingRate, uniqueID, controlID);
    }
    void updateStaticCameraPos(cocos2d::CCPoint pos, bool staticX, bool staticY,
                               bool followOrSmoothEase, float time,
                               int easingType, float easingRate) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::updateStaticCameraPos(pos, staticX, staticY,
                                               followOrSmoothEase, time,
                                               easingType, easingRate);
    }
    void updateStaticCameraPosToGroup(int centerID, bool updateX, bool updateY,
                                      bool followObject, float followEase,
                                      float duration, int easingType,
                                      float easingRate, bool smoothVelocity,
                                      float velocityMod) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::updateStaticCameraPosToGroup(centerID, updateX, updateY,
                                                      followObject, followEase,
                                                      duration, easingType,
                                                      easingRate, smoothVelocity,
                                                      velocityMod);
    }

    // GJBaseGameLayer::playGravityEffect is an inline definition (Geode can't
    // hook it). We hook PlayLayer::playGravityEffect (the runtime-resolved
    // virtual override) instead — see TrajPlayLayerHook above.
};

class $modify(TrajPlayerObjectHook, PlayerObject) {
    void update(float dt) {
        PlayerObject::update(dt);
        if (!sim().isSimulating()) sim().setFrameDelta(dt);
    }

    void incrementJumps() {
        if (sim().isSimulating()) return;
        PlayerObject::incrementJumps();
    }

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to) {
        if (sim().isSimulating()) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }

    void playBumpEffect(int objectType, GameObject* obj) {
        if (sim().isSimulating()) return;
        PlayerObject::playBumpEffect(objectType, obj);
    }

    // Mode-switch portal flash. Lives on the player, not the layer, so the
    // GJBaseGameLayer playFlashEffect gate doesn't catch it. Same suppress
    // pattern.
    void flashPlayer(float flashDuration, float flashDelay,
                     cocos2d::ccColor3B mainColor, cocos2d::ccColor3B secondColor) {
        if (sim().isSimulating()) return;
        PlayerObject::flashPlayer(flashDuration, flashDelay, mainColor, secondColor);
    }

    // Speed-mod portals call into PlayerObject::updateTimeMod via the layer's
    // updateTimeMod(speed, /*players=*/true, ...), which fans out to BOTH real
    // players regardless of who crossed. When a sim crosses a speed portal,
    // EffectGameObject::triggerObject is allowed (TrajEffectHook gates on
    // !isSpeedMod), and that path lands here on m_player1 / m_player2 — and
    // overwrites the real players' m_playerSpeed. Block that during sim
    // unless the target is a sim player (so the sim still gets its own
    // speed update for accurate prediction).
    void updateTimeMod(float speed, bool noEffects) {
        auto& s = sim();
        if (s.isSimulating() && !s.isSimPlayer(this)) return;
        PlayerObject::updateTimeMod(speed, noEffects);
    }
};

class $modify(TrajEffectHook, EffectGameObject) {
    // Speed-modifier portals (m_speedModType != 0) MUST fire during sim —
    // they're how the sim's PlayerObject::m_playerSpeed updates so lookahead
    // pathing accounts for upcoming speed changes. Every other effect
    // (color/move/spawn triggers, activation flashes) we suppress because
    // their side effects are global and would either (a) contaminate the
    // real game state when the sim crosses them or (b) leave persistent
    // visual children attached to shared GameObjects.
    bool isSpeedMod() const { return m_speedModType != 0; }

    // Speed-mod special-case: sim DOES need the engine to add the portal to
    // PlayLayer::m_speedObjects (so sim physics picks up the new speed during
    // its runPlan), but the engine also writes m_activatedByPlayer1/2 on the
    // portal itself. m_speedObjects is rolled back by LayerStateSnapshot at
    // runPlan boundaries; the per-object activation flags are NOT in the
    // snapshot, so without explicit rollback they leak across runPlans —
    // when the real player eventually reaches the portal, hasBeenActivatedByPlayer
    // returns true (set by the sim's earlier crossing) and the engine refuses to
    // re-fire the trigger. Real player keeps old speed → diverges from the
    // path the bot just committed → premature death.
    //
    // Save-and-restore wrapper keeps sim physics speed-aware while leaving the
    // portal's "has the real player activated me?" state untouched.
    void triggerObject(GJBaseGameLayer* layer, int uniqueID,
                       gd::vector<int> const* remapKeys) {
        if (sim().isSimulating()) {
            if (!isSpeedMod()) return;
            bool prev1 = m_activatedByPlayer1;
            bool prev2 = m_activatedByPlayer2;
            EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
            m_activatedByPlayer1 = prev1;
            m_activatedByPlayer2 = prev2;
            return;
        }
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
    }

    void triggerActivated(float xPosition) {
        if (sim().isSimulating()) {
            if (!isSpeedMod()) return;
            bool prev1 = m_activatedByPlayer1;
            bool prev2 = m_activatedByPlayer2;
            EffectGameObject::triggerActivated(xPosition);
            m_activatedByPlayer1 = prev1;
            m_activatedByPlayer2 = prev2;
            return;
        }
        EffectGameObject::triggerActivated(xPosition);
    }
};

class $modify(TrajGameObjectHook, GameObject) {
    void playShineEffect() {
        if (sim().isSimulating()) return;
        GameObject::playShineEffect();
    }
};

class $modify(TrajEnhancedHook, EnhancedGameObject) {
    void activatedByPlayer(PlayerObject* player) {
        auto& s = sim();
        if (s.isSimPlayer(player)) {
            s.markActivated(this);
            return;
        }
        EnhancedGameObject::activatedByPlayer(player);
    }

    // Sim's view of "already activated" must combine its own per-run set
    // with the engine's real-player flags. EnhancedGameObject stores
    // m_activatedByPlayer1/2 (set on first activation) and resets them for
    // m_isMultiActivate orbs — so delegating to the real player gives us
    // correct multi-vs-single-activate semantics for free, instead of us
    // re-implementing the property logic. Single-activate orbs the real
    // player already consumed read as activated → sim won't ghost-bounce.
    bool hasBeenActivatedByPlayer(PlayerObject* player) {
        auto& s = sim();
        if (!s.isSimPlayer(player)) {
            return EnhancedGameObject::hasBeenActivatedByPlayer(player);
        }
        if (s.hasBeenActivated(this)) return true;
        auto* pl = s.playLayer();
        if (!pl) return false;
        if (pl->m_player1 && EnhancedGameObject::hasBeenActivatedByPlayer(pl->m_player1)) return true;
        if (pl->m_player2 && EnhancedGameObject::hasBeenActivatedByPlayer(pl->m_player2)) return true;
        return false;
    }
};

class $modify(TrajHardStreakHook, HardStreak) {
    void addPoint(cocos2d::CCPoint p) {
        if (sim().isSimulating()) return;
        HardStreak::addPoint(p);
    }
};
