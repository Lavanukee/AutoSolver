# GJBaseGameLayer — sim-relevance audit

*GJBaseGameLayer is the base layer PlayLayer inherits from. Many of the engine's per-tick game logic methods (collisionCheckObjects, checkCollisions, flipGravity, updateTimeMod, triggers, camera tweens, particles, group spawns) live here. The mod's TrajBaseLayerHook in `src/Hooks.cpp` gates many of these on `isSimulating()`. Class declaration spans lines 7283-8125 of `_deps/bindings-src/bindings/2.208/GeometryDash.bro`.*

## Summary stats

- Total methods declared (virtual + non-virtual): 426 (60 virtual + 366 non-virtual)
- Total fields declared: 287 (incl. PADs)
- Methods needing isolation: ~70 (24 currently hooked; ~46 latent leak candidates)
- Fields needing snapshot/restore: ~25 (15 currently in LayerStateSnapshot; the rest are either covered by the POD-prefix memcpy on `m_gameState` or are uncaptured leak vectors)

## Methods table

### Virtual methods (line 7292–7345)

| Line | Name | Signature | Virtual? | Classification | Sim impact / notes |
|------|------|-----------|----------|----------------|-------------------|
| 7292 | update | (float dt) | yes | physics-required | The master tick. Sim drives the engine update for sim players via simulate() inside updateCamera; never call directly on the layer with sim isolation off. |
| 7293 | init | () -> bool | yes | engine-internal | One-shot setup. |
| 7294 | visit | () | yes | visual | Cocos draw traversal. Not called during sim. |
| 7295 | postUpdate | (float dt) | yes | engine-internal | inline; PlayLayer override is the one we care about. |
| 7296 | checkForEnd | () | yes | state-mutating-game | PlayLayer override fires win/end logic; would fire end-anim if sim crossed the end portal. Mitigated indirectly by `playEndAnimationToPos` suppression in PlayLayer. |
| 7297 | testTime | () | yes | engine-internal | inline. |
| 7298 | updateVerifyDamage | () | yes | engine-internal | inline. |
| 7299 | updateAttemptTime | (float) | yes | engine-internal | inline. |
| 7300 | updateVisibility | (float dt) | yes | state-mutating-layer | inline; updates m_visibleObjects. **NOT hooked** — leak candidate if super pulled during sim path. |
| 7301 | playerTookDamage | (PlayerObject*) | yes | trigger-dispatch | inline. Fires damage gameEvent. Sim's destroyPlayer is gated; this routes there. |
| 7302 | opacityForObject | (GameObject*) -> float | yes | read-only | getter for object opacity. |
| 7303 | addToSpeedObjects | (EffectGameObject*) | yes | state-mutating-layer | inline; appends to m_speedObjects. Sim DOES need this for speed-mod sim physics — captured by LayerStateSnapshot `speedObjects` and rolled back. |
| 7304 | objectsCollided | (int blockA, int blockB) | yes | trigger-dispatch | Sets m_unkVecFloat1 via objectsCollided event. **NOT hooked**. |
| 7305 | updateColor | (color, fadeTime, colorID, blending, opacity, hsv, ...) | yes | state-mutating-layer | Writes m_effectManager color state. **NOT hooked** — color triggers from sim would leak. (Mitigated only because EffectGameObject::triggerObject is gated; color triggers ARE EffectGameObject subclasses.) |
| 7306 | toggleGroupTriggered | (group, activate, remapKeys, triggerID, controlID) | yes | trigger-dispatch | The toggle trigger fanout. **NOT hooked** — relies on EffectGameObject gating. |
| 7307 | spawnGroup | (group, ordered, delay, remapKeys, triggerID, controlID) | yes | trigger-dispatch | Spawn trigger fanout. **NOT hooked** — relies on EffectGameObject gating. |
| 7308 | spawnObject | (GameObject*, delay, remapKeys) | yes | trigger-dispatch | Single-object spawn path. **NOT hooked**. |
| 7309 | activateEndTrigger | (targetID, reverse, lockPlayerY) | yes | trigger-dispatch | inline; end trigger handling. Sim crossing end trigger would fire it. **NOT hooked** — leak candidate. |
| 7310 | activatePlatformerEndTrigger | (EndTriggerGameObject*, remapKeys) | yes | trigger-dispatch | inline. **NOT hooked**. |
| 7311 | toggleGlitter | (bool) | yes | visual | inline. |
| 7312 | destroyPlayer | (PlayerObject*, GameObject*) | yes | physics-conditional | inline; PlayLayer override is hooked in TrajPlayLayerHook. |
| 7313 | updateDebugDraw | () | yes | visual | Debug viz; harmless. |
| 7314 | addToSection | (GameObject*) | yes | state-mutating-layer | Writes m_sections / m_nonEffectObjects. **NOT hooked** — would corrupt engine bookkeeping if invoked from sim. |
| 7315 | addToGroup | (GameObject*, groupID, triggerGroup) | yes | state-mutating-layer | Writes m_groupDict / m_groups. **NOT hooked**. |
| 7316 | removeFromGroup | (GameObject*, groupID) | yes | state-mutating-layer | Removes from m_groups. **NOT hooked**. |
| 7317 | updateObjectSection | (GameObject*) | yes | state-mutating-layer | Re-sectionizes object. **NOT hooked**. |
| 7318 | updateDisabledObjectsLastPos | (CCArray*) | yes | state-mutating-layer | inline. |
| 7319 | toggleGroundVisibility | (bool) | yes | visual | Hides/shows m_groundLayer. **NOT hooked**. Sim-crossed visibility trigger would flicker real ground. |
| 7320 | toggleMGVisibility | (bool) | yes | visual | Same for middleground. **NOT hooked**. |
| 7321 | toggleHideAttempts | (bool) | yes | visual | UI toggle. |
| 7322 | timeForPos | (pos, order, channel, songTriggers, id) | yes | read-only | inline. |
| 7323 | posForTime | (time) | yes | read-only | inline. |
| 7324 | resetSPTriggered | () | yes | state-mutating-layer | inline; resets startpos flags. |
| 7325 | updateScreenRotation | (rot, add, convert, duration, easing, rate, uid, cid) | yes | visual | **HOOKED** in TrajBaseLayerHook (suppress for sim) — line 251-256. |
| 7326 | reverseDirection | (EffectGameObject*) | yes | trigger-dispatch | Direction trigger. **NOT hooked** — relies on EffectGameObject gating. |
| 7327 | rotateGameplay | (RotateGameplayGameObject*) | yes | state-mutating-layer | Toggles m_levelFlipping etc. **NOT hooked** — relies on EffectGameObject gating; gameplay-area rotation state leak risk. |
| 7328 | didRotateGameplay | () | yes | engine-internal | inline post-rotate callback. |
| 7329 | updateTimeWarp | (timeWarp) | yes | state-mutating-layer | Writes m_gameState.m_timeWarp/m_queuedTimeWarp. Captured by LayerStateSnapshot POD prefix + explicit `timeWarp`/`queuedTimeWarp` fields. |
| 7330 | applyTimeWarp | (timeWarp) | yes | state-mutating-layer | Companion. Same coverage. |
| 7331 | playGravityEffect | (bool flip) | yes | visual | inline (in base). **HOOKED via PlayLayer override** — TrajPlayLayerHook::playGravityEffect line 69-72 (because inline can't be hooked at base level). |
| 7332 | manualUpdateObjectColors | (GameObject*) | yes | visual | inline. |
| 7333 | createCustomParticle | (key, struct, min, dontAdd) | yes | visual | Adds CCParticleSystemQuad to scene. **NOT hooked** — particle leak vector if invoked during sim. Indirectly mitigated by spawnParticle suppression but THIS function is a separate path. |
| 7334 | claimCustomParticle | (key, struct, zLayer, zOrder, uiObject, dontAdd) | yes | visual | Same. **NOT hooked**. |
| 7335 | unclaimCustomParticle | (key, particle) | yes | visual | Returns particle to pool. **NOT hooked**. |
| 7336 | activatedAudioTrigger | (SFXTriggerGameObject*) | yes | audio | SFX trigger entry point. **NOT hooked** — audio leak vector if sim crosses SFX trigger. |
| 7337 | checkpointActivated | (CheckpointGameObject*) | yes | state-mutating-game | Adds checkpoint to PlayLayer state. **NOT hooked** — sim hitting checkpoint object would persist. (Checkpoints typically only fire on actual practice-mode death so this may be cold in practice.) |
| 7338 | flipArt | (bool) | yes | visual | inline; flips visual art. |
| 7339 | addKeyframe | (KeyframeGameObject*) | yes | state-mutating-layer | Adds to m_keyframeGroups. **NOT hooked**. |
| 7340 | updateTimeLabel | (sec, csec, decimals) | yes | visual | UI label. inline. |
| 7341 | checkSnapshot | () | yes | engine-internal | inline. |
| 7342 | toggleProgressbar | () | yes | visual | inline. |
| 7343 | toggleInfoLabel | () | yes | visual | inline. |
| 7344 | removeAllCheckpoints | () | yes | state-mutating-game | inline. |
| 7345 | toggleMusicInPractice | () | yes | audio | inline. |

### Non-virtual methods (line 7347–7714)

| Line | Name | Signature | Virtual? | Classification | Sim impact / notes |
|------|------|-----------|----------|----------------|-------------------|
| 7347 | activateCustomRing | (RingObject*) | no | trigger-dispatch | Custom-ring activation. **NOT hooked** — ring activation is gated via TrajOrbsLayerHook::playerTouchedRing in Orbs.cpp, but THIS path is separate (e.g. ring custom-trigger). Leak candidate. |
| 7348 | activatedAudioTrigger | (obj, levelTime) overload | no | audio | **NOT hooked**. |
| 7349 | activateEventTrigger | (EventLinkTrigger*, remapKeys) | no | trigger-dispatch | Event link trigger. **NOT hooked** — relies on EffectGameObject gating (EventLinkTrigger inherits from it). |
| 7350 | activateItemCompareTrigger | (ItemTriggerGameObject*, remapKeys) | no | trigger-dispatch | Item compare. **NOT hooked**. |
| 7351 | activateItemEditTrigger | (ItemTriggerGameObject*) | no | trigger-dispatch | Item edit. **NOT hooked**. |
| 7352 | activateObjectControlTrigger | (ObjectControlGameObject*) | no | trigger-dispatch | Object control. **NOT hooked**. |
| 7353 | activatePersistentItemTrigger | (ItemTriggerGameObject*) | no | trigger-dispatch | **NOT hooked**. |
| 7354 | activatePlayerControlTrigger | (PlayerControlGameObject*) | no | trigger-dispatch | Lock/disable player trigger. **NOT hooked** — could lock real player if sim crosses. |
| 7355 | activateResetTrigger | (EffectGameObject*) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7356 | activateSFXEditTrigger | (SFXTriggerGameObject*) | no | audio | **NOT hooked**. |
| 7357 | activateSFXTrigger | (SFXTriggerGameObject*) | no | audio | **NOT hooked** — sim crossing SFX would play audio on real. |
| 7358 | activateSongEditTrigger | (SongTriggerGameObject*) | no | audio | **NOT hooked**. |
| 7359 | activateSongTrigger | (SongTriggerGameObject*) | no | audio | **NOT hooked** — could swap song globally. |
| 7360 | activateTimerTrigger | (TimerTriggerGameObject*, remapKeys) | no | trigger-dispatch | Timer trigger. **NOT hooked**. |
| 7361 | addAreaEffect | (obj, instances, type) | no | state-mutating-layer | Adds to m_areaObjects. **NOT hooked**. |
| 7362 | addCustomEnterEffect | (obj, enter) | no | state-mutating-layer | **NOT hooked**. |
| 7363 | addGuideArt | (obj) -> bool | no | visual | Editor-only typically. |
| 7364 | addObjectCounter | (LabelGameObject*) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7365 | addPickupTrigger | (CountTriggerGameObject*) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7366 | addPoints | (int) | no | state-mutating-game | inline. **NOT hooked** — sim adding points would persist (score, item count). |
| 7367 | addProximityVolumeEffect | (channelID, targetType, obj) | no | audio | **NOT hooked**. |
| 7368 | addRemapTargets | (set<int>&) | no | state-mutating-layer | Mutates remap targets. |
| 7369 | addToGroupParents | (GameObject*) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7370 | addToGroups | (GameObject*, duplicateTrigger) | no | state-mutating-layer | **NOT hooked**. |
| 7371 | addToObjectsToShow | (GameObject*) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7372 | addUIObject | (GameObject*) | no | state-mutating-layer | Mutates m_uiObjects. **NOT hooked**. |
| 7373 | animateInDualGroundNew | (obj, height, instant, duration) | no | visual | Mode-switch ground animation. **NOT hooked** — empirically, suppressing breaks sim ball physics; LayerStateSnapshot captures groundLayer positions + stopAllActions instead (Trajectory.cpp:80-86, 205-216). |
| 7374 | animateInGroundNew | (unk1, unk2, unk3) | no | visual | Same. Same mitigation. |
| 7375 | animateOutGroundNew | (instant) | no | visual | Same. Same mitigation. |
| 7376 | animatePortalY | (fromY, toY, duration, easingRate) | no | visual | Mode portal Y tween. **HOOKED** in TrajBaseLayerHook line 322-325 (suppress for sim). |
| 7377 | applyLevelSettings | (GameObject*) | no | engine-internal | inline. |
| 7378 | applyRemap | (obj, remapKeys, remap) | no | engine-internal | Used internally by trigger fanout. |
| 7379 | applySFXEditTrigger | (channelID, targetType, obj) | no | audio | **NOT hooked**. |
| 7380 | applyShake | (CCPoint&) | no | visual | inline; called from updateCamera. Sim suppresses via shakeCamera gate. |
| 7381 | assignNewStickyGroups | (CCArray*) | no | engine-internal | Cold path. |
| 7382 | asyncBGLoaded | (int background) | no | engine-internal | inline; async loader callback. |
| 7383 | asyncGLoaded | (int ground) | no | engine-internal | inline. |
| 7384 | asyncMGLoaded | (int middleground) | no | engine-internal | inline. |
| 7385 | atlasValue | (int) -> int | no | read-only | inline. |
| 7386 | bumpPlayer | (PlayerObject*, EffectGameObject*) | no | physics-required | Bump pad-like effect. Sim needs the velocity change; route is sim-player gated upstream. |
| 7387 | buttonIDToButton | (int) -> int | no | read-only | inline. |
| 7388 | calculateColorGroups | () | no | engine-internal | One-shot setup. |
| 7389 | cameraMoveX | (value, duration, rate, unused) | no | visual | **HOOKED** in TrajBaseLayerHook line 337-340 (suppress for sim). |
| 7390 | cameraMoveY | (value, duration, rate, force) | no | visual | **HOOKED** line 341-344. |
| 7391 | canBeActivatedByPlayer | (PlayerObject*, EffectGameObject*) -> bool | no | physics-conditional | **HOOKED** line 203-207 (true for sim-player, otherwise super). |
| 7392 | canProcessSFX | (state, indices, times, states) -> bool | no | audio | **NOT hooked** — engine-internal SFX guard. |
| 7393 | canTouchObject | (GameObject*) -> bool | no | read-only | inline. |
| 7394 | checkCameraLimitAfterTeleport | (PlayerObject*, yOffset) | no | state-mutating-layer | **HOOKED** in TrajPortalsLayerHook (Portals.cpp:23-26) — skip if sim player so camera limits don't shift for real. |
| 7395 | checkCollision | (blockA, blockB) -> bool | no | physics-required | Block-vs-block collision check helper. Sim uses indirectly via collisionCheckObjects. |
| 7396 | checkCollisionBlocks | (obj, blocks, count) | no | physics-required | Block-vs-block resolve. Sim uses. |
| 7397 | checkCollisions | (PlayerObject*, dt, ignoreDamage) -> int | no | physics-required | Master player collision check. Sim NEEDS this for trajectory accuracy. |
| 7398 | checkRepellPlayer | () | no | physics-required | Repel logic; harmless. |
| 7399 | checkSpawnObjects | () | no | trigger-dispatch | Drains queued spawn objects. **NOT hooked** — if sim queues a spawn it'd materialize next real tick. |
| 7400 | claimMoveAction | (groupID, ignoreStaticGroups) -> CCPoint | no | state-mutating-layer | **NOT hooked**. |
| 7401 | claimParticle | (key, zLayer) -> CCParticleSystemQuad* | no | visual | Particle factory. **NOT hooked** — leak vector if sim invokes. |
| 7402 | claimRotationAction | (targetID, centerID, rotation, offset, ignoreStaticGroups, unused) | no | state-mutating-layer | **NOT hooked**. |
| 7403 | clearActivatedAudioTriggers | () | no | audio | inline. |
| 7404 | clearPickedUpItems | () | no | state-mutating-game | inline. |
| 7405 | collectedObject | (EffectGameObject*) | no | coin-collect | inline. **NOT hooked** — coins are filtered out at collisionCheckObjects (Hooks.cpp:177), so this should never fire for sim. |
| 7406 | collisionCheckObjects | (PlayerObject*, vec, count, dt) | no | physics-required | **HOOKED** in TrajBaseLayerHook line 127-191. Filters cull window + sim-destroyed + pads/orbs/portals/coins for sim. |
| 7407 | controlAdvancedFollowCommand | (obj, controlID, command) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7408 | controlAreaEffect | (obj, instances, command) | no | trigger-dispatch | **NOT hooked**. |
| 7409 | controlAreaEffectWithID | (uid, controlID, command) | no | trigger-dispatch | **NOT hooked**. |
| 7410 | controlDynamicCommand | (obj, controlID, actions, command) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7411 | controlDynamicMoveCommand | (obj, controlID, command) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7412 | controlDynamicRotateCommand | (obj, controlID, command) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7413 | controlEventLink | (uid, controlID, command) | no | trigger-dispatch | **NOT hooked**. |
| 7414 | controlGradientTrigger | (obj, command) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7415 | controlTriggersInGroup | (group, command) | no | trigger-dispatch | **NOT hooked**. |
| 7416 | controlTriggersWithControlID | (controlID, command) | no | trigger-dispatch | **NOT hooked**. |
| 7417 | createBackground | (int) | no | engine-internal | Level setup. |
| 7418 | createGroundLayer | (ground, line) | no | engine-internal | Level setup. |
| 7419 | createMiddleground | (int) | no | engine-internal | Level setup. |
| 7420 | createNewKeyframeAnim | () -> CCArray* | no | state-mutating-layer | inline. **NOT hooked**. |
| 7421 | createParticle | (objectType, plistName, tag, posType) -> CCParticleSystemQuad* | no | visual | **NOT hooked** — particle leak vector. |
| 7422 | createPlayer | () | no | engine-internal | One-shot setup. |
| 7423 | createPlayerCollisionBlock | () | no | engine-internal | One-shot setup. |
| 7424 | createTextLayers | () | no | engine-internal | One-shot setup. |
| 7425 | damagingObjectsInRect | (rect, enabledGroups) -> CCArray* | no | read-only | Spatial query. |
| 7426 | destroyObject | (GameObject*) | no | state-mutating-layer | **HOOKED** in TrajBaseLayerHook line 118-125. For sim, marks sim-destroyed in per-run set, doesn't touch engine state. |
| 7427 | enterDualMode | (obj, unused) | no | state-mutating-layer | inline. Dual-mode transitions go through toggleDualMode which is hooked in Portals.cpp. |
| 7428 | exitStaticCamera | (exitX, exitY, time, easing, rate, smoothVelocity, mod, exitInstant) | no | visual | Static camera CCAction. **NOT hooked** — camera tween leak candidate. |
| 7429 | flipFinished | () | no | engine-internal | inline. |
| 7430 | flipGravity | (PlayerObject*, flip, noEffects) | no | physics-conditional | **HOOKED** in TrajBaseLayerHook line 193-201 — for sim, only fires for sim-player + force noEffects=true. |
| 7431 | flipObjects | () | no | visual | Visual flip of static objects. **NOT hooked** — flipObjects-during-sim could mirror art. |
| 7432 | gameEventTriggered | (event, material, playerID) | no | trigger-dispatch | Generic event dispatch. **NOT hooked** — sim-triggered game events may fan out to triggers. |
| 7433 | generateEnterEasingBuffer | (easing, rate) -> int | no | state-mutating-layer | Adds entry to m_enterEasingValues. **NOT hooked**. |
| 7434 | generateEnterEasingBuffers | (obj) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7435 | generatePickupAnimRandVal | (obj, r1, r2) | no | visual | inline. |
| 7436 | generateSpawnRemap | () | no | engine-internal | One-shot. |
| 7437 | generateTargetGroups | () | no | engine-internal | One-shot. |
| 7438 | generateVisibilityGroups | () | no | engine-internal | One-shot. |
| 7439 | getActiveOrderSpawnObjects | () -> CCArray* | no | read-only | inline. |
| 7440 | getAreaObjectValue | (instance, obj, pos, show) -> float | no | read-only | |
| 7441 | getBumpMod | (player, type) -> float | no | read-only | inline. |
| 7442 | getCameraEdgeValue | (type) -> float | no | read-only | inline. |
| 7443 | getCapacityString | () -> string | no | read-only | inline. |
| 7444 | getCenterGroupObject | (groupID, defaultID) -> GameObject* | no | read-only | inline. |
| 7445 | getCustomEnterEffects | (id, enter) -> vector* | no | read-only | inline. |
| 7446 | getEasedAreaValue | (obj, instance, value, show, idx) -> float | no | read-only | |
| 7447 | getEnterEasingKey | (easing, rate) -> int | no | read-only | inline. |
| 7448 | getEnterEasingValue | (value, easing, rate, buffer) -> float | no | read-only | inline. |
| 7449 | getFollowSpeedVal | (obj, startSpeedRef, startDirRef, startDir, startSpeed) -> CCPoint | no | read-only | |
| 7450 | getGroundHeight | (player, type) -> float | no | read-only | inline. Sim uses for ground resolution. |
| 7451 | getGroundHeightForMode | (type) -> float | no | read-only | |
| 7452 | getGroup | (int id) -> CCArray* | no | read-only | |
| 7453 | getGroupParent | (groupId) -> GameObject* | no | read-only | inline. |
| 7454 | getGroupParentsString | (obj) -> string | no | read-only | inline. |
| 7455 | getItemValue | (type, id) -> double | no | read-only | |
| 7456 | getMaxPortalY | () -> float | no | read-only | |
| 7457 | getMinDistance | (pos, objects, minNear, mode) -> float | no | read-only | |
| 7458 | getMinPortalY | () -> float | no | read-only | |
| 7459 | getModifiedDelta | (dt) -> float | no | read-only | |
| 7460 | getMoveTargetDelta | (obj, mainObject) -> CCPoint | no | read-only | inline. |
| 7461 | getOptimizedGroup | (groupID) -> CCArray* | no | read-only | inline. |
| 7462 | getOtherPlayer | (player) -> PlayerObject* | no | read-only | inline. |
| 7463 | getParticleKey | (objectType, plistName, tag, posType) -> string | no | read-only | |
| 7464 | getParticleKey2 | (key) -> string | no | read-only | |
| 7465 | getPlayerButtonID | (button, p2) -> int | no | read-only | inline. |
| 7466 | getPlayTimerFullSeconds | () -> int | no | read-only | inline. |
| 7467 | getPlayTimerMilli | () -> int | no | read-only | inline. |
| 7468 | getPortalTarget | (TeleportPortalObject*) -> TeleportPortalObject* | no | read-only | inline. |
| 7469 | getPortalTargetPos | (portal, target, player) -> CCPoint | no | read-only | inline. |
| 7470 | getRecordString | () -> string | no | read-only | |
| 7471 | getRotateCommandTargets | (obj, centerObj, targetObj, rotateObj) | no | read-only | inline. |
| 7472 | getSavedPosition | (groupID, delay) -> CCPoint | no | read-only | |
| 7473 | getScaledGroundHeight | (height) -> float | no | read-only | inline. |
| 7474 | getSingleGroupObject | (groupID) -> GameObject* | no | read-only | inline. |
| 7475 | getSpecialKey | (groupID, ignoreGroupParent, ignoreLinkedObjects) -> int | no | read-only | inline. |
| 7476 | getStaticGroup | (groupID) -> CCArray* | no | read-only | inline. |
| 7477 | getStickyGroup | (group) -> CCArray* | no | read-only | inline. |
| 7478 | getTargetFlyCameraY | (obj) -> float | no | read-only | inline. |
| 7479 | getTargetGroup | (idx, uid) -> CCArray* | no | read-only | inline. |
| 7480 | getTargetGroupOrigin | (idx, uid) -> CCArray* | no | read-only | inline. |
| 7481 | gravBumpPlayer | (player, obj) | no | physics-required | inline. Pad-like effect. |
| 7482 | groupStickyObjects | (CCArray*) | no | engine-internal | Cold path. |
| 7483 | handleButton | (down, button, isPlayer1) | no | physics-required | **HOOKED** in TrajBaseLayerHook line 94-97 (real-button recording for sim) and BotHooks.cpp:76 (bot's own button drain). |
| 7484 | hasItem | (id) -> bool | no | read-only | inline. |
| 7485 | hasUniqueCoin | (EffectGameObject*) -> bool | no | read-only | Unique coin presence check. |
| 7486 | increaseBatchNodeCapacity | () | no | engine-internal | One-shot. |
| 7487 | isFlipping | () -> bool | no | read-only | inline. |
| 7488 | isPlayer2Button | (button) -> bool | no | read-only | inline. |
| 7489 | lightningFlash | (to, color) overload | no | visual | **HOOKED** line 286-289. |
| 7490 | lightningFlash | (from, to, color, lineWidth, duration, displacement, flash, opacity) | no | visual | **HOOKED** line 290-296. |
| 7491 | loadGroupParentsFromString | (obj, groupList) | no | engine-internal | One-shot. |
| 7492 | loadLevelSettings | () | no | engine-internal | One-shot. |
| 7493 | loadStartPosObject | () | no | engine-internal | One-shot. |
| 7494 | loadUpToPosition | (pos, order, channel) | no | engine-internal | Pre-tick warmup. |
| 7495 | maxZOrderForShaderZ | (zLayer) -> int | no | read-only | |
| 7496 | minZOrderForShaderZ | (zLayer) -> int | no | read-only | |
| 7497 | modifyGroupPhysics | (obj, group) | no | state-mutating-layer | **NOT hooked**. |
| 7498 | modifyObjectPhysics | (obj, physics) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7499 | moveAreaObject | (obj, dx, dy) | no | state-mutating-layer | **NOT hooked**. |
| 7500 | moveCameraToPos | (pos) | no | visual | **HOOKED** line 247-250. |
| 7501 | moveObject | (obj, dx, dy, lockY) | no | state-mutating-layer | inline. **NOT hooked** — move trigger fanout target. |
| 7502 | moveObjects | (objs, dx, dy, lockY) | no | state-mutating-layer | **NOT hooked** — sim crossing a move trigger would translate objects globally. Relies on EffectGameObject gating. |
| 7503 | moveObjectsSilent | (groupId, dx, dy) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7504 | moveObjectToStaticGroup | (obj) | no | state-mutating-layer | **NOT hooked**. |
| 7505 | objectIntersectsCircle | (obj, circle) -> bool | no | read-only | inline. |
| 7506 | objectTypeToGameEvent | (type) -> GJGameEvent | no | read-only | |
| 7507 | optimizeMoveGroups | () | no | engine-internal | One-shot. |
| 7508 | orderSpawnObjects | () | no | engine-internal | |
| 7509 | parentForZLayer | (zLayer, blending, parentMode, uiObject) -> CCNode* | no | read-only | |
| 7510 | pauseAudio | () | no | audio | **NOT hooked** — sim shouldn't pause real audio (low probability of being hit from sim). |
| 7511 | performMathOperation | (op1, op2, op) -> double | no | read-only | inline. |
| 7512 | performMathRounding | (value, type) -> double | no | read-only | inline. |
| 7513 | pickupItem | (EffectGameObject*) | no | coin-collect | **NOT hooked** — coins filtered at collisionCheckObjects. (Belt-and-suspenders hook recommended.) |
| 7514 | playAnimationCommand | (id, groupID) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7515 | playerCircleCollision | (player, obj) -> bool | no | physics-required | Circle-vs-player collision. |
| 7516 | playerIntersectsCircle | (player, obj) -> bool | no | read-only | inline. |
| 7517 | playerTouchedObject | (player, obj) | no | physics-required | inline; entry to object touch dispatch. Sim uses. |
| 7518 | playerTouchedRing | (player, RingObject*) | no | physics-conditional | **HOOKED** in TrajOrbsLayerHook (Orbs.cpp:15-19) — only fires for sim player; suppressed for real if sim is running. |
| 7519 | playerTouchedTrigger | (player, EffectGameObject*) | no | physics-conditional | **NOT hooked** — trigger touch dispatch. EffectGameObject::triggerObject is hooked downstream so this is partially safe but a defensive hook would be cleaner. |
| 7520 | playerWasTouchingObject | (player, obj) -> bool | no | read-only | inline. |
| 7521 | playerWillSwitchMode | (player, obj) | no | state-mutating-layer | Mode-switch precondition. |
| 7522 | playExitDualEffect | (player) | no | visual | **NOT hooked** — visual flash when exiting dual mode. Leak candidate if sim crosses dual exit. |
| 7523 | playFlashEffect | (duration, flashes, unk) | no | visual | **HOOKED** line 259-262. |
| 7524 | playKeyframeAnimation | (obj, remapKeys) | no | trigger-dispatch | **NOT hooked**. |
| 7525 | playSpeedParticle | (timeMod) | no | visual | **HOOKED** line 301-304. |
| 7526 | positionForShaderTarget | (groupID) -> CCPoint | no | read-only | |
| 7527 | positionUIObjects | () | no | engine-internal | |
| 7528 | prepareSavePositionObjects | () | no | engine-internal | |
| 7529 | prepareTransformParent | (rotateNode) | no | engine-internal | inline. |
| 7530 | preResumeGame | () | no | engine-internal | inline. |
| 7531 | preUpdateVisibility | (dt) | no | state-mutating-layer | Updates visibility groups. **NOT hooked**. |
| 7532 | processActivatedAudioTriggers | (levelTime) | no | audio | **NOT hooked** — drains the per-tick SFX queue. |
| 7533 | processAdvancedFollowAction | (instance, started, dt) | no | trigger-dispatch | **NOT hooked**. |
| 7534 | processAdvancedFollowActions | (dt) | no | trigger-dispatch | **NOT hooked** — runs follow actions globally. |
| 7535 | processAreaActions | (dt, visibleFrame) | no | trigger-dispatch | **NOT hooked**. |
| 7536 | processAreaEffects | (effects, type, dt, visibleFrame) | no | trigger-dispatch | **NOT hooked**. |
| 7537 | processAreaFadeGroupAction | (objs, instance, pos, targetGroups) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7538 | processAreaMoveGroupAction | (objs, instance, pos, ...) | no | trigger-dispatch | **NOT hooked**. |
| 7539 | processAreaRotateGroupAction | (objs, instance, pos, ...) | no | trigger-dispatch | **NOT hooked**. |
| 7540 | processAreaTintGroupAction | (objs, instance, pos, targetGroups) | no | trigger-dispatch | **NOT hooked**. |
| 7541 | processAreaTransformGroupAction | (objs, instance, pos, ...) | no | trigger-dispatch | **NOT hooked**. |
| 7542 | processAreaVisualActions | (dt) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7543 | processCameraObject | (obj, player) -> GameObject* | no | read-only | inline. |
| 7544 | processCommands | (dt) | no | trigger-dispatch | The per-tick command-action drain. **NOT hooked** — and this is the engine's main fanout for queued commands. Sim queueing one would fire it next real tick. |
| 7545 | processDynamicObjectActions | (type, dt) | no | trigger-dispatch | **NOT hooked**. |
| 7546 | processFollowActions | () | no | trigger-dispatch | **NOT hooked**. |
| 7547 | processItems | () | no | trigger-dispatch | Item pickup fanout. **NOT hooked**. |
| 7548 | processMoveActions | () | no | trigger-dispatch | **NOT hooked**. |
| 7549 | processMoveActionsStep | (dt, visibleFrame) | no | trigger-dispatch | **NOT hooked**. |
| 7550 | processOptionsTrigger | (GameOptionsTrigger*) | no | state-mutating-game | **NOT hooked** — could disable real player, set options globally. |
| 7551 | processPlayerFollowActions | (dt) | no | trigger-dispatch | **NOT hooked**. |
| 7552 | processQueuedAudioTriggers | () | no | audio | **NOT hooked** — drains audio queue. |
| 7553 | processQueuedButtons | (p1, p2) | no | physics-required | Drains m_queuedButtons. Sim invokes via simulate() path. |
| 7554 | processRotationActions | () | no | trigger-dispatch | **NOT hooked**. |
| 7555 | processSFXObjects | () | no | audio | **NOT hooked**. |
| 7556 | processSFXState | (s1, s2, type, levelTime) | no | audio | **NOT hooked**. |
| 7557 | processSongState | (musicID, delay, levelTime, offset, vol, speed, states, obj) -> bool | no | audio | **NOT hooked**. |
| 7558 | processStateObjects | () | no | trigger-dispatch | inline. **NOT hooked**. |
| 7559 | processTransformActions | (visibleFrame) | no | trigger-dispatch | **NOT hooked**. |
| 7560 | queueButton | (button, push, p2) | no | physics-required | inline. Sim records via handleButton hook. |
| 7561 | reAddToStickyGroup | (obj) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7562 | recordAction | (button, down, p2) | no | state-mutating-game | inline; writes to m_recordString. **NOT hooked** — sim button presses being recorded would corrupt the replay. |
| 7563 | rectIntersectsCircle | (rect, center, radius) -> bool | no | read-only | inline. |
| 7564 | refreshCounterLabels | () | no | visual | UI labels. |
| 7565 | refreshKeyframeAnims | () | no | engine-internal | |
| 7566 | regenerateEnterEasingBuffers | () | no | engine-internal | inline. |
| 7567 | registerSpawnRemap | (vector<ChanceObject>&) -> int | no | engine-internal | inline. |
| 7568 | registerStateObject | (EffectGameObject*) | no | state-mutating-layer | inline. |
| 7569 | removeBackground | () | no | engine-internal | inline. |
| 7570 | removeCustomEnterEffects | (id, enter) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7571 | removeFromGroupParents | (obj) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7572 | removeFromGroups | (obj) | no | state-mutating-layer | **NOT hooked**. |
| 7573 | removeFromStickyGroup | (obj) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7574 | removeGroundLayer | () | no | engine-internal | inline. |
| 7575 | removeGroupParent | (groupID) | no | state-mutating-layer | **NOT hooked**. |
| 7576 | removeKeyframe | (KeyframeGameObject*) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7577 | removeMiddleground | () | no | engine-internal | inline. |
| 7578 | removeObjectFromSection | (obj) | no | state-mutating-layer | **NOT hooked**. |
| 7579 | removePlayer2 | () | no | state-mutating-layer | inline. |
| 7580 | removeTemporaryParticles | () | no | visual | inline. |
| 7581 | reorderObjectSection | (obj) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7582 | reparentObject | (node, parent) | no | engine-internal | inline. |
| 7583 | resetActiveEnterEffects | () | no | state-mutating-layer | |
| 7584 | resetAreaObjectValues | (obj, update) -> bool | no | state-mutating-layer | |
| 7585 | resetAudio | () | no | audio | **NOT hooked**. |
| 7586 | resetCamera | () | no | state-mutating-layer | |
| 7587 | resetGradientLayers | () | no | state-mutating-layer | |
| 7588 | resetGroupCounters | (reset) | no | state-mutating-layer | inline. |
| 7589 | resetLevelVariables | () | no | state-mutating-game | Called at level reset; OK. |
| 7590 | resetMoveOptimizedValue | () | no | state-mutating-layer | inline. |
| 7591 | resetPlayer | () | no | state-mutating-layer | |
| 7592 | resetSongTriggerValues | () | no | audio | inline. |
| 7593 | resetSpawnChannelIndex | () | no | state-mutating-layer | |
| 7594 | resetStaticCamera | (resetX, resetY) | no | state-mutating-layer | |
| 7595 | resetStoppedAreaObjects | () | no | state-mutating-layer | inline. |
| 7596 | restoreAllUIObjects | () | no | state-mutating-layer | inline. |
| 7597 | restoreDefaultGameplayOffsetX | () | no | state-mutating-layer | inline. |
| 7598 | restoreDefaultGameplayOffsetY | () | no | state-mutating-layer | inline. |
| 7599 | restoreRemap | (obj, remap) | no | engine-internal | |
| 7600 | resumeAudio | () | no | audio | **NOT hooked**. |
| 7601 | rotateAreaObjects | (obj, objs, rotation, reset) | no | state-mutating-layer | |
| 7602 | rotateObject | (obj, rotation) | no | state-mutating-layer | **NOT hooked**. |
| 7603 | rotateObjects | (objs, rotation, pos, offset, finished, unused) | no | state-mutating-layer | **NOT hooked** — rotate trigger fanout target. |
| 7604 | setGroupParent | (obj, groupID) | no | state-mutating-layer | **NOT hooked**. |
| 7605 | setStartPosObject | (StartPosObject*) | no | engine-internal | inline. |
| 7606 | setupLayers | () | no | engine-internal | One-shot. |
| 7607 | setupLevelStart | (LevelSettingsObject*) | no | engine-internal | One-shot. |
| 7608 | setupReplay | (inputs) | no | engine-internal | |
| 7609 | shakeCamera | (duration, strength, interval) | no | visual | **HOOKED** line 243-246. |
| 7610 | shouldExitHackedLevel | () -> bool | no | read-only | |
| 7611 | sortAllGroupsX | () | no | engine-internal | inline. |
| 7612 | sortGroups | () | no | engine-internal | |
| 7613 | sortSectionVector | () | no | engine-internal | |
| 7614 | sortStickyGroups | () | no | engine-internal | |
| 7615 | spawnGroupTriggered | (targetID, delay, ordered, remapKeys, uid, cid) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7616 | spawnObjectsInOrder | (objs, delay, remapKeys, uid, cid) | no | trigger-dispatch | **NOT hooked**. |
| 7617 | spawnParticle | (plist, zOrder, posType, position) -> CCParticleSystemQuad* | no | visual | **HOOKED** line 271-276 (return nullptr for sim). |
| 7618 | spawnParticleTrigger | (SpawnParticleGameObject*) | no | visual | **HOOKED** line 277-280. |
| 7619 | spawnParticleTrigger | (particleID, pos, rot, scale) | no | visual | **HOOKED** line 281-285. |
| 7620 | spawnPlayer2 | () | no | state-mutating-layer | inline. |
| 7621 | speedForShaderTarget | (groupID) -> CCPoint | no | read-only | |
| 7622 | staticObjectsInRect | (rect, enabledGroups) -> CCArray* | no | read-only | |
| 7623 | stopAllGroundActions | () | no | visual | inline. |
| 7624 | stopCameraShake | () | no | visual | inline. |
| 7625 | stopCustomEnterEffect | (obj) overload | no | state-mutating-layer | inline. **NOT hooked**. |
| 7626 | stopCustomEnterEffect | (obj, enter) | no | state-mutating-layer | **NOT hooked**. |
| 7627 | stopSFXTrigger | (SFXTriggerGameObject*) | no | audio | inline. **NOT hooked**. |
| 7628 | swapBackground | (int) | no | visual | inline. |
| 7629 | swapGround | (int) | no | visual | inline. |
| 7630 | swapMiddleground | (int) | no | visual | inline. |
| 7631 | switchToFlyMode | (player, obj, noPortal, type) | no | physics-conditional | inline. Mode switch. Sim needs this for ship/wave/swing physics. |
| 7632 | switchToRobotMode | (player, obj, noPortal) | no | physics-conditional | Mode switch. Sim needs this. |
| 7633 | switchToRollMode | (player, obj, noPortal) | no | physics-conditional | Mode switch (ball). |
| 7634 | switchToSpiderMode | (player, obj, noPortal) | no | physics-conditional | Mode switch. |
| 7635 | syncBGTextures | () | no | engine-internal | |
| 7636 | teleportPlayer | (TeleportPortalObject*, player) | no | physics-required | Sim needs the teleport. Real-player camera-limit side effect is gated by checkCameraLimitAfterTeleport hook in Portals.cpp. |
| 7637 | testInstantCountTrigger | (itemID, count, groupID, activate, mode, remapKeys, uid, cid) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7638 | toggleAudioVisualizer | (visible) | no | visual | |
| 7639 | toggleDualMode | (obj, dual, player, noEffects) | no | physics-conditional | **HOOKED** in TrajPortalsLayerHook (Portals.cpp:18-21) — skip if sim player. |
| 7640 | toggleFlipped | (flip, noEffects) | no | physics-conditional | **HOOKED** in TrajBaseLayerHook line 231-237 — force noEffects=true for sim. |
| 7641 | toggleGroup | (id, activate) | no | state-mutating-layer | **NOT hooked** — toggle trigger fanout. Relies on EffectGameObject gating. |
| 7642 | toggleLockPlayer | (disable, p2) | no | state-mutating-layer | inline. **NOT hooked** — could lock real player. |
| 7643 | togglePlayerStreakBlend | (blend) | no | visual | inline. |
| 7644 | togglePlayerVisibility | (visible) | no | visual | inline. |
| 7645 | togglePlayerVisibility | (visible, p1) | no | visual | inline. |
| 7646 | transformAreaObjects | (obj, objs, scaleX, scaleY, reset) | no | state-mutating-layer | |
| 7647 | triggerAdvancedFollowCommand | (obj) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7648 | triggerAdvancedFollowEditCommand | (obj) | no | trigger-dispatch | **NOT hooked**. |
| 7649 | triggerAreaEffect | (obj) | no | trigger-dispatch | **NOT hooked**. |
| 7650 | triggerAreaEffectAnimation | (obj) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7651 | triggerDynamicMoveCommand | (obj) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7652 | triggerDynamicRotateCommand | (obj) | no | trigger-dispatch | **NOT hooked**. |
| 7653 | triggerGradientCommand | (obj) | no | trigger-dispatch | **NOT hooked**. |
| 7654 | triggerGravityChange | (obj, playerID) | no | trigger-dispatch | inline. **NOT hooked** — relies on EffectGameObject gating + flipGravity hook. |
| 7655 | triggerMoveCommand | (EffectGameObject*) | no | trigger-dispatch | **NOT hooked** — move trigger main fanout. |
| 7656 | triggerRotateCommand | (EnhancedTriggerObject*) | no | trigger-dispatch | inline. **NOT hooked**. |
| 7657 | triggerShaderCommand | (ShaderGameObject*) | no | trigger-dispatch | **NOT hooked**. |
| 7658 | triggerTransformCommand | (TransformTriggerGameObject*) | no | trigger-dispatch | **NOT hooked**. |
| 7659 | tryGetGroupParent | (groupID) -> GameObject* | no | read-only | inline. |
| 7660 | tryGetMainObject | (groupID) -> GameObject* | no | read-only | |
| 7661 | tryGetObject | (groupID) -> GameObject* | no | read-only | |
| 7662 | tryResumeAudio | () | no | audio | inline. |
| 7663 | unclaimParticle | (key, particle) | no | visual | |
| 7664 | ungroupStickyObjects | (CCArray*) | no | engine-internal | |
| 7665 | unlinkAllEvents | () | no | engine-internal | inline. |
| 7666 | updateActiveEnterEffect | (obj) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7667 | updateAllObjectSection | () | no | state-mutating-layer | inline. |
| 7668 | updateAreaObjectLastValues | (obj) | no | state-mutating-layer | |
| 7669 | updateAudioVisualizer | () | no | visual | |
| 7670 | updateBGArtSpeed | (modX, modY) | no | state-mutating-layer | inline. |
| 7671 | updateCamera | (dt) | no | state-mutating-layer | **HOOKED** in TrajBaseLayerHook line 81-92 (drives simulate()). |
| 7672 | updateCameraBGArt | (pos, zoom) | no | state-mutating-layer | |
| 7673 | updateCameraEdge | (direction, value) | no | state-mutating-layer | inline. **NOT hooked** — but mitigated by cameraEdge0..3 in LayerStateSnapshot. |
| 7674 | updateCameraMode | (obj, updateDual) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7675 | updateCameraOffsetX | (offsetX, duration, easing, rate, uid, cid) | no | visual | **HOOKED** line 345-350. |
| 7676 | updateCameraOffsetY | (offsetY, duration, easing, rate, uid, cid) | no | visual | **HOOKED** line 351-356. |
| 7677 | updateCollisionBlocks | () | no | engine-internal | |
| 7678 | updateCounters | (itemId, value) | no | state-mutating-game | **NOT hooked** — sim incrementing item counter would persist. |
| 7679 | updateDualGround | (obj, mode, instant, duration) | no | visual | **NOT hooked**. Closely related to animateInDualGroundNew, same family. |
| 7680 | updateEnterEffects | (dt) | no | state-mutating-layer | |
| 7681 | updateExtendedCollision | (obj, extendedCollision) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7682 | updateExtraGameLayers | () | no | state-mutating-layer | |
| 7683 | updateGameplayOffsetX | (offsetX, staticOffset) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7684 | updateGameplayOffsetY | (offsetY, staticOffset) | no | state-mutating-layer | inline. **NOT hooked**. |
| 7685 | updateGradientLayers | () | no | state-mutating-layer | |
| 7686 | updateGroundShadows | () | no | visual | |
| 7687 | updateGuideArt | () | no | visual | |
| 7688 | updateInternalCamOffsetX | (offsetX, duration, rate) | no | visual | inline. **NOT hooked** — internal camera CCAction. |
| 7689 | updateInternalCamOffsetY | (offsetY, duration, rate) | no | visual | inline. **NOT hooked**. |
| 7690 | updateKeyframeOrder | (keyframeGroup) | no | state-mutating-layer | |
| 7691 | updateLayerCapacity | (capacityString) | no | engine-internal | |
| 7692 | updateLegacyLayerCapacity | (front, frontBlend, back, backBlend) | no | engine-internal | inline. |
| 7693 | updateLevelColors | () | no | state-mutating-layer | Recomputes m_effectManager colors. |
| 7694 | updateMaxGameplayY | () | no | state-mutating-layer | Updates m_maxGameplayY. **NOT hooked** — leak candidate. |
| 7695 | updateMGArtSpeed | (modX, modY) | no | state-mutating-layer | inline. |
| 7696 | updateMGOffsetY | (offsetY, duration, easing, rate, uid, cid) | no | visual | **NOT hooked** — middleground Y tween. Position captured by LayerStateSnapshot but CCAction is not stopped via dedicated hook (relies on stopAllActions on m_middleground at restore). |
| 7697 | updateOBB2 | (rect) | no | state-mutating-layer | inline. |
| 7698 | updateParticles | (dt) | no | state-mutating-layer | |
| 7699 | updatePlatformerTime | () | no | state-mutating-game | |
| 7700 | updatePlayerCollisionBlocks | () | no | engine-internal | |
| 7701 | updateProximityVolumeEffects | () | no | audio | **NOT hooked**. |
| 7702 | updateQueuedLabels | () | no | visual | inline. |
| 7703 | updateReplay | () | no | engine-internal | inline. |
| 7704 | updateSavePositionObjects | () | no | state-mutating-layer | inline. |
| 7705 | updateShaderLayer | (dt) | no | state-mutating-layer | |
| 7706 | updateSpecialGroupData | () | no | engine-internal | |
| 7707 | updateSpecialLabels | () | no | visual | |
| 7708 | updateStaticCameraPos | (pos, staticX, staticY, ...) | no | visual | **HOOKED** line 357-364. |
| 7709 | updateStaticCameraPosToGroup | (centerID, ...) | no | visual | **HOOKED** line 365-376. |
| 7710 | updateTimeMod | (speed, players, noEffects) | no | physics-conditional | **HOOKED** in TrajLayerSpeedHook (Hooks.cpp:444-477). For sim: snapshot real player speeds → super → propagate new speed to sim → restore real. |
| 7711 | updateTimerLabels | () | no | visual | |
| 7712 | updateZoom | (zoom, duration, easing, rate, uid, cid) | no | visual | **NOT hooked** — camera zoom tween. Snapshot reverts m_cameraZoom but CCAction may keep tweening. **Leak candidate.** |
| 7713 | visitWithColorFlash | () | no | visual | |
| 7714 | volumeForProximityEffect | (instance) -> float | no | read-only | |

## Fields table

PlayLayer-direct fields (declared on GJBaseGameLayer; PlayLayer inherits them).

| Line | Name | Type | Classification | Sim impact / notes |
|------|------|------|----------------|-------------------|
| 7717 | m_gameState | GJGameState | layer-game-state | **CAPTURED**: explicit fields + POD-prefix memcpy in LayerStateSnapshot. Contains camera/portal/timeMod/levelFlipping state. |
| 7718 | m_level | GJGameLevel* | level-data | Engine-level pointer; unchanged during sim. |
| 7719 | m_playbackMode | PlaybackMode | engine-internal | |
| 7720-7726 | m_lowDetailMode, m_extraLDM, m_ignoreDamage, m_enable22Changes, m_allowStaticRotate, m_fixNegativeScale, m_startingFromBeginning | bool | engine-internal | Configuration flags, unchanged by gameplay. |
| 7727 | m_activeSfxTriggers | vector<SFXTriggerGameObject*> | shared-container | **NOT captured** — sim-crossed SFX trigger would persist in real's active list. Audio leak vector. |
| 7728 | m_unk8a0 | vector<void*> | engine-internal | Unknown. |
| 7729-7734 | m_hoverNode, m_areaTransformNode, m_areaSkewNode, m_areaScaleNode, m_areaRotateNode, m_areaTransformNode2 | CCNode* | ui-element | Sub-node refs. Position changes during sim would visibly mutate these. |
| 7735 | m_obb2 | OBB2D* | engine-internal | |
| 7736 | m_spawnRemapTriggers | vector<unordered_map<int,int>> | shared-container | **NOT captured** — sim activating remap-style spawn would mutate. |
| 7737 | m_uiObjectPositions | unordered_map<int, CCPoint> | shared-container | **NOT captured**. |
| 7738 | m_effectManager | GJEffectManager* | trigger-state | **NOT captured** — color trigger / pulse trigger / timer trigger state lives here. Sim mutations to color groups would leak globally. **Leak vector** for any non-suppressed color-related trigger. |
| 7739-7875 | m_*BlendingLayer*, m_*Layer*, m_specialLayer*, m_*BatchNode | CCSpriteBatchNode* / CCNodeContainer* | ui-element | ~138 batch-node references for z-layer composition. Engine-managed; sim shouldn't touch. |
| 7876 | m_player1 | PlayerObject* | player-pointer | Engine-owned real player. |
| 7877 | m_player2 | PlayerObject* | player-pointer | Engine-owned real player. |
| 7878 | m_levelSettings | LevelSettingsObject* | level-data | |
| 7879 | m_objects | CCArray* | shared-container | Master object list — sim must NOT add/remove. |
| 7880 | m_collisionBlocks | CCArray* | shared-container | |
| 7881 | m_spawnObjectsArray | CCArray* | shared-container | **NOT captured** — checkSpawnObjects drains this. |
| 7882 | m_spawnObjects | CCDictionary* | shared-container | |
| 7883 | m_unkdd0 | CCNode* | engine-internal | |
| 7884 | m_unkdd8 | vector<GameObject*> | shared-container | Unknown. |
| 7885 | m_disabledObjects | vector<GameObject*> | shared-container | |
| 7886 | m_unke08 | vector<GameObject*> | shared-container | |
| 7887 | m_areaObjects | vector<GameObject*> | shared-container | |
| 7888 | m_processedAreaObjects | vector<GameObject*> | shared-container | |
| 7889 | m_visibilityGroups | unordered_map<int, vector<GameObject*>> | shared-container | |
| 7890 | m_visibleObjects | vector<GameObject*> | shared-container | Per-frame; engine recomputes. |
| 7891-7905 | m_visibleObjectsCount/Index, m_visibleObjects2*, m_disabledObjects*, m_areaObjects*, m_processedAreaObjects* | int | section-index | Per-frame counters; engine recomputes. |
| 7906 | m_groupDict | CCDictionary* | trigger-state | **NOT captured** — group toggle state. |
| 7907 | m_staticGroupDict | CCDictionary* | trigger-state | |
| 7908 | m_optimizedGroupDict | CCDictionary* | trigger-state | |
| 7909 | m_groups | vector<CCArray*> | trigger-state | **NOT captured** — addToGroup/removeFromGroup mutate this; sim-driven move/toggle triggers would leak. |
| 7910 | m_staticGroups | vector<CCArray*> | trigger-state | |
| 7911 | m_optimizedGroups | vector<CCArray*> | trigger-state | |
| 7912 | m_parentGroupsDict | CCDictionary* | trigger-state | |
| 7913 | m_parentGroupIDs | CCDictionary* | trigger-state | |
| 7914 | m_removedParentGroupIDs | CCDictionary* | trigger-state | |
| 7915 | m_targetGroupsArray | CCArray* | trigger-state | |
| 7916 | m_targetGroups | unordered_map<int, pair<int,int>> | trigger-state | |
| 7917 | m_linkedGroupDict | CCDictionary* | trigger-state | |
| 7918 | m_lastUsedLinkedID | int | trigger-state | |
| 7919-7924 | m_objectParent, m_inShaderParent, m_aboveShaderParent, m_objectLayer, m_inShaderObjectLayer, m_aboveShaderObjectLayer | CCNode*/CCLayer* | ui-element | Sim's player is parented to m_objectLayer (Trajectory.cpp:241). |
| 7925 | m_background | CCSprite* | ui-element | |
| 7926 | m_unk1000 | void* | engine-internal | |
| 7927 | m_groundLayer | GJGroundLayer* | layer-game-state | **CAPTURED**: position + stopAllActions in LayerStateSnapshot. Sim ball-physics reads this. |
| 7928 | m_groundLayer2 | GJGroundLayer* | layer-game-state | **CAPTURED**. |
| 7929 | m_middleground | GJMGLayer* | layer-game-state | **CAPTURED**. |
| 7930 | m_batchNodes | CCArray* | ui-element | |
| 7931 | m_objectsToDeactivate | CCDictionary* | shared-container | |
| 7932 | m_labelObjects | unordered_map<int, vector<LabelGameObject*>> | shared-container | |
| 7933 | m_timeLabelObjects | unordered_map<int, vector<LabelGameObject*>> | shared-container | |
| 7934 | m_spawnTuples | set<tuple<int,int,int>> | shared-container | **NOT captured** — sim-driven spawnObject would add to this. |
| 7935 | m_increasedLayerCapacity | bool | engine-internal | |
| 7936 | m_varianceValues | array<float, 2000> | engine-internal | RNG / variance table. |
| 7937 | m_destroyObjectValues | map<pair<int,int>, pair<float,float>> | shared-container | **NOT captured** — destroyObject writes here. Mitigated because destroyObject is hooked. |
| 7938 | m_enterEasingValues | vector<float> | shared-container | **NOT captured** — generateEnterEasingBuffer would append. |
| 7939 | m_enterEasingIndices | unordered_map<int,int> | shared-container | |
| 7940 | m_enterEasingValuesIndex | int | shared-container | |
| 7941 | m_dualTouchTrigger | bool | layer-game-state | **NOT captured directly** but probably stable; potential leak. |
| 7942 | m_clicks | int | state-mutating-game | **NOT captured** — sim button "press" wouldn't reach here (handleButton gates via sim), but defensive capture might be warranted. |
| 7943 | m_attempts | int | state-mutating-game | |
| 7944 | m_jumping | bool | layer-game-state | |
| 7945-7948 | m_leftSectionIndex, m_rightSectionIndex, m_bottomSectionIndex, m_topSectionIndex | int | section-index | Per-tick recomputed; no isolation needed. |
| 7949-7951 | m_isEditor, m_blending, m_isPlatformer | bool | engine-internal | Configuration. |
| 7952 | m_player1CollisionBlock | GameObject* | engine-internal | Engine-managed. |
| 7953 | m_player2CollisionBlock | GameObject* | engine-internal | |
| 7954-7956 | m_particleCount, m_customParticleCount, m_particleSystemLimit | int | shared-container | Particle pool counters. Particle creation is suppressed for sim. |
| 7957-7963 | m_particlesDict, m_customParticles, m_unclaimedParticles, m_particleCountToParticleString, m_claimedParticles, m_temporaryParticles, m_customParticlesUIDs | dictionaries/sets | shared-container | Particle pools. Sim suppresses particle creation. |
| 7964 | m_gradientLayers | CCDictionary* | trigger-state | **NOT captured** — gradient trigger state. |
| 7965 | m_activeGradients | int | trigger-state | |
| 7966 | m_shaderLayer | ShaderLayer* | ui-element | |
| 7967 | m_objectsDeactivated | bool | layer-game-state | |
| 7968 | m_areaObjectsUpdated | bool | layer-game-state | |
| 7969 | m_startPosObject | StartPosObject* | level-data | |
| 7970 | m_useReplay | bool | engine-internal | |
| 7971 | m_unk3189 | bool | engine-internal | |
| 7972 | m_solidCollisionObjectsCount | int | section-index | |
| 7973 | m_solidCollisionObjectsIndex | int | section-index | |
| 7974 | m_solidCollisionObjects | vector<GameObject*> | shared-container | Per-frame; engine recomputes. |
| 7975-7977 | m_hazardCollisionObjects* | int/vector | section-index | Per-frame. |
| 7978 | m_sequenceTriggers | vector<SequenceTriggerGameObject*> | shared-container | **NOT captured** — sim-driven sequence triggers would mutate. |
| 7979 | m_isPracticeMode | bool | engine-internal | |
| 7980 | m_practiceMusicSync | bool | engine-internal | |
| 7981 | m_loadingProgress | float | engine-internal | |
| 7982 | m_flashNode | CCNode* | ui-element | |
| 7983 | m_unk31f8 | float | engine-internal | |
| 7984-7986 | m_cameraFlip, m_cameraWidthOffset, m_cameraHeightOffset | float | layer-game-state | Camera dimensions; should be stable across sim ticks. **NOT explicitly captured** — possible leak if a trigger writes them mid-sim. |
| 7987 | m_updateGroundShadows | bool | layer-game-state | |
| 7988 | m_collectedItems | CCDictionary* | state-mutating-game | **NOT captured** — sim collecting items would persist. (Coins filtered upstream.) |
| 7989 | m_levelLength | float | level-data | |
| 7990 | m_resetActiveObjects | bool | layer-game-state | |
| 7991 | m_skipArtReload | bool | engine-internal | |
| 7992 | m_endPortal | EndPortalObject* | level-data | |
| 7993 | m_isTestMode | bool | engine-internal | |
| 7994 | m_freezeStartCamera | bool | layer-game-state | |
| 7995 | m_unk322a | bool | engine-internal | |
| 7996 | m_cameraUnzoomedHeightOffset | float | layer-game-state | |
| 7997 | m_targetCameraHeightOffset | float | layer-game-state | |
| 7998 | m_calculateTargetHeightOffset | bool | layer-game-state | |
| 7999 | m_glitterParticles | CCParticleSystemQuad* | ui-element | |
| 8000-8001 | m_staticCameraShake, m_skipCameraShake | bool | layer-game-state | |
| 8002 | m_playerDied | bool | state-mutating-game | Sim death sets this on the SIM player only; real death distinct. |
| 8003 | m_extraDelta | double | layer-game-state | Engine tick accumulator. |
| 8004 | m_started | bool | layer-game-state | |
| 8005 | m_unk3251 | bool | engine-internal | |
| 8006-8009 | m_cameraWidth, m_cameraHeight, m_cameraUnzoomedX, m_halfCameraWidth | float | layer-game-state | Camera dimensions. |
| 8010 | m_audioEffectsLayer | AudioEffectsLayer* | ui-element | |
| 8011 | m_cameraObb2 | OBB2D* | engine-internal | |
| 8012-8014 | m_activeObjects, m_activeObjectsCount, m_activeObjectsIndex | vector/int | shared-container / section-index | |
| 8015 | m_lightBGColor | ccColor3B | layer-game-state | |
| 8016 | m_resumeTimer | int | layer-game-state | |
| 8017-8021 | m_recordInputs, m_unk32a1..m_unk32a4 | bool | engine-internal | |
| 8022 | m_recordString | string | state-mutating-game | **NOT captured** — sim button presses would corrupt the replay buffer (mitigated only because handleButton's sim-gate skips real-button recording for sim ticks). |
| 8023-8030 | m_unk32c8, m_unk32d0, m_unk32d4, m_queueInterval, m_coinsCollected, m_replayRandSeed, m_unk32ec, m_currentStep | various | state-mutating-game | **m_coinsCollected NOT captured** — coins filtered upstream. m_replayRandSeed / m_currentStep used by replay; sim shouldn't reach. |
| 8031-8033 | m_queuedButtons, m_queuedRecordedButtons, m_queuedReplayButtons | vector<PlayerButtonCommand> | shared-container | **NOT captured** — sim handleButton path could enqueue; mitigated by BotHooks.cpp handleButton override skipping engine super for bot path. |
| 8034-8035 | m_unk3340, m_unk3358 | vector<void*> | engine-internal | |
| 8037 | m_queuedRecordedButtonsSize | int | shared-container | |
| 8038-8040 | m_portalIndicators, m_orbIndicators, m_indicatorSprites | bool/CCArray* | ui-element | |
| 8041-8043 | m_unk3380, m_unk3388, m_unk33a0 | float/vector<int> | engine-internal | |
| 8045 | m_hideGround | bool | ui-element | |
| 8046-8047 | m_unk33c0, m_objectsToMove | CCArray* | shared-container | |
| 8048-8049 | m_savePositionObjects, m_savePositionValues | unordered_map | shared-container | **NOT captured** — save-position triggers would mutate. |
| 8050 | m_keepGroupParents | bool | engine-internal | |
| 8051-8052 | m_keyframeGroups, m_keyframeGroup | CCDictionary*/int | trigger-state | **NOT captured** — addKeyframe / refreshKeyframeAnims would mutate. |
| 8053-8056 | m_uiLayer, m_uiObjects, m_uiObjectLayers, m_uiTriggerUI | UILayer*/CCArray*/CCDictionary*/CCNode* | ui-element | |
| 8057 | m_timePlayed | double | state-mutating-game | |
| 8058-8059 | m_unk3568, m_unk356c | int | engine-internal | |
| 8060 | m_levelEndAnimationStarted | bool | state-mutating-game | Sim must NOT touch; mitigated by playEndAnimationToPos hook. |
| 8061-8062 | m_points, m_pointsString | int/string | state-mutating-game | **NOT captured** — addPoints leak vector. |
| 8063-8075 | m_sections, m_nonEffectObjects, m_collisionBlockSections, m_calcNonEffectObjects*, m_calcCollisionBlockObjects* (and Sizes vectors) | vectors / arrays | shared-container | Spatial section index. Engine-managed per-tick. Sim must NOT call addToSection / updateObjectSection. |
| 8076-8077 | m_sectionXFactor, m_sectionYFactor | float | engine-internal | |
| 8078 | m_maxGameplayY | float | layer-game-state | Updated by updateMaxGameplayY (which is NOT hooked); ball-portal mode flips can alter this. **NOT explicitly captured** — possible regression vector. |
| 8079 | m_songTriggerInterval | float | layer-game-state | |
| 8080 | m_stickyGroups | unordered_map<int,int> | trigger-state | |
| 8081-8083 | m_audioVisualizerBG, m_audioVisualizerSFX, m_showAudioVisualizer | FMODLevelVisualizer*/bool | ui-element / audio | |
| 8084-8107 | m_area*Count*, m_movedCount, m_scaledCount, m_rotatedCount, m_followedCount (+ Display variants) | int | engine-internal | Statistics counters. |
| 8108-8111 | m_loadingStartPosition, m_processingAudioTriggers, m_audioPaused, m_startOptimization | bool | engine-internal | |
| 8112 | m_loadingLayer | GJGameLoadingLayer* | ui-element | |
| 8113-8115 | m_debugDrawNode, m_debugDrawPoints, m_isDebugDrawEnabled | CCDrawNode*/array*/bool | ui-element | Sim uses for trajectory viz (Trajectory.cpp:265). |
| 8116 | m_disablePlayerHitbox | bool | engine-internal | |
| 8117 | m_hitboxesOnDeath | bool | engine-internal | |
| 8118 | m_anticheatSpike | GameObject* | engine-internal | Sentinel object referenced in TrajPlayLayerHook::destroyPlayer (Hooks.cpp:51). |
| 8119 | m_timestamp | double | layer-game-state | |
| 8120-8122 | m_isBetweenSteps, m_clickBetweenSteps, m_clickOnSteps | bool | layer-game-state | Sub-tick stepping flags. |

## Cross-reference with current isolation

### Methods hooked in src/Hooks.cpp / Orbs.cpp / Portals.cpp

`TrajPlayLayerHook` (`PlayLayer` — included for context; not GJBaseGameLayer):
- `setupHasCompleted` — registers sim with mod state.
- `resetLevel`, `onQuit` — sim lifecycle.
- `destroyPlayer` (PlayLayer override) — sim death markers; skips real cascade if sim player.
- `playEndAnimationToPos`, `playPlatformerEndAnimationToPos` — suppress for sim.
- `playGravityEffect` (PlayLayer override) — suppress for sim; covers the inline base.

`TrajBaseLayerHook` (`GJBaseGameLayer`):
- `updateCamera` — sim driver (entry to simulate()).
- `handleButton` — record real button presses for sim.
- `destroyObject` — sim-local destroyed set; never touch real.
- `collisionCheckObjects` — spatial cull + sim-destroyed/coins/pads/orbs/portals filter.
- `flipGravity` — sim-player only + force noEffects.
- `canBeActivatedByPlayer` — true for sim-player only.
- `toggleFlipped` — force noEffects.
- `shakeCamera`, `moveCameraToPos`, `updateScreenRotation` — suppress for sim.
- `playFlashEffect` — suppress.
- `spawnParticle` (returns nullptr), `spawnParticleTrigger` (×2), `lightningFlash` (×2), `playSpeedParticle` — suppress.
- `animatePortalY` — suppress for sim.
- `cameraMoveX`, `cameraMoveY`, `updateCameraOffsetX/Y`, `updateStaticCameraPos`, `updateStaticCameraPosToGroup` — suppress.

`TrajLayerSpeedHook` (`GJBaseGameLayer`):
- `updateTimeMod(speed, players, noEffects)` — snapshot+propagate dance for speed-mod portals.

`TrajPlayerObjectHook`, `TrajEffectHook`, `TrajGameObjectHook`, `TrajEnhancedHook`, `TrajHardStreakHook` — not GJBaseGameLayer hooks; isolated separately for the player object and trigger objects.

`TrajOrbsLayerHook` (Orbs.cpp, `GJBaseGameLayer`):
- `playerTouchedRing` — sim-player gated.

`TrajPortalsLayerHook` (Portals.cpp, `GJBaseGameLayer`):
- `toggleDualMode` — sim-player skip.
- `checkCameraLimitAfterTeleport` — sim-player skip.

### Fields in LayerStateSnapshot (`src/Trajectory.cpp:34-227`)

On `m_gameState` (explicitly captured):
- `m_cameraZoom`, `m_targetCameraZoom`, `m_cameraOffset`, `m_cameraPosition`, `m_cameraPosition2`, `m_cameraAngle`, `m_targetCameraAngle`, `m_cameraEdgeValue0/1/2/3`, `m_cameraShakeEnabled`, `m_cameraShakeFactor`, `m_cameraStepDiff`.
- `m_isDualMode`, `m_dualRelated`.
- `m_levelFlipping`, `m_gravityRelated`, `m_portalY`, `m_lastActivatedPortal1`, `m_lastActivatedPortal2`.
- `m_timeWarp`, `m_queuedTimeWarp`, `m_timeWarpRelated`, `m_currentChannel`, `m_rotateChannel`.
- `m_middleGroundOffsetY`, `m_timeModRelated`, `m_timeModRelated2`.
- **POD-prefix memcpy** covering the entire PRE-`m_spawnChannelRelated0` byte range, restored LAST after explicit field writes (Trajectory.cpp:225). This is the catch-all for any GJGameState POD field not enumerated above.

On `PlayLayer` directly:
- `m_speedObjects` (entire CCArray content as pointer-set).
- `m_groundLayer` position + stopAllActions.
- `m_groundLayer2` position + stopAllActions.
- `m_middleground` position + stopAllActions.

### Leak candidates (NOT hooked / NOT captured)

#### High-risk (live trigger fanout that could fire during sim if a non-EffectGameObject route hits them)

- **`activateSongTrigger`, `activateSongEditTrigger`, `activatedAudioTrigger` (virtual + overload), `activateSFXTrigger`, `activateSFXEditTrigger`, `activatePersistentItemTrigger`, `applySFXEditTrigger`, `processActivatedAudioTriggers`, `processQueuedAudioTriggers`, `processSFXObjects`, `processSFXState`, `processSongState`, `pauseAudio`, `resumeAudio`, `resetAudio`, `addProximityVolumeEffect`, `updateProximityVolumeEffects`, `stopSFXTrigger`, `tryResumeAudio`** — Symptom-if-leak: sim crossing an SFX/song trigger plays audio on real / swaps music globally. Today all audio triggers route through EffectGameObject::triggerObject which is gated, BUT the engine's per-tick `processActivatedAudioTriggers` and `processQueuedAudioTriggers` ALSO drain accumulated state regardless of who queued — so any sim-side queue insertion would replay through on the next real tick. *Suggested fix: hook `activateSFXTrigger`, `activateSongTrigger`, `activatedAudioTrigger` (both forms) with sim suppression.*

- **`processCommands`, `processMoveActions`, `processMoveActionsStep`, `processRotationActions`, `processFollowActions`, `processPlayerFollowActions`, `processAdvancedFollowActions`, `processAreaActions`, `processAreaEffects`, `processAreaVisualActions`, `processDynamicObjectActions`, `processTransformActions`, `processItems`, `processStateObjects`** — Symptom-if-leak: per-tick command drain runs regardless of who queued an entry. If sim invokes a path that registers a queued action (move group, follow, area effect), the next real tick fires it. *Mitigation today: relies on EffectGameObject::triggerObject suppressing the QUEUE-the-action call. Any non-EffectGameObject path to queueing leaks.*

- **`toggleGroupTriggered`, `toggleGroup`, `spawnGroup`, `spawnGroupTriggered`, `spawnObject`, `spawnObjectsInOrder`, `checkSpawnObjects`** — Symptom-if-leak: sim activates a group toggle / spawn, real player sees objects appear/disappear that weren't crossed. *Suggested fix: defensive hooks on these top-level virtuals with sim suppression to make the gating not depend solely on EffectGameObject::triggerObject.*

- **`activateEndTrigger`, `activatePlatformerEndTrigger`** — Symptom-if-leak: sim crossing end trigger writes end-state to layer. *Indirectly mitigated by PlayLayer's playEndAnimationToPos hook but the underlying state may still set.*

- **`activatePlayerControlTrigger`, `toggleLockPlayer`** — Symptom-if-leak: sim crossing a lock-player trigger disables real player input.

- **`updateCounters`, `addPoints`, `recordAction`** — Symptom-if-leak: sim adds points / counters / replay-bytes; persists into real save and stats. Note `m_recordString` (line 8022) is the replay buffer; if a sim-driven path ever calls recordAction, the replay file becomes wrong.

- **`processOptionsTrigger`** — Symptom-if-leak: sim-crossed options trigger modifies game options globally (disable death, no-input, etc.).

- **`gameEventTriggered`** — Symptom-if-leak: generic GJGameEvent dispatch; sim might fire material/death events through this. *Not hooked but unlikely to be reached during pure trajectory sim.*

#### Mid-risk (state mutators that today depend solely on EffectGameObject gating)

- **`moveObject`, `moveObjects`, `moveObjectsSilent`, `moveObjectToStaticGroup`, `triggerMoveCommand`** — Symptom-if-leak: sim crossing a move trigger translates real objects. Today mitigated by triggerObject gate; a defensive hook here would harden against trigger paths that bypass that gate.
- **`rotateObject`, `rotateObjects`, `rotateAreaObjects`, `triggerRotateCommand`, `triggerDynamicRotateCommand`** — Same class: rotation-trigger leak vector.
- **`transformAreaObjects`, `triggerTransformCommand`** — Same class.
- **`updateColor`** (virtual, line 7305) — Symptom-if-leak: sim-driven color trigger writes m_effectManager state; real game sees color shift. Heavily depends on EffectGameObject gate.
- **`addKeyframe`, `removeKeyframe`, `updateKeyframeOrder`, `refreshKeyframeAnims`, `playKeyframeAnimation`** — Symptom-if-leak: sim-crossed keyframe trigger adds to m_keyframeGroups, plays animations in real time.
- **`triggerShaderCommand`, `triggerGradientCommand`, `controlGradientTrigger`** — Same class for shader/gradient triggers. m_gradientLayers / m_activeGradients not in snapshot.
- **`addAreaEffect`, `triggerAreaEffect`, `triggerAreaEffectAnimation`, `controlAreaEffect`, `controlAreaEffectWithID`** — Symptom-if-leak: sim adds area effect; real game sees the area animation play.
- **`controlEventLink`, `activateEventTrigger`** — Event-link trigger.
- **`activateItemEditTrigger`, `activateItemCompareTrigger`, `activateObjectControlTrigger`, `activateTimerTrigger`** — Item / object-control / timer trigger state.

#### Visual leak candidates (CCAction-based)

- **`updateZoom`** — Symptom-if-leak: zoom CCAction keeps running past sim. `m_cameraZoom` snapshot reverts the instant value but CCAction re-applies during real ticks. Not hooked.
- **`updateMGOffsetY`** — Symptom-if-leak: middleground Y CCAction keeps running. Mitigated only because `m_middleground->stopAllActions()` fires at snapshot restore.
- **`updateInternalCamOffsetX/Y`** — Symptom-if-leak: internal camera CCAction. Not stopped explicitly.
- **`exitStaticCamera`** — Symptom-if-leak: static-camera CCAction.
- **`flipObjects`** — Symptom-if-leak: sim crossing flip-trigger toggles visual flip on real art (mitigated only because toggleFlipped is hooked and its flipObjects call would happen there).
- **`updateGuideArt`, `updateGroundShadows`** — Lower-risk visual updates.
- **`playExitDualEffect`** — Symptom-if-leak: sim's dual-exit triggers the visual flash on real (real probably stays in dual so the call may be cold).
- **`toggleGroundVisibility`, `toggleMGVisibility`** — Symptom-if-leak: sim-crossed visibility trigger flickers real ground/MG.

#### Particle leak candidates

- **`createParticle`, `createCustomParticle`, `claimCustomParticle`, `claimParticle`, `unclaimCustomParticle`, `unclaimParticle`** — Symptom-if-leak: sim-driven particle factory adds to scene tree; m_particlesDict/m_claimedParticles/m_unclaimedParticles accumulate. `spawnParticle` and `spawnParticleTrigger` ARE hooked, but these factories are separate entry points.

#### Scene-graph / bookkeeping leak candidates

- **`addToSection`, `removeObjectFromSection`, `updateObjectSection`, `updateAllObjectSection`, `reorderObjectSection`, `sortSectionVector`** — Symptom-if-leak: sim mutates spatial section index; real game's per-tick neighborhood queries become wrong.
- **`addToGroup`, `removeFromGroup`, `addToGroups`, `removeFromGroups`, `addToGroupParents`, `removeFromGroupParents`, `setGroupParent`, `removeGroupParent`, `reAddToStickyGroup`, `removeFromStickyGroup`** — Symptom-if-leak: sim-driven group membership change persists.
- **`addUIObject`, `restoreAllUIObjects`, `addObjectCounter`** — UI-object membership.
- **`addPickupTrigger`** — Adds count-trigger entry. Not hooked.
- **`registerStateObject`** — Adds to state-object list. Not hooked.

#### Fields not captured

- **`m_effectManager`** (line 7738) — Trigger system state. Color/pulse/timer trigger writes go here; sim mutations leak. *Highest-risk field not in snapshot.*
- **`m_groups`** (7909), `m_staticGroups`, `m_optimizedGroups`, **`m_groupDict`** (7906) — Group dictionaries. Sim move/toggle triggers add/remove here.
- **`m_keyframeGroups`** (8051), `m_keyframeGroup` (8052) — Keyframe trigger state.
- **`m_gradientLayers`** (7964), `m_activeGradients` (7965) — Gradient trigger state.
- **`m_activeSfxTriggers`** (7727) — SFX trigger active list.
- **`m_collectedItems`** (7988) — Item collection state.
- **`m_coinsCollected`** (8027) — Coin count.
- **`m_points`** (8061), `m_pointsString` (8062) — Point counters.
- **`m_clicks`** (7942), `m_attempts` (7943), `m_jumping` (7944) — Player input counters.
- **`m_recordString`** (8022) — Replay buffer.
- **`m_savePositionObjects`** (8048), `m_savePositionValues` (8049) — Save-position trigger state.
- **`m_spawnTuples`** (7934), `m_spawnObjectsArray` (7881), `m_spawnObjects` (7882) — Spawn queue.
- **`m_destroyObjectValues`** (7937) — Destroy-object cache. Mitigated by destroyObject hook.
- **`m_enterEasingValues`** (7938), `m_enterEasingIndices` (7939), `m_enterEasingValuesIndex` (7940) — Enter-easing buffers.
- **`m_sequenceTriggers`** (7978) — Sequence trigger list.
- **`m_maxGameplayY`** (8078) — Updated by updateMaxGameplayY; ball-portal flip could rewrite.
- **`m_cameraFlip`** (7984), **`m_cameraWidthOffset`** (7985), **`m_cameraHeightOffset`** (7986), `m_cameraUnzoomedHeightOffset` (7996), `m_targetCameraHeightOffset` (7997), `m_calculateTargetHeightOffset` (7998), `m_cameraWidth/Height/UnzoomedX/halfCameraWidth` (8006-8009) — Camera dimensions. Mostly stable but a trigger could rewrite mid-sim.
- **`m_queuedButtons`** (8031), `m_queuedRecordedButtons` (8032), `m_queuedReplayButtons` (8033) — Button queues; sim mitigated only through handleButton gating.

## Special focus areas — findings

### Trigger-dispatch methods

The entire `activate*Trigger`, `process*`, `trigger*Command`, `control*` family (~40 methods) is **not individually hooked**. Today the design assumes every trigger fires through `EffectGameObject::triggerObject` (which TrajEffectHook gates on `!isSpeedMod`), so the gate at that single point catches the dispatch. This works for triggers whose only call site is the standard activation pipeline, but several methods are ALSO called from non-EffectGameObject paths:

- `processCommands`, `processMoveActions`, `processFollowActions`, `processQueuedAudioTriggers`, `processActivatedAudioTriggers` are drained EVERY tick from the engine's update loop regardless of activator. If a sim-side flow registers an entry (via, say, a queued audio trigger inserted by a non-EffectGameObject activation source), the next REAL tick drains it.
- `addToSpeedObjects` is virtual on GJBaseGameLayer; it's hit by EffectGameObject speed-mod activation (which we let pass for sim) — captured in snapshot's `speedObjects` so OK.
- `gameEventTriggered`, `activateCustomRing` are non-trigger-class entry points to dispatch.

### Scene-graph mutators

`addToSection`, `addToGroup`, `removeFromGroup`, `updateObjectSection`, `addUIObject`, `addToGroups`, `removeFromGroups`, `setGroupParent`, `removeGroupParent` are all **not hooked**. The sim should never reach them in normal flow (objects exist before sim starts), but a sim-side reset/setup path or a future feature could.

### Audio

Confirmed: **none of the audio methods are hooked**. The audio fanout family — `activatedAudioTrigger` (virtual + overload), `activateSongTrigger`, `activateSongEditTrigger`, `activateSFXTrigger`, `activateSFXEditTrigger`, `applySFXEditTrigger`, `addProximityVolumeEffect`, `processActivatedAudioTriggers`, `processQueuedAudioTriggers`, `processSFXObjects`, `processSFXState`, `processSongState`, `pauseAudio`, `resumeAudio`, `resetAudio`, `tryResumeAudio`, `stopSFXTrigger`, `updateProximityVolumeEffects`, `volumeForProximityEffect`, `canProcessSFX` — currently relies entirely on EffectGameObject::triggerObject gating. *Suggested defensive hook: at minimum `activateSongTrigger` + `activateSFXTrigger` + `activatedAudioTrigger` virtual.*

### Time warp / speed

- `updateTimeWarp` and `applyTimeWarp` write to `m_gameState.m_timeWarp` / `m_queuedTimeWarp` / `m_timeWarpRelated` — **captured** (Trajectory.cpp:141-143).
- `addToSpeedObjects` writes to `m_speedObjects` CCArray — **captured** as a pointer snapshot.
- `removeFromSpeedObjects` — **NOT present in the binding** (only `addToSpeedObjects` is declared). The engine's internal removal would write into the snapshot-captured array. The snapshot fully rebuilds m_speedObjects content on restore, so removal during sim is rolled back implicitly.
- `updateTimeMod` (LAYER) — **HOOKED** in TrajLayerSpeedHook with the snapshot+propagate dance.
- `playSpeedParticle` — **HOOKED** suppression for sim.

### Groups / sections

`assignNewStickyGroups`, `optimizeMoveGroups`, `groupStickyObjects`, `generateTargetGroups`, `generateSpawnRemap`, `sortGroups`, `sortStickyGroups`, `generateVisibilityGroups` — all engine-internal, called from level setup, cold-path during sim. None hooked; not concerning.

---

## Surprises / flags to call out to user

1. **`m_effectManager` is not captured.** This is the single highest-impact uncaptured field — it owns color triggers, pulse triggers, timer triggers, and arguably touch-trigger state. Today the only thing stopping a leak is that EffectGameObject::triggerObject suppression early-exits before m_effectManager is mutated. If any non-EffectGameObject route writes to m_effectManager during sim, it persists into the real game. Empirical evidence of color drift after sim runs would point here.

2. **`updateColor` is virtual and currently NOT hooked.** It writes to m_effectManager. Color triggers ARE EffectGameObject subclasses so today's EffectGameObject gate catches them, but the virtual is the natural defense-in-depth hook surface.

3. **Audio is wide-open.** All 15+ audio methods rely solely on EffectGameObject gating. The engine's per-tick `processActivatedAudioTriggers` / `processQueuedAudioTriggers` drain queues regardless of source; if sim ever queues an audio entry through any non-EffectGameObject path, it plays on real.

4. **`processCommands` drains the per-tick command queue unconditionally.** Same risk class as audio — any sim-side queue insertion replays on the next real tick.

5. **`updateZoom` is not hooked.** Camera-zoom CCAction can continue past sim end. The snapshot reverts m_cameraZoom, but the CCAction targets the layer itself; `m_objectLayer->stopAllActions()` or equivalent at restore would close the loop. Currently only the ground/middleground layers get stopAllActions.

6. **`updateMaxGameplayY` is not hooked, and `m_maxGameplayY` is not captured.** A sim-crossed ball-portal that updates the gameplay ceiling would leak the new ceiling to real.

7. **`m_recordString` (replay buffer, line 8022) is not captured.** This is the only field tied to the replay output. Today the only mitigation is that `handleButton`'s sim gate skips real-button recording — if any other code path writes to m_recordString during sim, the saved replay becomes wrong.

8. **`m_cameraFlip`, `m_cameraWidthOffset`, `m_cameraHeightOffset` and friends** — camera dimensions sit OUTSIDE m_gameState (lines 7984-7986), so the POD-prefix memcpy can't reach them. If a trigger rewrites these during sim, they leak. Currently no explicit capture.

9. **Heuristic gating works because of trigger taxonomy, not because of a hard barrier.** The mod's isolation today rests on the empirical observation that almost every trigger is an EffectGameObject (and is therefore gated). The audit shows 60+ trigger-dispatch entry points on GJBaseGameLayer that are not individually defended. The dependence on the EffectGameObject gate is a single point of failure for future engine changes or for any trigger class that bypasses it.

10. **Per-line cross-reference highlights for follow-up:** the highest-yield defensive hooks would be `updateColor` (virtual, line 7305), `toggleGroupTriggered` (virtual, line 7306), `spawnGroup` (virtual, line 7307), `spawnObject` (virtual, line 7308), `activateSFXTrigger` (line 7357), `activateSongTrigger` (line 7359), `activatedAudioTrigger` (virtual, line 7336), `updateZoom` (line 7712), `moveObjects` (line 7502), and `rotateObjects` (line 7603). Each is one-line `if (sim().isSimulating()) return;` away from defense-in-depth.
