#include "Trajectory.hpp"
#include "Pads.hpp"
#include "Portals.hpp"
#include "Orbs.hpp"
#include "Triggers.hpp"
#include "Telemetry.hpp"
#include "ShadowLayer.hpp"

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
        // Guard: when this fires for the SHADOW PlayLayer's setup (during
        // ShadowLayer::ensureCreated), skip all the mod's per-level
        // initialization — we don't want the simulator/telemetry to bind
        // to the shadow as their level reference. Just let super run so
        // shadow's own setup completes, then bail.
        if (bot::ShadowLayer::get().inShadowConstruction()) {
            PlayLayer::setupHasCompleted();
            return;
        }
        sim().onPlayLayerInit(this);
        PlayLayer::setupHasCompleted();
        // Same rationale as BotHooks: defer simulate() until the engine's
        // setup-tick updateCamera has finished firing.
        sim().setLevelReady(true);
        tel::levelStart(this);
    }

    // Cheap helper for the shadow gate. Every per-PlayLayer hook needs it
    // because PlayLayer::create internally invokes a chain of methods we
    // hook (resetLevel, destroyPlayer, etc.) AND we don't want those to
    // mutate the real simulator/telemetry state when they fire for the
    // shadow.
    bool isShadow_() {
        auto& sh = bot::ShadowLayer::get();
        return sh.inShadowConstruction() || sh.inShadowTick()
            || static_cast<PlayLayer*>(sh.shadow()) == static_cast<PlayLayer*>(this);
    }

    void resetLevel() {
        if (isShadow_()) { PlayLayer::resetLevel(); return; }
        PlayLayer::resetLevel();
        sim().onPlayLayerReset();
        tel::levelReset(m_level ? m_level->m_attempts : 0);
    }

    void onQuit() {
        if (isShadow_()) { PlayLayer::onQuit(); return; }
        tel::levelQuit();
        bot::ShadowLayer::get().onRealLevelQuit();
        sim().onPlayLayerQuit();
        PlayLayer::onQuit();
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        if (isShadow_()) { PlayLayer::destroyPlayer(player, gameObject); return; }
        if (gameObject != m_anticheatSpike && sim().markSimDeadIfSimPlayer(player)) return;
        // Telemetry: log real-player death with cause object type + percent.
        // Sim deaths are handled via markSimDeadIfSimPlayer above and are not
        // reported (would drown the log — bot dies thousands of times per
        // visual frame across candidates).
        tel::death(player, gameObject, getCurrentPercent());
        PlayLayer::destroyPlayer(player, gameObject);
    }

    void playEndAnimationToPos(cocos2d::CCPoint p) {
        if (isShadow_()) { PlayLayer::playEndAnimationToPos(p); return; }
        if (sim().isSimulating()) return;
        tel::win(getCurrentPercent());
        PlayLayer::playEndAnimationToPos(p);
    }

    void playPlatformerEndAnimationToPos(cocos2d::CCPoint p, bool instant) {
        if (isShadow_()) { PlayLayer::playPlatformerEndAnimationToPos(p, instant); return; }
        if (sim().isSimulating()) return;
        tel::win(getCurrentPercent());
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
        // Guard: if updateCamera fires for the SHADOW (either during its
        // construction or during a manual shadow tick), don't run the
        // trajectory simulator on it. The sim binds to the REAL PlayLayer
        // and would either crash or pollute shadow's state.
        auto& shadow = bot::ShadowLayer::get();
        if (shadow.inShadowConstruction() || shadow.inShadowTick()
            || static_cast<GJBaseGameLayer*>(shadow.shadow()) == static_cast<GJBaseGameLayer*>(this)) {
            GJBaseGameLayer::updateCamera(dt);
            return;
        }
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
            if (traj::isPad(t))          keep = s.wantsPads();
            else if (traj::isOrb(t))     keep = s.wantsOrbs();
            else if (traj::isPortal(t))  keep = s.wantsPortals();
            else if (traj::isTrigger(t)) keep = s.wantsTriggers();
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

    // Mode-switch portals (cube/ship/ball/ufo/robot/spider) call animatePortalY
    // to tween the camera-Y bound that defines the playable area. The instant
    // value of m_portalY is snapshotted/restored by LayerStateSnapshot, and
    // suppressing the tween prevents the CCAction from re-positioning the
    // real gameplay area past sim end.
    //
    // The ground-bar animation suppressions (animateInGroundNew /
    // animateInDualGroundNew / animateOutGroundNew) WERE also added in commit
    // 4 but turn out to be the regression vector for the "vertical line at
    // ball portal" bug — empirically, suppressing them leaves the engine's
    // ball-mode physics in an inconsistent state (the engine evidently reads
    // ground-bar position for ball-mode ground-resolve). Suppressing only
    // animatePortalY is sufficient to keep the camera-Y leak fix while not
    // breaking ball-mode sim physics. The visible ground bar tween is still
    // a minor cosmetic leak when sim crosses a mode portal, but that's
    // acceptable vs. the bot picking unsurvivable paths.
    void animatePortalY(float fromY, float toY, float duration, float easingRate) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::animatePortalY(fromY, toY, duration, easingRate);
    }
    // animateInGroundNew / animateInDualGroundNew / animateOutGroundNew are NOT
    // suppressed for sim — empirically, suppressing them breaks sim's
    // ball-mode physics (vertical-line trajectory bug on ball-portal crossing,
    // because the engine reads m_groundLayer position for ball ground-resolve).
    // Instead, LayerStateSnapshot captures the ground bar positions before
    // sim and restores + stopAllActions after sim, which cancels the visible
    // tween leak the original suppressions were meant to fix.

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
        auto& s = sim();
        if (!s.isSimulating()) {
            s.setFrameDelta(dt);
            // Sample real-player position to telemetry. PlayerObject::update
            // fires per physics tick (240Hz) — tel::realPos has its own
            // stride that drops it to ~10 samples/sec. Gate on player1 so
            // dual-mode levels don't double-log; player2 telemetry can be
            // added later if needed for 2P-mode debugging.
            auto* pl = s.playLayer();
            if (pl && this == pl->m_player1) {
                tel::realPos(this, pl->getCurrentPercent());
            }
        }
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

