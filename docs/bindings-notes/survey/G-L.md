# Bindings Survey: G-L

Total files in range: 176. Reviewed: 176.

NOTE: All header files in `bindings/bindings/Geode/binding/` are thin shims that
`#include` the real declarations from `binding_arm/` (or `binding_intel/`). All
line numbers below reference the **`binding_arm/`** copies, which is what we
actually use on this Mac dev box.

---

## Heavyweight reads

### `GJBaseGameLayer.hpp` (4374 lines) - Main game layer **[HEAVY READ]**

Inherits `cocos2d::CCLayer, TriggerEffectDelegate`. The `m_gameState` field
(of type `GJGameState`, line 3964) is the snapshot target we already protect
in `Trajectory.cpp`. Many "free" gameplay-state fields live directly on the
layer outside that struct; those are NOT in our snapshot today.

#### Particle methods (problem F1 - portal particles in sim)
- `virtual createCustomParticle(key, struct, minimum, dontAdd)` at L443
- `virtual claimCustomParticle(key, struct, zLayer, zOrder, uiObject, dontAdd)` at L452
- `virtual unclaimCustomParticle(key, particle)` at L461
- `claimParticle(gd::string key, int zLayer)` at L1073 - **[INTEREST]** non-virtual claim path; if `noEffects` blocks the virtual one, this still gets called by GameObject internals.
- `createParticle(objectType, plistName, tag, positionType)` at L1262
- `getParticleKey(...)` at L1640
- `getParticleKey2(key)` at L1649
- `lightningFlash(to, color)` at L1883
- `lightningFlash(from, to, color, lineWidth, duration, displacement, flash, opacity)` at L1892
- `playSpeedParticle(timeMod)` at L2207 - **[INTEREST]** speed-portal entry particle. Hookable for F1 if `sim().isSimulating()`.
- `removeTemporaryParticles()` at L2729 - **[INTEREST]** cleanup hook; could be invoked at sim-end to flush any sim-spawned particles that leaked.
- `spawnParticle(plist, zOrder, positionType, position)` at L3089 - **[INTEREST]** generic spawn; main entry to consider gating.
- `spawnParticleTrigger(SpawnParticleGameObject*)` at L3098
- `spawnParticleTrigger(int particleID, position, rotation, scale)` at L3107
- `unclaimParticle(key, particle)` at L3503
- `updateParticles(dt)` at L3818

Particle batch nodes (background storage, irrelevant to gating but explains the pool):
- `m_particleLayerT4..T1, B0..B5` and `m_particleBlendingLayer*` at L3989-4120
- `m_glitterParticles` at L4246

Particle bookkeeping fields at L4201-4210:
- `m_particleCount`, `m_customParticleCount`, `m_particleSystemLimit`,
  `m_particlesDict`, `m_customParticles`, `m_unclaimedParticles`,
  `m_particleCountToParticleString`, `m_claimedParticles`,
  `m_temporaryParticles`, `m_customParticlesUIDs`.
- **[INTEREST]** `m_temporaryParticles` is the array `removeTemporaryParticles()` empties - if portal-particle-during-sim is per-frame and not per-collision, retaining it across sims is what's leaking.

#### Camera methods (problem F2 - camera tweens leak from sim)
- `virtual updateScreenRotation(rotation, add, convert, duration, easingType, easingRate, uniqueID, controlID)` at L371
- `cameraMoveX(value, duration, rate, unused)` at L965
- `cameraMoveY(value, duration, rate, force)` at L974
- `moveCameraToPos(pos)` at L1982
- `shakeCamera(duration, strength, interval)` at L3008 - **[INTEREST]** F2 - this and stopCameraShake mutate camera shake state we DO snapshot (`m_cameraShakeEnabled`, `m_cameraShakeFactor`).
- `stopCameraShake()` at L3152
- `applyShake(point&)` at L875
- `updateCamera(dt)` at L3575 - **[INTEREST]** main camera advance call inside update; sim's update tick may invoke this transitively.
- `updateCameraBGArt(position, zoom)` at L3584
- `updateCameraEdge(direction, value)` at L3593
- `updateCameraMode(obj, updateDual)` at L3602
- `updateCameraOffsetX(offsetX, duration, easingType, easingRate, uniqueID, controlID)` at L3611 - **[INTEREST]** these create CCActions on the GJValueTween/effect manager. Tween action map IS a snapshot field (`m_tweenActions` in GJGameState), but the function may schedule additional CCActions on the layer/objects themselves.
- `updateCameraOffsetY(...)` at L3620
- `updateStaticCameraPos(pos, staticX, staticY, followOrSmoothEase, time, easingType, easingRate)` at L3908 - **[INTEREST]** static camera lock; the `followOrSmoothEase` flag often drives EaseInOut actions.
- `updateStaticCameraPosToGroup(...)` at L3917
- `updateInternalCamOffsetX(offsetX, duration, easingRate)` at L3728
- `updateInternalCamOffsetY(offsetY, duration, easingRate)` at L3737
- `updateZoom(zoom, duration, easing, rate, uniqueID, controlID)` at L3944 - **[INTEREST]** zoom tween; touches `m_cameraZoom`/`m_targetCameraZoom` (snapshotted) but also probably `runAction` on the layer.
- `updateGameplayOffsetX(offsetX, staticOffset)` at L3683 - non-tween
- `updateGameplayOffsetY(offsetY, staticOffset)` at L3692
- `updateMGOffsetY(offsetY, ...)` at L3800
- `restoreDefaultGameplayOffsetX()` at L2900
- `restoreDefaultGameplayOffsetY()` at L2909
- `exitStaticCamera(exitX, exitY, time, easingType, easingRate, smoothVelocity, smoothVelocityMod, exitInstant)` at L1325 - **[INTEREST]** schedules camera-release tween.
- `resetStaticCamera(resetX, resetY)` at L2873
- `resetCamera()` at L2783 - **[INTEREST]** likely zeroes camera state; consider invoking on sim cleanup OR sniffing it for the full set of fields it resets (more thorough than our manual snapshot).
- `checkCameraLimitAfterTeleport(player, yOffset)` at L1010

Camera support fields on the layer (NOT in `GJGameState`!):
- `m_cameraFlip` at L4231
- `m_cameraWidthOffset` at L4232
- `m_cameraHeightOffset` at L4233
- `m_cameraUnzoomedHeightOffset` at L4243
- `m_targetCameraHeightOffset` at L4244
- `m_calculateTargetHeightOffset` at L4245
- `m_staticCameraShake` at L4247 - **[INTEREST]** F2 - we snapshot `m_cameraShakeEnabled` (in GJGameState) but NOT this layer-side bool.
- `m_skipCameraShake` at L4248 - **[INTEREST]** F2 - same, layer-side.
- `m_cameraWidth` L4253, `m_cameraHeight` L4254, `m_cameraUnzoomedX` L4255, `m_halfCameraWidth` L4256
- `m_cameraObb2` at L4258 - **[INTEREST]** F5 spatial cull - this is the camera's oriented bounding box; `staticObjectsInRect`/`damagingObjectsInRect` already use rect tests.
- `m_freezeStartCamera` at L4241

#### Spatial partitioning (problem F5 - spatial cull)
At L4314-4329:
- `m_sections` (`vector<vector<vector<GameObject*>*>*>`) - 2D grid of object pointers per (x-section, y-section).
- `m_nonEffectObjects` at L4315 - same shape, only "physics relevant" objects.
- `m_collisionBlockSections` at L4316
- **[INTEREST]** `m_calcNonEffectObjects` (`vector<GameObject*>`) at L4317 - **engine-side filtered list of physics-active objects in the player's section**. Matches the description in the prompt - this is the spatial cull source GD uses internally.
- `m_calcNonEffectObjectsSize` at L4318
- `m_calcCollisionBlockObjects` (+`Size`) at L4319-4320
- `m_calcCollisionBlockObjects2` (+`Size`) at L4321-4322
- `m_sectionSizes`, `m_nonEffectObjectsSizes`, `m_collisionBlockSectionSizes`, `m_nonEffectObjectsFlags` at L4323-4326
- `m_sectionXFactor` at L4327, `m_sectionYFactor` at L4328 - **[INTEREST]** F5 - x/y world coords divided by these give section indices. We can mirror this exact arithmetic to consult `m_sections` directly for our own cull.
- `m_maxGameplayY` at L4329
- Also section bounds for player: `m_leftSectionIndex`, `m_rightSectionIndex`, `m_bottomSectionIndex`, `m_topSectionIndex` at L4192-4195 - **[INTEREST]** F5 - already-computed view of which sections the player is touching THIS frame. Free spatial cull ready-made.