// GJBaseGameLayer-level updateTimeMod is the chokepoint that speed-portal
// activations route through. The engine's binary impl of the LAYER's
// updateTimeMod writes m_playerSpeed (and m_playerSpeedAC) inline on
// m_player1/m_player2 (the real players) BEFORE delegating to the per-player
// virtual. Those inline writes bypass the per-player TrajPlayerObjectHook
// gate, and m_simP1/m_simP2 (the sim players) are separate PlayerObjects
// that super never touches — so a naive save/restore around super reverts
// real's speed correctly but leaves sim's speed unchanged at the OLD value,
// meaning sim's predicted trajectories ignore speed-mod portals entirely.
//
// The correct dance: snapshot real → super → read the new speed from real
// (super wrote it there) → propagate that new speed to the corresponding
// sim player (m_simP1 mirrors m_player1, m_simP2 mirrors m_player2) →
// restore real. After this, real is unchanged, and the sim players that
// will simulate forward from this portal have the new speed in their
// physics state.
class $modify(TrajLayerSpeedHook, GJBaseGameLayer) {
    // Rate-limited diagnostic log budget for the speed-portal probe. Logs
    // appear in Geode's log file (typically %localappdata%/GeometryDash/geode/logs
    // on Windows, ~/Library/Application Support/GeometryDash/geode/logs on mac).
    // Search for "[speed-hook]" to find them.
    static inline int s_speedHookLogBudget = 0;