Section helper methods:
- `virtual addToSection(GameObject*)` at L272
- `virtual updateObjectSection(GameObject*)` at L299
- `removeObjectFromSection(GameObject*)` at L2702
- `reorderObjectSection(GameObject*)` at L2738
- `updateAllObjectSection()` at L3539
- `sortSectionVector()` at L3053
- `staticObjectsInRect(rect, enabledGroups)` at L3134 - **[INTEREST]** F5 - returns CCArray of solid blocks intersecting a rect. Use this with the player's tight rect to massively reduce collision candidates.
- `damagingObjectsInRect(rect, enabledGroups)` at L1298 - **[INTEREST]** F5 - same for hazards.

#### Tick / frame timing (problem F3 - trajectory divergence)
- `virtual update(dt)` at L74 - **[INTEREST]** F3 main update path.
- `virtual postUpdate(dt)` at L101
- `processCommands(dt, isHalfTick, isLastTick)` at L2378 - **[INTEREST]** F3 - this is the per-substep dispatch; the `isHalfTick`/`isLastTick` flags are the substep semantics that govern button vs physics order. Mirror in sim.
- `processFollowActions()` at L2396
- `processPlayerFollowActions(dt)` at L2441
- `processAdvancedFollowActions(dt)` at L2288
- `processMoveActions()` at L2414
- `processMoveActionsStep(dt, visibleFrame)` at L2423
- `processRotationActions()` at L2477
- `processTransformActions(visibleFrame)` at L2522
- `processStateObjects()` at L2513
- `processItems()` at L2405
- `getModifiedDelta(dt)` at L1604 - **[INTEREST]** F3 - applies time-warp; if our sim feeds raw dt while engine multiplies by m_timeWarp, that's a step desync.
- `m_extraDelta` (double) at L4250 - **[INTEREST]** F3 - delta accumulator across ticks. Sim should snapshot/restore or reset.
- `m_timestamp` (double) at L4370
- `m_isBetweenSteps` at L4371, `m_clickBetweenSteps` at L4372, `m_clickOnSteps` at L4373 - **[INTEREST]** F3 - half-step click bookkeeping. If sim doesn't replicate these flags, button events latch on the wrong tick.
- `m_tickIndex` at L4309
- `m_clickIndex` at L4310
- `m_currentStep` at L4277

#### Button queue (problem F3, F6 - frame-tick desync, staircase fail)
- `processQueuedButtons(dt, clearInputQueue)` at L2459 - **[INTEREST]** F3/F6 - main consumer. Note `clearInputQueue` flag.
- `queueButton(button, push, isPlayer2, timestamp)` at L2531 - **[INTEREST]** F3/F6 - the producer. Sim should clone the queue, run, then restore.
- `handleButton(down, button, isPlayer1)` at L1829 - **[INTEREST]** F3/F6 - synchronous dispatch path, parallel to queueButton.
- `buttonIsRelevant(PlayerButtonCommand const&)` at L947
- `buttonIDToButton(id)` at L938
- `getPlayerButtonID(button, player2)` at L1658
- `isPlayer2Button(button)` at L1874
- `recordAction(button, down, player2)` at L2558
- `removeReleasedButtons()` at L2720
- `shouldUseSubstepForButton(dt)` at L3026
- `m_queuedButtons` at L4278 - **[INTEREST]** F3/F6 - `vector<PlayerButtonCommand>`. Save/restore around sim, or sim runs on a clone.
- `m_queuedRecordedButtons` at L4279
- `m_queuedReplayButtons` at L4282
- `m_queuedRecordedButtonsSize` at L4286
- `m_clicks` at L4189

#### Level setup / lifecycle / save state
- `setupLayers()` at L2981
- `setupLevelStart(LevelSettingsObject*)` at L2990
- `loadLevelSettings()` at L1910
- `loadStartPosObject()` at L1919
- `loadUpToPosition(position, order, channel)` at L1928
- `applyLevelSettings(GameObject*)` at L848
- `resetLevelVariables()` at L2810 - **[INTEREST]** F3 - includes camera, audio, etc. State-machine reset; useful comparison for our restore code.
- `resetMoveOptimizedValue()` at L2819
- `resetPlayer()` at L2828
- `optimizeMoveGroups()` at L2045
- `generateTargetGroups()` at L1406
- `generateVisibilityGroups()` at L1415
- `generateSpawnRemap()` at L1397
- `m_persistentStateString` at L4287
- `m_savedPersistentStateString` at L4288
- `m_savedAttempts` at L4289
- `m_levelSettings` at L4125
- `m_objects` at L4126 - **[INTEREST]** F5 - the full level array. Avoid iterating; use sections.
- `m_collisionBlocks` at L4127 - **[INTEREST]** F5 - solid-block subset.
- `m_objectLayer` at L4169 - the cocos node where sim PlayerObject is parented.

#### Other interesting / sim-mode hooks (problem F1, F2 - "no-effects" mode)
- `flipGravity(player, flip, noEffects)` at L1343 - **[INTEREST]** F1 - the `noEffects` arg is exactly the kind of flag we want for our portal handler in sim. Setting `true` on simmed gravity flips suppresses gravity-portal particles/sound.
- `toggleDualMode(object, dual, player, noEffects)` at L3287 - **[INTEREST]** F1 - same pattern.
- `toggleFlipped(flip, noEffects)` at L3296 - **[INTEREST]** F1 - same.
- `updateTimeMod(speed, players, noEffects)` at L3926 - **[INTEREST]** F1 - same.
- `playGravityEffect(flip)` at L425 (virtual) - **[INTEREST]** F1 - blockable hook.
- `playExitDualEffect(player)` at L2180 - **[INTEREST]** F1 - same.
- `playFlashEffect(duration, flashes, unknown)` at L2189 - **[INTEREST]** F1 - same.
- `playKeyframeAnimation(KeyframeAnimTriggerObject*, remapKeys)` at L2198
- `playAnimationCommand(id, groupID)` at L2108
- `playSpeedParticle(timeMod)` at L2207 - listed above.
- `addCustomEnterEffect(EnterEffectObject*, enter)` at L713
- `bumpPlayer(player, EffectGameObject*)` at L929 - **[INTEREST]** F4 - orb activation entry.
- `gravBumpPlayer(player, object)` at L1811 - **[INTEREST]** F4 - same for gravity rings.
- `canBeActivatedByPlayer(player, EffectGameObject*)` at L983 - **[INTEREST]** F4 - the predicate the engine uses to gate orb activations. If we hook this, we control which orbs can fire during sim.
- `canTouchObject(object)` at L1001 - **[INTEREST]** F4 - generic touch gate.
- `playerCircleCollision(player, object)` at L2117 - **[INTEREST]** F4 - precise circle test for orbs/rings.
- `playerIntersectsCircle(player, object)` at L2126
- `objectIntersectsCircle(object, circle)` at L2027
- `rectIntersectsCircle(rect, center, radius)` at L2567
- `playerTouchedObject(player, object)` at L2135 - **[INTEREST]** F4 - generic dispatcher.
- `playerTouchedRing(player, RingObject*)` at L2144 - **[INTEREST]** F4 - ring-specific.
- `playerTouchedTrigger(player, EffectGameObject*)` at L2153
- `playerWasTouchingObject(player, object)` at L2162 - **[INTEREST]** F4 - frame-prior touch query; if sim doesn't seed this for the sim player (sharing real player's m_touchingRings would do it), false-positives.
- `playerWillSwitchMode(player, GameObject*)` at L2171
- `pickupItem(EffectGameObject*)` at L2099
- `collectedObject(EffectGameObject*)` at L1109 - **[INTEREST]** F1 - collect/pickup particles spawn here.
- `hasItem(id)` at L1838, `hasUniqueCoin(EffectGameObject*)` at L1847
- `clearPickedUpItems()` at L1100
- `m_collectedItems` at L4235