    void updateTimeMod(float speed, bool players, bool noEffects) {
        auto& s = sim();
        if (!s.isSimulating()) {
            GJBaseGameLayer::updateTimeMod(speed, players, noEffects);
            return;
        }
        auto* pl = s.playLayer();
        auto* r1 = (pl && pl->m_player1) ? pl->m_player1 : nullptr;
        auto* r2 = (pl && pl->m_player2) ? pl->m_player2 : nullptr;
        // m_speedMultiplier (double) is distinct from m_playerSpeed (float) on
        // PlayerObject. The audit (docs/sim-reference/01-playerobject.md, H4
        // in 11-leak-candidates.md) flagged it as a candidate for the
        // bookmarked speed-portal sim-side regression: super's inline write
        // path may set m_speedMultiplier rather than (or in addition to)
        // m_playerSpeed, and our previous TrajLayerSpeedHook only carried
        // m_playerSpeed/AC over to sim. If sim's physics integration consults
        // m_speedMultiplier, propagating only m_playerSpeed leaves sim's
        // movement speed unchanged across a portal crossing.
        float  prevPs1 = r1 ? r1->m_playerSpeed     : 0.f;
        float  prevAc1 = r1 ? r1->m_playerSpeedAC   : 0.f;
        double prevSm1 = r1 ? r1->m_speedMultiplier : 0.0;
        float  prevPs2 = r2 ? r2->m_playerSpeed     : 0.f;
        float  prevAc2 = r2 ? r2->m_playerSpeedAC   : 0.f;
        double prevSm2 = r2 ? r2->m_speedMultiplier : 0.0;

        GJBaseGameLayer::updateTimeMod(speed, players, noEffects);

        // Diagnostic data (2026-05-11) revealed:
        // - super is often invoked with players=false during the speed-mod
        //   trigger fire path → it does NOT propagate to per-player
        //   m_playerSpeed at all
        // - Even when fired, our previous read-from-real-then-copy-to-sim
        //   propagation copies the unchanged 1.1 → 1.1 (no effect)
        //
        // Workaround: explicitly invoke the per-player virtual on each sim
        // player AFTER super, passing the same speed arg. This hits the
        // engine's own PlayerObject::updateTimeMod (which our
        // TrajPlayerObjectHook above lets through for sim players), so the
        // sim's own m_playerSpeed gets written by the engine's actual speed
        // logic — bypassing the players=false gate. Real players are
        // unaffected (we don't call updateTimeMod on them; super may or may
        // not have, and we restore their speed values below regardless).
        auto* sp1 = s.simP1();
        auto* sp2 = s.simP2();
        if (sp1) sp1->updateTimeMod(speed, noEffects);
        if (sp2) sp2->updateTimeMod(speed, noEffects);

        // Diagnostic: dump speed-state snapshot when this hook fires during
        // sim. Budgeted to ~1 log per 60 hook fires so a level full of speed
        // portals doesn't tank perf. Records every speed-related field on
        // real and sim so we can see what super actually wrote and whether
        // propagation took effect.
        if (--s_speedHookLogBudget <= 0) {
            s_speedHookLogBudget = 60;
            geode::log::warn(
                "[speed-hook] arg_speed={:.4f} players={} | "
                "r1: ps {:.4f}→{:.4f} ac {:.4f}→{:.4f} sm {:.4f}→{:.4f} | "
                "sim1: ps={:.4f} ac={:.4f} sm={:.4f} pos.x={:.1f} | "
                "real1.pos.x={:.1f} | speedObjs.count={}",
                speed, players,
                prevPs1, r1 ? r1->m_playerSpeed : 0.f,
                prevAc1, r1 ? r1->m_playerSpeedAC : 0.f,
                prevSm1, r1 ? r1->m_speedMultiplier : 0.0,
                sp1 ? sp1->m_playerSpeed : 0.f,
                sp1 ? sp1->m_playerSpeedAC : 0.f,
                sp1 ? sp1->m_speedMultiplier : 0.0,
                sp1 ? sp1->getPositionX() : 0.f,
                r1 ? r1->getPositionX() : 0.f,
                pl && pl->m_speedObjects ? pl->m_speedObjects->count() : 0
            );
        }

        // Restore real player speeds — last so the propagation above reads
        // the engine-set (post-super) values, not the restored ones.
        if (r1) {
            r1->m_playerSpeed     = prevPs1;
            r1->m_playerSpeedAC   = prevAc1;
            r1->m_speedMultiplier = prevSm1;
        }
        if (r2) {
            r2->m_playerSpeed     = prevPs2;
            r2->m_playerSpeedAC   = prevAc2;
            r2->m_speedMultiplier = prevSm2;
        }
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
    // portal's "has the real player activated me?" state untouched. Speed
    // portals dispatch into GJBaseGameLayer::updateTimeMod(speed, players=true,
    // noEffects=true), which writes m_player1/m_player2->m_playerSpeed INLINE
    // — bypassing the PlayerObject::updateTimeMod virtual our existing
    // TrajPlayerObjectHook gates. Saving + restoring the real player speeds
    // across the super call neutralizes the inline write while still letting
    // the sim's PlayerObject::updateTimeMod virtual fire.
    static void saveRestoreRealPlayerSpeeds(auto&& body) {
        auto* pl = sim().playLayer();
        float ps1 = (pl && pl->m_player1) ? pl->m_player1->m_playerSpeed : 0.f;
        float ps2 = (pl && pl->m_player2) ? pl->m_player2->m_playerSpeed : 0.f;
        body();
        if (pl && pl->m_player1) pl->m_player1->m_playerSpeed = ps1;
        if (pl && pl->m_player2) pl->m_player2->m_playerSpeed = ps2;
    }

    // Diagnostic budget for the speed-effect-trigger probe; see TrajLayerSpeedHook.
    static inline int s_speedEffectLogBudget = 0;

    void triggerObject(GJBaseGameLayer* layer, int uniqueID,
                       gd::vector<int> const* remapKeys) {
        auto& s = sim();
        // Telemetry — log every fire (rate-limited). "sim" if we're in a sim
        // context, "real" otherwise; player_x is taken from the appropriate
        // primary player. This is what lets us tell whether sim is firing
        // move triggers or not.
        {
            auto* pl = s.playLayer();
            char const* who = s.isSimulating() ? "sim" : "real";
            auto* p = s.isSimulating() ? s.simP1() : (pl ? pl->m_player1 : nullptr);
            tel::triggerObject(static_cast<int>(m_objectType), who,
                               p ? p->getPositionX() : 0.f,
                               this->getPositionX());
        }
        if (s.isSimulating()) {
            bool const speedMod = isSpeedMod();
            // Speed-mod: always fire (sim physics needs the speed change).
            // Non-speed triggers: gated on wantsTriggers(). When enabled,
            // fire the trigger so sim physics reflects color/move/toggle/
            // spawn/etc. effects in its prediction. Side-effect leak is
            // prevented by the EffectManagerState snapshot/restore in
            // LayerStateSnapshot — see Trajectory.cpp. We still save/restore
            // m_activatedByPlayer1/2 here because those flags live on the
            // EffectGameObject itself, not in the effect-manager state, so
            // the snapshot can't catch them.
            if (!speedMod && !s.wantsTriggers()) return;
            bool prev1 = m_activatedByPlayer1;
            bool prev2 = m_activatedByPlayer2;
            if (speedMod) {
                // Diagnostic: log when a speed-mod trigger fires during sim. If
                // [speed-effect-trigger] appears but [speed-hook] (in
                // TrajLayerSpeedHook) does NOT, then super of triggerObject does
                // not route through GJBaseGameLayer::updateTimeMod — our
                // propagation never gets a chance to update sim's speed.
                if (--s_speedEffectLogBudget <= 0) {
                    s_speedEffectLogBudget = 60;
                    auto* pl = s.playLayer();
                    geode::log::warn(
                        "[speed-effect-trigger] type={} prev_act1={} prev_act2={} | "
                        "real1.pos.x={:.1f} speedObjs.count_pre={}",
                        static_cast<int>(m_speedModType), prev1, prev2,
                        pl && pl->m_player1 ? pl->m_player1->getPositionX() : 0.f,
                        pl && pl->m_speedObjects ? pl->m_speedObjects->count() : 0
                    );
                }
                saveRestoreRealPlayerSpeeds([&]{
                    EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
                });
            } else {
                EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
            }
            m_activatedByPlayer1 = prev1;
            m_activatedByPlayer2 = prev2;
            return;
        }
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
    }

    // Diagnostic budget for the triggerActivated speed-mod probe. Speed-mod
    // portals dispatch via triggerActivated (not triggerObject), so this is
    // the path that fires when sim crosses the portal.
    static inline int s_speedTrigActLogBudget = 0;

    void triggerActivated(float xPosition) {
        auto& s = sim();
        // Telemetry — log every fire (rate-limited). triggerActivated is the
        // path that fires when a player CROSSES the trigger's x threshold.
        // If this never fires with who=sim for a trigger we expect sim to
        // cross, that's the smoking gun for "engine's trigger fire pass uses
        // real player x, not sim's x".
        tel::triggerActivated(static_cast<int>(m_objectType),
                              s.isSimulating() ? "sim" : "real", xPosition);
        if (s.isSimulating()) {
            bool const speedMod = isSpeedMod();
            // Same gating rule as triggerObject: speed-mod always fires (sim
            // physics needs the speed change); non-speed triggers respect
            // wantsTriggers(). See the comment on triggerObject above for the
            // rationale on default-off.
            if (!speedMod && !s.wantsTriggers()) return;
            bool prev1 = m_activatedByPlayer1;
            bool prev2 = m_activatedByPlayer2;
            if (speedMod) {
                if (--s_speedTrigActLogBudget <= 0) {
                    s_speedTrigActLogBudget = 60;
                    auto* pl = s.playLayer();
                    auto* sp1 = s.simP1();
                    geode::log::warn(
                        "[speed-trigger-activated] type={} xPos={:.1f} "
                        "prev_act1={} prev_act2={} | "
                        "real1.pos.x={:.1f} sim1.pos.x={:.1f} | "
                        "real1.ps={:.4f} sim1.ps={:.4f} | "
                        "speedObjs.count_pre={}",
                        static_cast<int>(m_speedModType), xPosition,
                        prev1, prev2,
                        pl && pl->m_player1 ? pl->m_player1->getPositionX() : 0.f,
                        sp1 ? sp1->getPositionX() : 0.f,
                        pl && pl->m_player1 ? pl->m_player1->m_playerSpeed : 0.f,
                        sp1 ? sp1->m_playerSpeed : 0.f,
                        pl && pl->m_speedObjects ? pl->m_speedObjects->count() : 0
                    );
                }
                saveRestoreRealPlayerSpeeds([&]{
                    EffectGameObject::triggerActivated(xPosition);
                });
            } else {
                EffectGameObject::triggerActivated(xPosition);
            }
            m_activatedByPlayer1 = prev1;
            m_activatedByPlayer2 = prev2;
            if (speedMod && s_speedTrigActLogBudget == 60) {  // just logged the entry
                auto* pl = s.playLayer();
                auto* sp1 = s.simP1();
                geode::log::warn(
                    "[speed-trigger-activated POST] "
                    "real1.ps={:.4f} sim1.ps={:.4f} sim1.sm={:.4f} | "
                    "speedObjs.count_post={}",
                    pl && pl->m_player1 ? pl->m_player1->m_playerSpeed : 0.f,
                    sp1 ? sp1->m_playerSpeed : 0.f,
                    sp1 ? sp1->m_speedMultiplier : 0.0,
                    pl && pl->m_speedObjects ? pl->m_speedObjects->count() : 0
                );
            }
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
            // Spoof engine flags (m_activated + m_activatedByPlayer1/2) via
            // markActivated, then skip super so visual / audio side effects
            // don't fire for sim. The flags are restored to pre-sim values at
            // runPlan/runBranch end via clearActivated(). Without flag
            // spoofing, the engine's playerTouchedRing super (which checks
            // the flags directly in some code paths, not only through
            // hasBeenActivatedByPlayer) would treat every orb as if it had
            // never been activated, making the bot behave as if every orb
            // were multi-activate.
            s.markActivated(this, player);
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