Portal-related:
- `getPortalTarget(TeleportPortalObject*)` at L1685
- `getPortalTargetPos(TeleportPortalObject*, target, player)` at L1694
- `teleportPlayer(TeleportPortalObject*, player)` at L3260 - **[INTEREST]** F2/F3 - mutates camera and `m_lastActivatedPortal*` via state. Bracket with snapshot.
- `animatePortalY(fromY, toY, duration, easingRate)` at L839
- `getMaxPortalY()` at L1577, `getMinPortalY()` at L1595
- `m_endPortal` at L4239
- Mode switches: `switchToFlyMode/RobotMode/RollMode/SpiderMode` at L3215-3242 - all take `noPortal`.

Collision:
- `checkCollision(blockAID, blockBID)` at L1019
- `checkCollisionBlocks(EffectGameObject*, blocks, blockCount)` at L1028
- `checkCollisions(player, dt, ignoreDamage)` at L1037 - **[INTEREST]** F3/F5 - main player collision routine. Takes ignoreDamage flag - useful.
- `collisionCheckObjects(player, objects, objectCount, dt)` at L1118 - **[INTEREST]** F3/F5 - the inner loop. If we feed our own filtered list (sections in player's range only) we avoid the full m_objects scan.
- `objectsCollided(blockAID, blockBID)` virtual at L182
- `checkRepellPlayer()` at L1046
- `checkSpawnObjects()` at L1055
- `m_disablePlayerHitbox` at L4367
- `m_hitboxesOnDeath` at L4368
- `m_anticheatSpike` at L4369
- `m_isDebugDrawEnabled` at L4366

Pause/audio:
- `pauseAudio()` at L2072, `resumeAudio()` at L2927, `tryResumeAudio()` at L3494, `resetAudio()` at L2774
- `processSFXObjects()` at L2486, `processQueuedAudioTriggers()` at L2450, `processActivatedAudioTriggers(levelTime)` at L2270
- `m_processingAudioTriggers` at L4360, `m_audioPaused` at L4361, `m_audioEffectsLayer` at L4257
- `m_resumeTimer` at L4263 - **[INTEREST]** F2 - layer-side. Could change during sim and not be snapshotted.

Replay/record:
- `m_useReplay` at L4217, `m_recordInputs` at L4264, `m_recordString` at L4269
- `m_replayRandSeed` at L4275, `m_randomSeed` at L4273
- `setupReplay(inputs)` at L2999, `getRecordString(compress)` at L1712
- `processReplayCheckpoint(id)` at L2468, `updateReplay()` at L3863

State / mode flags:
- `m_isPracticeMode` at L4226
- `m_practiceMusicSync` at L4227
- `m_isTestMode` at L4240
- `m_isPlatformer` at L4198
- `m_isEditor` at L4196
- `m_blending` at L4197
- `m_playerDied` at L4249
- `m_started` at L4251
- `m_loadingProgress` at L4228
- `m_loadingStartPosition` at L4359
- `m_startOptimization` at L4362
- `m_levelEndAnimationStarted` at L4311
- `m_dualTouchTrigger` at L4188
- `m_attempts` at L4190
- `m_jumping` at L4191
- `m_levelLength` at L4236
- `m_resetActiveObjects` at L4237
- `m_skipArtReload` at L4238
- `m_increasedLayerCapacity` at L4182
- `m_objectsDeactivated` at L4214
- `m_areaObjectsUpdated` at L4215
- `m_startPosObject` at L4216

Active object lists (per-section / per-frame caches):
- `m_activeObjects` at L4259, `m_activeObjectsCount/Index` at L4260-4261
- `m_visibleObjects` at L4137, `m_visibleObjects2` at L4140 (with counts/indices)
- `m_solidCollisionObjects` at L4221, `m_hazardCollisionObjects` at L4224 (with counts/indices) - **[INTEREST]** F5 - already-classified-by-type lists. If accurate per-frame, F5 spatial cull becomes "iterate m_solidCollisionObjects + m_hazardCollisionObjects, all in player section".
- `m_disabledObjects` at L4132, `m_areaObjects` at L4134, `m_processedAreaObjects` at L4135
- `m_sequenceTriggers` at L4225
- `m_activeSfxTriggers` at L3974
- `m_savePositionObjects` at L4299

Group / target group / spawn machinery (probably irrelevant to our F1-F6):
- `m_groupDict`, `m_staticGroupDict`, `m_optimizedGroupDict`, `m_groups`, `m_staticGroups`, `m_optimizedGroups` at L4153-4158
- `m_parentGroupsDict`, `m_parentGroupIDs`, `m_removedParentGroupIDs` at L4159-4161
- `m_targetGroupsArray`, `m_targetGroups` at L4162-4163
- `m_linkedGroupDict` at L4164
- `m_stickyGroups` at L4331
- `m_keyframeGroups` at L4302, `m_keyframeGroup` at L4303
- `m_visibilityGroups` at L4136
- `m_spawnObjectsArray`, `m_spawnObjects` at L4128-4129
- `m_spawnTuples` at L4181
- `m_spawnRemapTriggers` at L3983

Counts / labels:
- `m_areaMovedCount/Total/Display` and `Scaled/Rotated/Color` family at L4335-4358
- `m_movedCount/scaledCount/rotatedCount/followedCount` (and Display variants)
- `m_labelObjects` at L4179, `m_timeLabelObjects` at L4180
- `m_uiLayer` at L4304, `m_uiObjects` at L4305, `m_uiObjectLayers` at L4306, `m_uiTriggerUI` at L4307, `m_uiObjectPositions` at L3984
- `m_points` at L4312, `m_pointsString` at L4313
- `m_indicatorSprites` at L4292, `m_portalIndicators` at L4290, `m_orbIndicators` at L4291

Misc:
- `m_lowDetailMode` at L3967, `m_extraLDM` at L3968
- `m_ignoreDamage` at L3969 - **[INTEREST]** F3 - if true the engine skips damage. Sim could flip this at top of run.
- `m_enable22Changes` at L3970, `m_allowStaticRotate` at L3971, `m_fixNegativeScale` at L3972
- `m_startingFromBeginning` at L3973
- `m_player1` at L4123, `m_player2` at L4124
- `m_player1CollisionBlock` at L4199, `m_player2CollisionBlock` at L4200
- `m_obb2` at L3982, `m_cameraObb2` at L4258
- `m_areaTransformNode` etc. at L3977-3981
- `m_effectManager` at L3985
- `m_groundLayer`, `m_groundLayer2` at L4174-4175, `m_middleground` at L4176
- `m_background` (sprite) at L4172
- `m_shaderLayer` at L4213, `m_aboveShaderObjectLayer/inShaderObjectLayer` at L4170-4171
- `m_gradientLayers`, `m_activeGradients` at L4211-4212
- `m_audioVisualizerBG/SFX` at L4332-4333, `m_showAudioVisualizer` at L4334
- `m_objectsToDeactivate` at L4178, `m_objectsToMove` at L4298, `m_destroyObjectValues` at L4184
- `m_varianceValues[2000]` at L4183
- `m_enterEasingValues/Indices/ValuesIndex` at L4185-4187
- `m_enable22Changes`, `m_dualTouchTrigger` covered above.

Did NOT find: anything literally named `isSimulation`, `noEffects` *flag* (only as parameter), or `preview` mode toggles. So GD has no built-in sim mode we can ride on.

---

### `GJGameState.hpp` (251 lines) - Snapshot target struct **[HEAVY READ]**

Fields list (ALL fields, with snapshot status from `Trajectory.cpp::LayerStateSnapshot`):

| Line | Field | Type | Snapshot? | Notes |
|---|---|---|---|---|
| 89 | `m_cameraZoom` | `float` | YES | |
| 90 | `m_targetCameraZoom` | `float` | YES | |
| 91 | `m_cameraOffset` | `CCPoint` | YES | |
| 92-120 | `m_unkPoint1..29` | `CCPoint` x29 | NO | **[INTEREST]** F2 - ~29 unknown CCPoint fields. Unidentified, but in the same struct region as camera state. Some may be intermediate camera/easing state used by tween actions; if so, leaking. |
| 121 | `m_unkBool1` | `bool` | NO | unknown |
| 122 | `m_unkInt1` | `int` | NO | unknown |
| 123-125 | `m_unkBool2,m_unkInt2,m_unkBool3` | NO | unknown |
| 126 | `m_unkPoint30` | `CCPoint` | NO | |
| 127 | `m_middleGroundOffsetY` | `float` | NO | **[INTEREST]** F2 - middleground Y offset; updateMGOffsetY tween writes here. Not snapshotted. |
| 128-141 | `m_unkInt3..11`, `m_unkBool4,5`, `m_unkFloat2,3` | NO | mixed |
| 142 | `m_unkUint1` (typed as float) | NO | |
| 143 | `m_portalY` | `float` | YES | |
| 144 | `m_unkBool6` | `bool` | NO | |
| 145 | `m_gravityRelated` | `bool` | YES | |
| 146-149 | `m_unkInt12..15`, `m_unkInt13`(float) | NO | |
| 150-152 | `m_unkBool7,8,9` | `bool` x3 | NO | |
| 153-156 | `m_unkFloat5..8` | NO | |
| 157 | `m_cameraAngle` | `float` | YES | |
| 158 | `m_targetCameraAngle` | `float` | YES | |
| 159 | `m_playerStreakBlend` | `bool` | NO | **[INTEREST]** F2 - togglePlayerStreakBlend(blend, force) at L3323 of GJBaseGameLayer mutates this. Not snapshotted - if sim crosses a streak-blend trigger, real player gets blend toggled. |
| 160 | `m_timeWarp` | `float` | YES | |
| 161 | `m_queuedTimeWarp` | `float` | YES | |
| 162 | `m_timeWarpRelated` | `float` | YES | |
| 163 | `m_currentChannel` | `int` | YES | |
| 164 | `m_rotateChannel` | `int` | YES | |
| 165 | `m_spawnChannelRelated0` | `unordered_map<int,int>` | NO | non-trivial container |
| 166 | `m_spawnChannelRelated1` | `unordered_map<int,bool>` | NO | non-trivial container |
| 167 | `m_totalTime` | `double` | NO | **[INTEREST]** F3 - total elapsed gameplay time. Sim ticks could advance this; though probably gated by `update` which we hook. |
| 168 | `m_levelTime` | `double` | NO | **[INTEREST]** F3 - level time. Same concern. |
| 169 | `m_unkDouble3` | `double` | NO | |
| 170 | `m_commandIndex` | `unsigned int` | NO | **[INTEREST]** F3 - if sim's processCommands advances this, real frame skips commands. |
| 171 | `m_unkUint3` | `float` | NO | |
| 172 | `m_currentProgress` | `unsigned int` | NO | progress percent? |
| 173-177 | `m_unkUint4..8` | `int` | NO | |
| 178 | `m_lastActivatedPortal1` | `GameObject*` | YES | |
| 179 | `m_lastActivatedPortal2` | `GameObject*` | YES | |
| 180 | `m_cameraPosition` | `CCPoint` | YES | |
| 181 | `m_unkBool10` | `bool` | NO | |
| 182 | `m_levelFlipping` | `float` | YES | |
| 183-184 | `m_unkBool11,12` | NO | |
| 185 | `m_isDualMode` | `bool` | YES | |
| 186 | `m_unkFloat9` | `float` | NO | |
| 187 | `m_tweenActions` | `unordered_map<int,GJValueTween>` | NO | non-trivial; intentionally skipped per Trajectory.cpp comment. **[INTEREST]** F2 - if a sim's updateZoom/updateScreenRotation/etc inserts an entry, restore() doesn't remove it. The TrajEffectHook gating must be airtight. |
| 188-191 | `m_cameraEdgeValue0..3` | `int` x4 | YES | |
| 192 | `m_gameObjectPhysics` | `unordered_map<int,GameObjectPhysics>` | NO | non-trivial; populated by modifyGroupPhysics/modifyObjectPhysics. |
| 193 | `m_unkVecFloat1` | `vector<float>` | NO | |
| 194-196 | `m_unkUint10,11,12` | NO | |
| 197 | `m_cameraStepDiff` | `CCPoint` | YES | |
| 198 | `m_unkFloat10` | `float` | NO | |
| 199 | `m_timeModRelated` | `float` | NO | **[INTEREST]** F3 - time-mod auxiliary; updateTimeMod(speed, players, noEffects) probably writes this. NOT snapshotted. If sim crosses speed portal and real player doesn't, time-mod state leaks. |
| 200 | `m_timeModRelated2` | `bool` | NO | **[INTEREST]** F3 - same. |
| 201 | `m_activatedObjectIDs` | `map<pair<int,int>,int>` | NO | **[INTEREST]** F4 - this looks like the orb-already-activated dedupe. If sim writes here, real player's orb dedupe is poisoned. (Maps from (group,id) -> something, plausibly tickIndex.) |
| 202 | `m_unkUint13` | `float` | NO | |
| 203 | `m_unkPoint32` | `CCPoint` | NO | |
| 204 | `m_cameraPosition2` | `CCPoint` | YES | |
| 205-207 | `m_unkBool20,21,22` | NO | |
| 208 | `m_unkUint14` | `float` | NO | |
| 209 | `m_unkBool26` | `bool` | NO | |
| 210 | `m_cameraShakeEnabled` | `bool` | YES | |
| 211 | `m_cameraShakeFactor` | `float` | YES | |
| 212-213 | `m_unkUint15,16` | NO | |
| 214 | `m_unkUint64_1` | `double` | NO | |
| 215 | `m_unkPoint34` | `CCPoint` | NO | |
| 216 | `m_dualRelated` | `unsigned int` | YES | |
| 217 | `m_stateObjects` | `unordered_map<int,EnhancedGameObject*>` | NO | non-trivial; processStateObjects iterates. |
| 218-220 | `m_unkMapPair*` event-trigger maps | NO | non-trivial |
| 220 | `m_enterEffectInstanceVectors` | `unordered_map<int,vector<EnterEffectInstance>>` | NO | non-trivial |
| 221 | `m_exitEffectInstanceVectors` | same | NO | non-trivial |
| 222 | `m_enterChannelMap` | `vector<int>` | NO | |
| 223 | `m_exitChannelMap` | `vector<int>` | NO | |
| 224-228 | `m_moveEffectInstances`, `m_rotateEffectInstances`, `m_scaleEffectInstances`, `m_fadeEffectInstances`, `m_tintEffectInstances` | `vector<EnterEffectInstance>` x5 | NO | non-trivial |
| 229 | `m_unsortedAreaEffects` | `unordered_set<int>` | NO | |
| 230 | `m_unkBool27` | NO | |
| 231 | `m_advanceFollowInstances` | `vector<AdvancedFollowInstance>` | NO | non-trivial |
| 232 | `m_dynamicMoveActions` | `vector<DynamicObjectAction>` | NO | non-trivial |
| 233 | `m_dynamicRotateActions` | `vector<DynamicObjectAction>` | NO | non-trivial |
| 234-235 | `m_unkBool28,29` | NO | |
| 236 | `m_unkUint17` | NO | |
| 237 | `m_unkUMap8` | `unordered_map<int,vector<int>>` | NO | |
| 238 | `m_proximityVolumeRelated` | `map<pair<int,int>,SFXTriggerInstance>` | NO | |
| 239 | `m_songChannelStates` | `unordered_map<int,SongChannelState>` | NO | |
| 240 | `m_songTriggerStateVectors` | `unordered_map<int,vector<SongTriggerState>>` | NO | |
| 241 | `m_sfxTriggerStates` | `vector<SFXTriggerState>` | NO | |
| 242 | `m_unkBool30` | NO | |
| 243 | `m_background` | `int` | NO | |
| 244 | `m_ground` | `int` | NO | |
| 245 | `m_middleground` | `int` | NO | |
| 246 | `m_unkBool31` | NO | |
| 247 | `m_points` | `int` | NO | |
| 248 | `m_unkBool32` | NO | |
| 249 | `m_pauseCounter` | `unsigned int` | NO | |
| 250 | `m_pauseBufferTimer` | `unsigned int` | NO | |

GJGameState methods:
- `controlTweenAction(uniqueID, controlID, GJActionCommand)` at L34
- `getGameObjectPhysics(GameObject*)` at L43 - returns `&` to inserted entry; mutating call.
- `processStateTriggers()` at L52
- `stopTweenAction(action)` at L61
- `tweenValue(from, to, action, duration, easing, rate, uniqueID, controlID)` at L70 - **[INTEREST]** F2 - inserts into `m_tweenActions` map (NOT snapshotted).
- `updateTweenAction(value, action)` at L79
- `updateTweenActions(tweenValue)` at L88 - **[INTEREST]** F2 - the per-frame tick of all tweens. If sim runs this, real player's tweens advance ahead.

**Summary of GJGameState findings re: snapshot gaps:**
1. `m_tweenActions` map (L187) NOT snapshotted - only "safe" if our TrajEffectHook prevents `tweenValue()` from running during sim. Worth verifying.
2. `m_activatedObjectIDs` (L201) - probable orb-dedupe table. F4.
3. `m_playerStreakBlend` (L159), `m_middleGroundOffsetY` (L127), `m_timeModRelated` (L199), `m_timeModRelated2` (L200), `m_totalTime` (L167), `m_levelTime` (L168), `m_commandIndex` (L170), `m_currentProgress` (L172), `m_pauseCounter` (L249), `m_pauseBufferTimer` (L250) - all single-field POD that can be added to LayerStateSnapshot trivially.
4. The 29 `m_unkPoint*` fields in L92-120 are most worrying; they are in the camera-state region and any could carry tween intermediates.

---

### `GameObject.hpp` (2546 lines) **[HEAVY READ]**

Fields (relevant subset, L2350-2546):
- `m_objectRect` at L2385 - **[INTEREST]** F5/F3 - cached rect; touch via `getObjectRectPointer()` at L1492.
- `m_isObjectRectDirty` at L2386 - **[INTEREST]** F5 - if dirty bit is sticky we can prefilter cheaply.
- `m_orientedBox` (`OBB2D*`) at L2364 - **[INTEREST]** F3 - exact rotated hitbox. Critical for spike/saw collision. `getOrientedBox()` virtual at L556.
- `m_isOrientedBoxDirty` at L2387
- `m_objectType` (`GameObjectType`) at L2407 - **[INTEREST]** F5 - enum: spike/saw/portal/ring/orb/etc. Filter on this.
- `m_savedObjectType` at L2408
- `m_objectID` at L2439 - **[INTEREST]** F5 - level-format ID number. Spike=8, regular=1, jumppad=37, etc.
- `m_classType` (`GameObjectClassType`) at L2505
- `m_isInvisible` at L2427 - **[INTEREST]** F5 - skip in cull.
- `m_isInvisibleBlock` at L2460
- `m_isHide` at L2523 - **[INTEREST]** F5 - same.
- `m_isDisabled` at L2417 - **[INTEREST]** F5 - skip.
- `m_isDisabled2` at L2372 - **[INTEREST]** F5 - skip.
- `m_isActivated` at L2371 - relevant for orbs/triggers.
- `m_isDecoration` at L2518 - **[INTEREST]** F5 - decoration; skip for collision.
- `m_isDecoration2` at L2519 - **[INTEREST]** F5 - same.
- `m_isPassable` at L2522 - **[INTEREST]** F5 - skip.
- `m_isNoTouch` at L2498 - **[INTEREST]** F5 - skip for hitbox; visual only.
- `m_isTrigger` at L2506 - **[INTEREST]** F5 - skip for collision.
- `m_isSpawnOrderTrigger` at L2507
- `m_isColorTrigger` at L2508
- `m_isStartPos` at L2512 - **[INTEREST]** F5 - skip.
- `m_isHighDetail` at L2513
- `m_hasNoEffects` at L2450 - **[INTEREST]** F1 - object opt-out flag for particles already exists per-object.
- `m_hasNoParticles` at L2451 - **[INTEREST]** F1 - same.
- `m_hasNoGlow` at L2437
- `m_hasNoAudioScale` at L2416
- `m_isUIObject` at L2544 - **[INTEREST]** F5 - skip.
- `m_uniqueID` at L2406
- `m_groups` (array<short,10>*) at L2486, `m_groupCount` at L2487
- `m_colorGroups` at L2489, `m_colorGroupCount` at L2490
- `m_opacityGroups` at L2491, `m_opacityGroupCount` at L2492
- `m_objectRadius` at L2402 - **[INTEREST]** F5 - radius for circle tests.
- `m_width` at L2368, `m_height` at L2369 - **[INTEREST]** F5/F3 - explicit collision dims.
- `m_isFlipX/Y` at L2359-2360, `m_startFlipX/Y` at L2424-2425
- `m_scaleX/Y` at L2484-2485, `m_customScaleX/Y` at L2422-2423
- `m_unmodifiedPositionX/Y` at L2410-2411, `m_positionX/Y` (double) at L2412-2413, `m_startPosition` at L2414
- `m_zLayer/m_defaultZLayer/m_zOrder/m_defaultZOrder` at L2472-2475, L2452
- `m_baseColor`, `m_detailColor` at L2469-2470
- `m_particle` (CCParticleSystemQuad*) at L2373 - **[INTEREST]** F1 - per-object particle handle. Nulling/pausing during sim might be too coarse but useful.
- `m_particleString` at L2374, `m_hasParticles` at L2375, `m_isParticleSpriteLocked` at L2379, `m_particleOffset` at L2378
- `m_particleUseObjectColor` at L2376, `m_particleLocked` at L2458
- `m_isRingPoweredOn` at L2367 - **[INTEREST]** F4 - ring-on state; probably mutated by ring activation.
- `m_slopeUphill/m_slopeDirection/m_slopeIsHazard` at L2465-2467
- `m_isIceBlock`, `m_isGripSlope`, `m_isScaleStick`, `m_isExtraSticky`, `m_isDontBoostX/Y`, `m_isNonStickX/Y` at L2524-2532
- `m_isSolidColorBlock` at L2444

Key virtual methods:
- `virtual getObjectRect()` at L295 / `getObjectRect(width, height)` at L304 / `getObjectRect2(width, height)` at L313 - **[INTEREST]** F3/F5 - hitbox accessors. Check m_isObjectRectDirty.
- `getObjectRectDirty() const` virtual at L790
- `getObjectRectPointer()` at L1492
- `getOuterObjectRect()` at L1519 - **[INTEREST]** F5 - inflated rect; useful for cheap pre-pass.
- `virtual getOrientedBox()` at L556, `updateOrientedBox()` at L565
- `virtual getType() const` at L826
- `virtual deactivateObject(deactivate)` at L277
- `virtual claimParticle()` at L376 - **[INTEREST]** F1 - per-object particle claim. Hookable.
- `virtual unclaimParticle()` at L385
- `virtual particleWasActivated()` at L394
- `virtual activatedByPlayer(PlayerObject*)` at L529
- `virtual hasBeenActivatedByPlayer(PlayerObject*)` at L538 - **[INTEREST]** F4 - the canonical "did this player already activate this orb" check.
- `virtual hasBeenActivated()` at L547
- `virtual transferObjectRect(rect&)` at L286
- `getObjectTextureRect()` virtual at L322
- `getRealPosition()` virtual at L331
- `setStartPos(position)` virtual at L340
- `triggerActivated(xPosition)` virtual at L475
- `getObjectRotation()` virtual at L574
- `restoreObject()` virtual at L502 - **[INTEREST]** F4 - reset to fresh state; useful if sim mutated something we didn't snapshot.

**Summary for F5 spatial cull:**
The `m_objectType`/`m_objectID`/`m_isInvisible`/`m_isDecoration`/`m_isDecoration2`/`m_isPassable`/`m_isNoTouch`/`m_isTrigger`/`m_hasNoEffects`/`m_hasNoParticles` fields together let us write a tight predicate without calling any vtable. Combined with m_section indices, F5 should be a pure data-walk.

---

## SKIP files (UI/menu/networking/editor/etc.)

- `GameCell.hpp` - cell for level browser, UI/menu skip.
- `GameLevelManager.hpp` (2870 lines) - online level fetch/cache (downloadLevel, getLevels, getLevelComments, getMapPacks); **[INTEREST mild]** holds `m_mainLevels` dict and is the canonical singleton for offline/online level data, but no gameplay-physics relevance. Skip.
- `GameLevelOptionsLayer.hpp` - options popup, UI/menu skip.
- `GameOptionsLayer.hpp` - options popup, UI/menu skip.
- `GameOptionsTrigger.hpp` - **[INTEREST mild]** trigger object with EffectGameObject parent; `processOptionsTrigger` in GJBaseGameLayer L2432. Sim-relevant in that it touches gameplay options like jump-hold etc.; one-line.
- `GameRateDelegate.hpp` - delegate header, skip.
- `GameStatsManager.hpp` (1574 lines) - achievements/stats singleton (`incrementStat`, `awardCurrencyForLevel`, persistent diamonds/orbs/coins). No gameplay-physics relevance. Skip.
- `GameToolbox.hpp` (508 lines) - static utility funcs (string helpers, color blending, easings, formatTime). Pure helpers, no state. Skip but worth a peek for any spatial helpers.
- `GauntletLayer.hpp` - UI/menu skip.
- `GauntletNode.hpp` - UI/menu skip.
- `GauntletSelectLayer.hpp` - UI/menu skip.
- `GauntletSprite.hpp` - UI sprite, skip.
- `GameObjectCopy.hpp` - editor copy/paste data, skip.
- `GameObjectEditorState.hpp` - editor state, skip.
- `GJAccountBackupDelegate.hpp` - networking delegate, skip.
- `GJAccountDelegate.hpp` - networking delegate, skip.
- `GJAccountLoginDelegate.hpp` - networking delegate, skip.
- `GJAccountManager.hpp` - account singleton, skip.
- `GJAccountRegisterDelegate.hpp` - networking delegate, skip.
- `GJAccountSettingsDelegate.hpp` - delegate, skip.
- `GJAccountSettingsLayer.hpp` - UI, skip.
- `GJAccountSyncDelegate.hpp` - networking delegate, skip.
- `GJActionManager.hpp` - **[INTEREST mild]** runs `GJValueTween`-style actions (controlAction/processAction) over time. If sim crosses a controlled tween it could mutate state. Skim later if F2 not solved by snapshot+gating; one-line for now.
- `GJAssetDownloadAction.hpp` - asset download, skip.
- `GJBigSprite.hpp` - decoration sprite, skip.
- `GJBigSpriteNode.hpp` - decoration node, skip.
- `GJChallengeDelegate.hpp` - delegate, skip.
- `GJChallengeItem.hpp` - challenge data, skip.
- `GJChestSprite.hpp` - chest UI sprite, skip.
- `GJColorSetupLayer.hpp` - editor color setup UI, skip.
- `GJComment.hpp` - comment data, skip.
- `GJCommentListLayer.hpp` - UI list, skip.
- `GJDailyLevelDelegate.hpp` - delegate, skip.
- `GJDifficultySprite.hpp` - UI sprite, skip.
- `GJDropDownLayer.hpp` - UI layer base, skip.
- `GJDropDownLayerDelegate.hpp` - UI delegate, skip.
- `GJEffectManager.hpp` (1129 lines) - **[INTEREST]** color/pulse trigger manager; holds `m_colorActionDict`, `m_colorActionVector`, `m_colorActionSpriteVector` at L1086-1091. Owns `colorActionChanged(ColorAction*)` at L143. Color-channel state is potentially mutated by triggers we cross during sim, but visual-only and not in the player-physics path. Hooked from GJBaseGameLayer through `m_effectManager`. Mostly skip; revisit only if a color-pulse trigger has a side-channel effect.
- `GJFlyGroundLayer.hpp` - **[INTEREST mild]** ship/ufo ground layer; visual. Skip with note.
- `GJFollowCommandLayer.hpp` - editor follow-command UI, skip.
- `GJFriendRequest.hpp` - networking data, skip.
- `GJGameLevel.hpp` (604 lines) - level metadata (string blob, name, attempts, normal/practice percent, etc.). Read by setupLevelStart but doesn't mutate during play. Skip.
- `GJGameLoadingLayer.hpp` - loading screen, skip.
- `GJGarageLayer.hpp` - icon customization UI, skip.
- `GJGradientLayer.hpp` - **[INTEREST mild]** the gradient layer object; spawned per gradient trigger. Visual; skip with note.
- `GJGroundLayer.hpp` (236 lines) - **[INTEREST mild]** ground rendering + ground physics height (`m_unkArr1` etc.). `getGroundHeight` lives on GJBaseGameLayer; this is visual. Skip.
- `GJHttpResult.hpp` - networking result, skip.
- `GJItemIcon.hpp` - UI icon, skip.
- `GJLevelList.hpp` - level list data, skip.
- `GJLevelScoreCell.hpp` - UI cell, skip.
- `GJListLayer.hpp` - UI list, skip.
- `GJLocalLevelScoreCell.hpp` - UI cell, skip.
- `GJLocalScore.hpp` - score data, skip.
- `GJMapObject.hpp` - world map object, skip.
- `GJMapPack.hpp` - map pack data, skip.
- `GJMessageCell.hpp` - UI cell, skip.
- `GJMessagePopup.hpp` - UI popup, skip.
- `GJMGLayer.hpp` - middleground rendering layer, skip (visual).
- `GJMoreGamesLayer.hpp` - UI, skip.
- `GJMPDelegate.hpp` - multiplayer delegate, skip.
- `GJMultiplayerManager.hpp` - multiplayer mgr, skip.
- `GJObjectDecoder.hpp` (43 lines) - object decoder helper; static methods only. Skip.
- `GJOnlineRewardDelegate.hpp` - networking delegate, skip.
- `GJOptionsLayer.hpp` - options UI, skip.
- `GJPathPage.hpp` - UI page, skip.
- `GJPathRewardPopup.hpp` - UI popup, skip.
- `GJPathsLayer.hpp` - UI, skip.
- `GJPathSprite.hpp` - UI sprite, skip.
- `GJPFollowCommandLayer.hpp` - editor UI, skip.
- `GJPointDouble.hpp` - struct holding two doubles; pure data, skip.
- `GJPromoPopup.hpp` - UI popup, skip.
- `GJPurchaseDelegate.hpp` - delegate, skip.
- `GJRateLevelLayer.hpp` - UI, skip.
- `GJRequestCell.hpp` - UI cell, skip.
- `GJRewardDelegate.hpp` - delegate, skip.
- `GJRewardItem.hpp` - reward data, skip.
- `GJRewardObject.hpp` - reward data, skip.
- `GJRobotSprite.hpp` - icon sprite, skip.
- `GJRotateCommandLayer.hpp` - editor UI, skip.
- `GJRotationControl.hpp` - editor UI, skip.
- `GJRotationControlDelegate.hpp` - delegate, skip.
- `GJScaleControl.hpp` - editor UI, skip.
- `GJScaleControlDelegate.hpp` - delegate, skip.
- `GJScoreCell.hpp` - UI cell, skip.
- `GJSearchObject.hpp` - search params, skip.
- `GJShaderState.hpp` - shader state struct; visual. Skip.
- `GJShopLayer.hpp` - UI, skip.
- `GJSmartBlockPreview.hpp` - editor preview, skip.
- `GJSmartBlockPreviewSprite.hpp` - editor sprite, skip.
- `GJSmartPrefab.hpp` - editor data, skip.
- `GJSmartTemplate.hpp` - editor data, skip.
- `GJSongBrowser.hpp` - song browser UI, skip.
- `GJSpecialColorSelect.hpp` - UI, skip.
- `GJSpecialColorSelectDelegate.hpp` - delegate, skip.
- `GJSpiderSprite.hpp` - icon sprite, skip.
- `GJSpriteColor.hpp` - color obj data; referenced by GameObject `m_baseColor`/`m_detailColor`. Visual. Skip.
- `GJStoreItem.hpp` - shop data, skip.
- `GJTransformControl.hpp` - editor UI, skip.
- `GJTransformControlDelegate.hpp` - delegate, skip.
- `GJTransformState.hpp` - editor undo data, skip.
- `GJUINode.hpp` - UI helper, skip.
- `GJUnlockableItem.hpp` - achievement data, skip.
- `GJUserCell.hpp` - UI cell, skip.
- `GJUserMessage.hpp` - networking data, skip.
- `GJUserScore.hpp` - score data, skip.
- `GJWorldNode.hpp` - world map node, skip.
- `GJWriteMessagePopup.hpp` - UI popup, skip.
- `GManager.hpp` - generic singleton base, skip.
- `GooglePlayDelegate.hpp` - networking delegate, skip.
- `GooglePlayManager.hpp` - networking mgr, skip.
- `GraphicsReloadLayer.hpp` - UI loading, skip.
- `GroupCommandObject2.hpp` - **[INTEREST mild]** EffectGameObject subclass holding a per-group command queue (move/rotate/scale data). Probably touched by triggerMoveCommand etc. State here might leak across sim if hit.
- `HSVLiveOverlay.hpp` - UI overlay, skip.
- `HSVWidgetDelegate.hpp` - delegate, skip.
- `HSVWidgetPopup.hpp` - UI popup, skip.
- `InfoAlertButton.hpp` - UI button, skip.
- `InfoLayer.hpp` - UI info popup, skip.
- `ItemInfoPopup.hpp` - UI popup, skip.
- `KeybindingsLayer.hpp` - UI, skip.
- `KeybindingsManager.hpp` - input mgr; relevant only for binding changes, not sim. Skip.
- `LeaderboardManagerDelegate.hpp` - networking delegate, skip.
- `LeaderboardsLayer.hpp` - UI, skip.
- `LevelAreaInnerLayer.hpp` - UI/world map, skip.
- `LevelAreaLayer.hpp` - UI/world map, skip.
- `LevelBrowserLayer.hpp` - UI, skip.
- `LevelCell.hpp` - UI cell, skip.
- `LevelCommentDelegate.hpp` - networking delegate, skip.
- `LevelDeleteDelegate.hpp` - networking delegate, skip.
- `LevelDownloadDelegate.hpp` - networking delegate, skip.
- `LevelEditorLayer.hpp` (1599 lines) - editor; not active during play. **[INTEREST mild]** subclasses GJBaseGameLayer; if there's an editor preview path that runs a slim sim it could be a reference, but skip for now. We're solving PlayLayer-side bugs.
- `LevelFeatureLayer.hpp` - UI, skip.
- `LevelInfoLayer.hpp` - UI, skip.
- `LevelLeaderboard.hpp` - networking/UI, skip.
- `LevelListCell.hpp` - UI cell, skip.
- `LevelListDeleteDelegate.hpp` - networking delegate, skip.
- `LevelListLayer.hpp` - UI, skip.
- `LevelManagerDelegate.hpp` - delegate, skip.
- `LevelOptionsLayer.hpp` - UI, skip.
- `LevelOptionsLayer2.hpp` - UI, skip.
- `LevelPage.hpp` - UI, skip.
- `LevelRateInfoDelegate.hpp` - networking delegate, skip.
- `LevelSearchLayer.hpp` - UI, skip.
- `LevelSelectLayer.hpp` - UI, skip.
- `LevelSettingsDelegate.hpp` - delegate, skip.
- `LevelSettingsLayer.hpp` - editor UI, skip.
- `LikeItemDelegate.hpp` - networking delegate, skip.
- `LikeItemLayer.hpp` - UI, skip.
- `ListButtonBar.hpp` - UI, skip.
- `ListButtonBarDelegate.hpp` - delegate, skip.
- `ListButtonPage.hpp` - UI, skip.
- `ListCell.hpp` - UI cell, skip.
- `ListUploadDelegate.hpp` - networking delegate, skip.
- `LoadingCircle.hpp` - UI, skip.
- `LoadingCircleSprite.hpp` - UI, skip.
- `LoadingLayer.hpp` - UI loading screen, skip.
- `LocalLevelManager.hpp` (200 lines) - local level singleton (saved levels list, custom songs, level orders). Mutates only on user save action; not during play. Skip.

---

## LOOK-FOR files (gameplay/physics relevant)

- `GameEffectsManager.hpp` (45 lines) - **[INTEREST]** F1 - tiny class with `addParticleEffect(particle, unused)` at L29 and `scaleParticle(particle, scale)` at L43. Owns `m_playLayer`. This is a thin facade over `m_playLayer->m_*ParticleLayer*`. If we're trying to suppress particles, scaleParticle to 0 on entry is one mechanism. Init `init(playLayer)` at L36.

- `GameObjectPhysics.hpp` (23 lines) - **[INTEREST mild]** F3 - 3 `int`s, 2 `float`s, 2 `CCPoint`s, plus a back-pointer `GameObject* m_gameObject`. Stored per-object in `GJGameState::m_gameObjectPhysics` map (which we don't snapshot - non-trivial container). Likely caches velocity/cached-physics for AdvancedFollow. Risk: sim crosses an AdvancedFollow trigger -> map gains an entry -> real player iterates new entry next frame. Confirm by inspecting `modifyGroupPhysics`/`modifyObjectPhysics` on GJBaseGameLayer L1955/L1964.

- `GhostTrailEffect.hpp` (93 lines) - **[INTEREST]** F1/F2 - PlayerObject visual trail. Fields: `m_snapshotInterval`, `m_fadeInterval`, `m_ghostScale`, `m_scaleTwice`, `m_playerScale`, `m_blendFunc`, `m_iconSprite`, `m_playerObject`, `m_objectLayer`, `m_opacity`, `m_color`, `m_position`. Methods: `runWithTarget(sprite, snapshotInterval, fadeInterval, duration, ghostScale, scaleTwice)` at L60, `stopTrail()` at L69, `trailSnapshot(dt)` at L78, `draw()` at L42. Trails snapshot on schedule; if sim PlayerObject has a trail, ghosts could spawn into the world. Verify our sim PlayerObject does NOT have ghost trails attached.

- `GravityEffectSprite.hpp` (61 lines) - **[INTEREST mild]** F1 - the visual particle sprite that plays on gravity flip. `playGravityEffect` in GJBaseGameLayer L425 spawns this. If we gate that virtual or pass `noEffects=true` to flipGravity, we suppress it.

- `GradientTriggerObject.hpp` (73 lines) - **[INTEREST mild]** F1/F2 - EffectGameObject subclass with blendingLayer, gradientID, channel IDs, vertexMode flag. Triggered via `controlGradientTrigger` (GJBaseGameLayer L1190) and `triggerGradientCommand` (L3413). Visual; skip unless spawning the gradient layer itself causes side effects.

- `GroupCommandObject2.hpp` (202 lines) - **[INTEREST mild]** F2 - the per-group enqueued command pattern. Holds command queue (move targets, easings, rotations) consumed by `processFollowActions` etc. State could mutate during sim if we hit a trigger that calls `claimMoveAction`/`controlDynamicCommand`. Skip detailed flagging - already gated by TrajEffectHook.

- `HardStreak.hpp` (149 lines) - **[INTEREST]** F1/F2 - the wave streak (`m_pointArray`, `m_currentPoint`, `m_waveSize`, `m_pulseSize`, `m_isSolid`, `m_isFlipped`, `m_drawStreak`). Methods `updateStroke(dt)` at L141, `stopStroke()` at L132, `scheduleAutoUpdate()` at L123. Wave-only artifact; if simmed wave player has its streak attached, ghost segments leak into world. Ensure sim PlayerObject's `m_regularTrail`/`m_waveTrail` (PlayerObject has these) are detached/disabled.

- `InheritanceNode.hpp` (48 lines) - **[INTEREST mild]** F2 - per-color-channel inheritance: `m_colorID`, `m_inheritanceNode`, `m_colorAction`. Holds a ColorAction*. If sim crosses a color trigger, inheritance node's `m_colorAction` could be replaced and the old one leaked. Minor - color is visual.

- `ItemTriggerGameObject.hpp` (97 lines) - **[INTEREST]** F4 - subclass of EffectGameObject for item-based triggers (counters, edits, compares, persistents). Activated via `activateItemEditTrigger` (L614), `activateItemCompareTrigger` (L605), `activatePersistentItemTrigger` (L632), `addPickupTrigger` (L740) on GJBaseGameLayer. Mutates `m_collectedItems` dictionary on the layer (L4235) and the per-itemID counters in the layer's count maps. Itemcount mutation during sim could cause real player to think it has picked up something. Confirm `pickupItem` (L2099) is gated.

- `KeyframeAnimTriggerObject.hpp` (68 lines) - **[INTEREST mild]** F2 - keyframe animation trigger; `playKeyframeAnimation` at L2198 of GJBaseGameLayer. Animation system is large but mostly visual.

- `KeyframeGameObject.hpp` (106 lines) - **[INTEREST mild]** F2 - keyframe data; `addKeyframe` (L497), `removeKeyframe` (L2684), `updateKeyframeOrder` (L3746). Mostly visual.

- `KeyframeObject.hpp` (47 lines) - **[INTEREST mild]** F2 - per-keyframe POD. Visual.

- `LabelGameObject.hpp` (216 lines) - **[INTEREST]** F4 - level-counter labels and time labels live on this. `m_collectedItems` and `updateCounters` (L3638), `addObjectCounter` (L731). Counter side-effect risk; minor since most counter ticks come from triggers which should be gated.

- `LevelTools.hpp` (276 lines) - **[INTEREST]** F4/F5 - static utility funcs. Worth a peek for any object-classification helpers (`getColor`, `levelDimensions`, etc.) that could feed our cull predicate. Most are level-loading helpers though.

- `LevelSettingsObject.hpp` (126 lines) - **[INTEREST]** F2/F3 - the LevelSettings POD passed to setupLevelStart. Fields include `m_startMode`, `m_startSpeed`, `m_startMini`, `m_startDual`, `m_mirrorMode`, `m_rotateGameplay`, `m_twoPlayerMode`, `m_platformerMode`, `m_isFlipped`, `m_reverseGameplay`, `m_disableStartPos`, `m_resetCamera`, `m_spawnGroup`, `m_allowMultiRotation`, `m_enablePlayerSqueeze`, `m_fixGravityBug`, `m_fixNegativeScale`, `m_fixRobotJump`, `m_dynamicLevelHeight`, `m_sortGroups`, `m_fixRadiusCollision`, `m_enable22Changes`, `m_allowStaticRotate`, `m_reverseSync`, `m_decreaseBoostSlide`. Read-only during play (set once at level load). Useful for sim setup to know what physics tweaks are active. NOT mutated during sim.

---

## Summary of findings in this range

1. **F2 camera leak - largest gap:** GJGameState has 29 unidentified `m_unkPoint*` fields (L92-120) inside the camera-state region; any could carry tween intermediates. Also, `m_tweenActions` map (L187) is intentionally NOT snapshotted - relies on TrajEffectHook to prevent any `tweenValue()` insertion during sim. Layer-side fields `m_staticCameraShake` (L4247), `m_skipCameraShake` (L4248), `m_resumeTimer` (L4263), and the camera offset fields (`m_cameraWidthOffset` L4232, `m_cameraHeightOffset` L4233, `m_cameraUnzoomedHeightOffset` L4243, `m_targetCameraHeightOffset` L4244, `m_calculateTargetHeightOffset` L4245) are also outside the snapshot. Cheap fix: add them to `LayerStateSnapshot`.

2. **F1 portal particles - cleanest hook is the engine's own `noEffects` parameter:** `flipGravity(...)`, `toggleDualMode(...)`, `toggleFlipped(...)`, `updateTimeMod(...)` all already accept a `noEffects` bool (L1343, L3287, L3296, L3926). If our sim path forces these to `true` via hooks, we get free particle suppression for portal-induced changes. Per-object `m_hasNoEffects` (L2450) and `m_hasNoParticles` (L2451) on GameObject are also already-existing flags; the engine respects them. Could temp-set them on simmed-during-flight objects.

3. **F1 particle pool flush:** `removeTemporaryParticles()` (L2729) and `m_temporaryParticles` (L4209) are the engine's own cleanup; we could call this at sim-end to evict leaked particles defensively.

4. **F3 frame-tick desync - `getModifiedDelta` (L1604) is the smoking gun:** if our sim feeds raw dt while real engine multiplies by m_timeWarp via this call, every dual-portal/timewarp sim diverges. Also `m_extraDelta` (L4250), `m_isBetweenSteps` (L4371), `m_clickBetweenSteps` (L4372), `m_clickOnSteps` (L4373) and `m_currentStep` (L4277) are layer-side timing scaffolding NOT in the snapshot.

5. **F4 orb false-hit - `m_activatedObjectIDs` (GJGameState L201) is almost certainly the dedupe map:** `map<pair<int,int>, int>`. If sim writes here while crossing an orb, real player thinks it's already activated. Add to snapshot. Also `canBeActivatedByPlayer` (L983) and `hasBeenActivatedByPlayer` (GameObject virtual L538) are the predicates - hook either to guarantee sim never `m_isActivated`-flips real-game objects.

6. **F5 spatial cull - the engine already does it for us:**
   - `m_calcNonEffectObjects` (L4317) is the per-frame "physics-active objects in player's current sections" precomputed list.
   - `m_solidCollisionObjects` (L4221) and `m_hazardCollisionObjects` (L4224) are pre-classified by collision type.
   - `m_leftSectionIndex/m_rightSectionIndex/m_bottomSectionIndex/m_topSectionIndex` (L4192-4195) tell us which sections the player is touching this frame.
   - `m_sectionXFactor/m_sectionYFactor` (L4327-4328) let us compute section indices from a world position.
   - `staticObjectsInRect(rect, enabledGroups)` (L3134) and `damagingObjectsInRect(rect, enabledGroups)` (L1298) are query helpers that already do rect-section filtering.
   - On `GameObject`: `m_isInvisible/m_isDisabled/m_isDisabled2/m_isDecoration/m_isDecoration2/m_isPassable/m_isNoTouch/m_isTrigger/m_isStartPos/m_isUIObject/m_isHide` (L2417-2544) are pure POD bools - filter without a vtable call.

7. **F3 button queue - `m_queuedButtons` (L4278) is the place to clone:** `vector<PlayerButtonCommand>`, with sibling vectors `m_queuedRecordedButtons` (L4279), `m_queuedReplayButtons` (L4282). Sim should clone all three into a local copy, run, then restore. `processQueuedButtons` (L2459) consumes them with `clearInputQueue` flag - if sim calls it with `clearInputQueue=true`, the real-game queue is empty next frame. Critical to verify what we pass.

8. **F6 staircase failures - look at `shouldUseSubstepForButton(dt)` (L3026) and `isBetweenSteps`/`clickBetweenSteps`/`clickOnSteps`:** the half-step substep machinery is what determines whether a buffered jump press lands on the same physics step or the next. If sim's substep scheduling differs from real engine (which uses these layer fields for state across frames), buffered presses can be eaten.

9. **`LayerStateSnapshot` proposed additions** (cheap, all POD, all in scope):
   - From `GJGameState`: `m_playerStreakBlend` (L159), `m_middleGroundOffsetY` (L127), `m_timeModRelated` (L199), `m_timeModRelated2` (L200), `m_totalTime` (L167), `m_levelTime` (L168), `m_commandIndex` (L170), `m_currentProgress` (L172), `m_pauseCounter` (L249), `m_pauseBufferTimer` (L250), `m_activatedObjectIDs` (L201, has trivial copy if size is small), and ideally a memcpy of the unkPoint1-29 block (L92-120) until each is identified.
   - From `GJBaseGameLayer` directly: `m_staticCameraShake`, `m_skipCameraShake`, `m_resumeTimer`, `m_cameraWidthOffset`, `m_cameraHeightOffset`, `m_cameraUnzoomedHeightOffset`, `m_targetCameraHeightOffset`, `m_calculateTargetHeightOffset`, `m_extraDelta`, `m_isBetweenSteps`, `m_clickBetweenSteps`, `m_clickOnSteps`, `m_tickIndex`, `m_clickIndex`, `m_currentStep`, `m_jumping`, `m_dualTouchTrigger`, `m_clicks`, `m_attempts` (decide if attempt-counting on death is wanted), `m_loadingProgress` (defensive), `m_freezeStartCamera`.

10. **No built-in sim mode found:** Searched for `isSimulation`, `noEffects` (only as param, not struct flag), `preview`. The closest-things-to-sim-mode in GJBaseGameLayer are the `noEffects` parameter on portal/mode-switch funcs and the per-`GameObject` `m_hasNoEffects/m_hasNoParticles` flags. We have to build our own gating; the engine doesn't have one to hijack.
