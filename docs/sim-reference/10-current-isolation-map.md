# Current isolation map — every hook in the mod

Reverse index of the mod's current isolation surface. Every hook in `src/Hooks.cpp` / `src/Orbs.cpp` / `src/Portals.cpp`, every snapshot field in `src/Trajectory.cpp`, every state-clear in `src/Trajectory.cpp`. Cross-referenced against the per-class audit docs so you can jump from "this hook exists" to "what does the engine actually do here."

Update this file whenever you add or remove a hook.

---

## Hook inventory by source file

### `src/Hooks.cpp`

| Hook class | Target class | Method | Line | Gating | Behavior | Audit ref |
|---|---|---|---|---|---|---|
| TrajPlayLayerHook | PlayLayer | setupHasCompleted | 32 | always | lifecycle: init sim, then defer levelReady until super done | [02](02-playlayer.md) |
| TrajPlayLayerHook | PlayLayer | resetLevel | 40 | always | lifecycle: notify sim | [02](02-playlayer.md) |
| TrajPlayLayerHook | PlayLayer | onQuit | 45 | always | lifecycle: cleanup before super | [02](02-playlayer.md) |
| TrajPlayLayerHook | PlayLayer | destroyPlayer | 50 | isSimPlayer + not anticheat | mark sim dead, skip super | [02](02-playlayer.md) |
| TrajPlayLayerHook | PlayLayer | playEndAnimationToPos | 55 | isSimulating | suppress | [02](02-playlayer.md) |
| TrajPlayLayerHook | PlayLayer | playPlatformerEndAnimationToPos | 60 | isSimulating | suppress | [02](02-playlayer.md) |
| TrajPlayLayerHook | PlayLayer | playGravityEffect | 69 | isSimulating | suppress | [02](02-playlayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | updateCamera | 81 | !isSimulating + this==registered pl | sim driver: calls simulate() | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | handleButton | 94 | button==1 | records real button (currently dead state) | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | destroyObject | 118 | isSimulating | mark sim-destroyed, skip super | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | collisionCheckObjects | 127 | isSimulating | spatial cull + type filter | [03](03-gjbasegamelayer.md), [01](01-playerobject.md) |
| TrajBaseLayerHook | GJBaseGameLayer | flipGravity | 193 | isSimulating + isSimPlayer | force noEffects=true for sim | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | canBeActivatedByPlayer | 203 | isSimulating | sim player: true; real-during-sim: false | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | toggleFlipped | 231 | isSimulating | force noEffects=true for sim | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | shakeCamera | 243 | isSimulating | suppress (CCAction would persist) | [03](03-gjbasegamelayer.md), [09](09-cocos2d-tweening.md) |
| TrajBaseLayerHook | GJBaseGameLayer | moveCameraToPos | 247 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | updateScreenRotation (float) | 251 | isSimulating | suppress; NOTE: PlayLayer's int overload is NOT hooked (C10) | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | playFlashEffect | 259 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | spawnParticle | 271 | isSimulating | return nullptr | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | spawnParticleTrigger (overload 1) | 277 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | spawnParticleTrigger (overload 2) | 281 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | lightningFlash (2-arg) | 286 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | lightningFlash (8-arg) | 290 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | playSpeedParticle | 301 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | animatePortalY | 322 | isSimulating | suppress; m_portalY value still snapshotted | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | cameraMoveX | 337 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | cameraMoveY | 341 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | updateCameraOffsetX | 345 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | updateCameraOffsetY | 351 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | updateStaticCameraPos | 357 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajBaseLayerHook | GJBaseGameLayer | updateStaticCameraPosToGroup | 365 | isSimulating | suppress | [03](03-gjbasegamelayer.md) |
| TrajPlayerObjectHook | PlayerObject | update | 384 | !isSimulating | record dt for sim's frame budget | [01](01-playerobject.md) |
| TrajPlayerObjectHook | PlayerObject | incrementJumps | 389 | isSimulating | suppress (persistent stat) | [01](01-playerobject.md) |
| TrajPlayerObjectHook | PlayerObject | playSpiderDashEffect | 394 | isSimulating | suppress | [01](01-playerobject.md) |
| TrajPlayerObjectHook | PlayerObject | playBumpEffect | 399 | isSimulating | suppress | [01](01-playerobject.md) |
| TrajPlayerObjectHook | PlayerObject | flashPlayer | 407 | isSimulating | suppress | [01](01-playerobject.md) |
| TrajPlayerObjectHook | PlayerObject | updateTimeMod | 421 | isSimulating + !isSimPlayer | skip super for real player during sim | [01](01-playerobject.md) |
| TrajLayerSpeedHook | GJBaseGameLayer | updateTimeMod (3-arg) | 444 | isSimulating | save real speeds → super → propagate new speed to sim → restore real | [03](03-gjbasegamelayer.md), [01](01-playerobject.md) |
| TrajEffectHook | EffectGameObject | triggerObject | 517 | isSimulating + !isSpeedMod | suppress; speed-mod path: save/restore activatedByPlayer flags + speeds, run super | [06](06-gameobject-base.md), [07](07-trigger-subclasses.md) |
| TrajEffectHook | EffectGameObject | triggerActivated | 533 | isSimulating + !isSpeedMod | suppress; speed-mod path: same wrapper as triggerObject | [06](06-gameobject-base.md) |
| TrajGameObjectHook | GameObject | playShineEffect | 550 | isSimulating | suppress | [06](06-gameobject-base.md) |
| TrajEnhancedHook | EnhancedGameObject | activatedByPlayer | 557 | isSimPlayer | spoof engine activation flags via markActivated, skip super (no visuals/audio) | [06](06-gameobject-base.md) |
| TrajEnhancedHook | EnhancedGameObject | hasBeenActivatedByPlayer | 582 | isSimPlayer | combine sim-local activation set with engine's real-player flags | [06](06-gameobject-base.md) |
| TrajHardStreakHook | HardStreak | addPoint | 597 | isSimulating | suppress (trail rendering) | n/a |

### `src/Orbs.cpp`

| Hook class | Target | Method | Line | Gating | Behavior | Audit ref |
|---|---|---|---|---|---|---|
| TrajOrbsLayerHook | GJBaseGameLayer | playerTouchedRing | 15 | isSimulating + !isSimPlayer | skip super for real player during sim; sim allowed through | [03](03-gjbasegamelayer.md) |
| TrajRingHook | RingObject | triggerActivated | 23 | isSimulating | suppress | [07](07-trigger-subclasses.md) |
| TrajRingHook | RingObject | powerOnObject | 28 | isSimulating | suppress | [06](06-gameobject-base.md) |
| TrajRingHook | RingObject | spawnCircle | 33 | isSimulating | suppress | [01](01-playerobject.md) |

### `src/Portals.cpp` (FROZEN)

| Hook class | Target | Method | Line | Gating | Behavior | Audit ref |
|---|---|---|---|---|---|---|
| TrajPortalsLayerHook | GJBaseGameLayer | toggleDualMode | 18 | isSimPlayer | skip super; m_isDualMode reverted by snapshot | [03](03-gjbasegamelayer.md) |
| TrajPortalsLayerHook | GJBaseGameLayer | checkCameraLimitAfterTeleport | 23 | isSimPlayer | skip super; camera state in snapshot | [03](03-gjbasegamelayer.md) |

### `src/BotHooks.cpp` (bot — adjacent, not sim isolation)

| Hook class | Target | Method | Line | Gating | Behavior |
|---|---|---|---|---|---|
| BotPlayLayerHook | PlayLayer | setupHasCompleted | 23 | always | bot lifecycle init |
| BotPlayLayerHook | PlayLayer | resetLevel | 32 | always | bot lifecycle reset |
| BotPlayLayerHook | PlayLayer | onQuit | 37 | always | bot lifecycle quit |
| BotBGLHook | GJBaseGameLayer | updateCamera | 54 | !isSimulating + this==registered pl | bot search/probe entrypoint |
| BotBGLHook | GJBaseGameLayer | handleButton | 78 | enabled + !injecting + !isSimulating + button==1 | suppress real keyboard during bot mode |
| BotPlayerObjectHook | PlayerObject | update | 105 | isP1/isP2 + live | inject bot's button pre-super, advance frame post-super |

---

## State copy from real to sim

### `copyAttributes(base)` (engine-supplied, opaque coverage)
**Where called:** `src/Trajectory.cpp:543` (`runBranch`) and `:633` (`runPlan::initSim`).
**Effect:** the engine copies a fairly large but UNVERIFIED set of PlayerObject fields. We don't know exactly which fields it carries. The explicit-copy block following it covers known gaps.

### Explicit per-field copies in `runBranch` (`src/Trajectory.cpp:545-586`)
Same list in `runPlan::initSim` (`:634-666`) — keep these two synchronized.

| Field | Audit ref |
|---|---|
| `m_isSliding` | [01](01-playerobject.md) |
| `m_maybeSlopeForce` | [01](01-playerobject.md) |
| `m_slopeAngle` | [01](01-playerobject.md) |
| `m_slopeSlidingMaybeRotated` | [01](01-playerobject.md) |
| `m_isOnIce` | [01](01-playerobject.md) |
| `m_maybeGoingCorrectSlopeDirection` | [01](01-playerobject.md) |
| `m_maybeUpsideDownSlope` | [01](01-playerobject.md) |
| `m_groundObjectMaterial` | [01](01-playerobject.md), [05](05-playercheckpoint.md) (NOT in checkpoint — keep here) |
| `m_stateOnGround` | [01](01-playerobject.md) |
| `m_lastGroundObject` | [01](01-playerobject.md) |
| `m_preLastGroundObject` | [01](01-playerobject.md) |
| `m_currentSlope2` | [01](01-playerobject.md) — note `m_currentSlope` itself is MISSING (see H2) |
| `m_collidedObject` | [01](01-playerobject.md) |
| `m_collidingWithLeft` | [01](01-playerobject.md) |
| `m_collidingWithRight` | [01](01-playerobject.md) |
| `m_jumpBuffered` | [01](01-playerobject.md) |
| `m_wasJumpBuffered` | [01](01-playerobject.md) |
| `m_stateJumpBuffered` | [01](01-playerobject.md) |
| `m_isOnGround2` | [01](01-playerobject.md) |
| `m_isOnGround3` | [01](01-playerobject.md) |
| `m_isOnGround4` | [01](01-playerobject.md) |
| `m_lastLandTime` | [01](01-playerobject.md) |
| `m_lastGroundedPos` | [01](01-playerobject.md) |
| `m_isOnSlope` | [01](01-playerobject.md) |
| `m_wasOnSlope` | [01](01-playerobject.md) |
| `m_slopeVelocity` | [01](01-playerobject.md) |
| `m_vehicleSize` | [01](01-playerobject.md), [05](05-playercheckpoint.md) (NOT in checkpoint — keep here) |
| Then: `m_gravityMod`, `m_isOnGround`, `setPosition(base->getPosition())` |

**Coverage analysis vs PlayerCheckpoint** ([05](05-playercheckpoint.md)): 28 of these 30 fields are also in PlayerCheckpoint and would be covered by `loadFromCheckpoint(cp)`. The 2 that are NOT: `m_vehicleSize` and `m_groundObjectMaterial` — these MUST stay even if we move to a checkpoint-based copy.

---

## State reset on sim per-run (`clearSimRingState`, `src/Trajectory.cpp:459-497`)

| Field | Reset value | Audit ref |
|---|---|---|
| `m_dashRing` | nullptr | [01](01-playerobject.md) |
| `m_isDashing` | false | [01](01-playerobject.md) |
| `m_dashX/Y/Angle/StartTime` | 0 | [01](01-playerobject.md) |
| `m_padRingRelated` | false | [01](01-playerobject.md) |
| `m_ringJumpRelated` | false | [01](01-playerobject.md) |
| `m_ringRelatedSet` | clear() | [01](01-playerobject.md) |
| `m_stateRingJump` | false | [01](01-playerobject.md), [05](05-playercheckpoint.md) (NOT in checkpoint) |
| `m_stateRingJump2` | false | [01](01-playerobject.md) |
| `m_touchedRing` | false | [01](01-playerobject.md) |
| `m_touchedCustomRing` | false | [01](01-playerobject.md) |
| `m_touchingRings` | own-array + removeAllObjects | [01](01-playerobject.md) — CRITICAL: must point at private array |
| `m_touchedRings` | clear() | [01](01-playerobject.md) |
| `m_jumpPadRelated` | clear() | [01](01-playerobject.md) |

## State reset on sim per-tick (`clearPerTickRingOverlap`, `src/Trajectory.cpp:499-512`)

| Field | Reset value | Justification |
|---|---|---|
| `m_touchedRing` | false | mirrors engine's per-tick clear |
| `m_touchedCustomRing` | false | mirrors engine's per-tick clear |
| `m_touchingRings` | removeAllObjects() | avoid per-tick orb-overlap accumulation |

---

## Layer-state snapshot (`LayerStateSnapshot`, `src/Trajectory.cpp:34-227`)

Captured at `runPlan`/`runBranch` start, restored at exit.

### Per-field capture (named)
| Field | Source | Audit ref |
|---|---|---|
| `m_cameraZoom` | GJGameState | [04](04-gjgamestate.md) (pre-POD, also via memcpy) |
| `m_targetCameraZoom` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraOffset` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraPosition` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraPosition2` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraAngle` | GJGameState | [04](04-gjgamestate.md) |
| `m_targetCameraAngle` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraEdgeValue0/1/2/3` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraShakeEnabled` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraShakeFactor` | GJGameState | [04](04-gjgamestate.md) |
| `m_cameraStepDiff` | GJGameState | [04](04-gjgamestate.md) |
| `m_isDualMode` | GJGameState | [04](04-gjgamestate.md) |
| `m_dualRelated` | GJGameState | [04](04-gjgamestate.md) |
| `m_levelFlipping` | GJGameState | [04](04-gjgamestate.md) |
| `m_gravityRelated` | GJGameState | [04](04-gjgamestate.md) |
| `m_portalY` | GJGameState | [04](04-gjgamestate.md) |
| `m_middleGroundOffsetY` | GJGameState | [04](04-gjgamestate.md) |
| `m_lastActivatedPortal1` | GJGameState | [04](04-gjgamestate.md) |
| `m_lastActivatedPortal2` | GJGameState | [04](04-gjgamestate.md) |
| `m_timeWarp` | GJGameState | [04](04-gjgamestate.md) |
| `m_queuedTimeWarp` | GJGameState | [04](04-gjgamestate.md) — name may be `m_unk18c` in `.bro`, see drift note |
| `m_timeWarpRelated` | GJGameState | [04](04-gjgamestate.md) |
| `m_currentChannel` | GJGameState | [04](04-gjgamestate.md) |
| `m_rotateChannel` | GJGameState | [04](04-gjgamestate.md) |
| `m_timeModRelated` | GJGameState | [04](04-gjgamestate.md) |
| `m_timeModRelated2` | GJGameState | [04](04-gjgamestate.md) |

### PlayLayer-direct fields captured
| Field | Source | Audit ref |
|---|---|---|
| `m_speedObjects` (full pointer-set snapshot/restore) | PlayLayer | [02](02-playlayer.md) |
| `m_groundLayer` position + stopAllActions | PlayLayer | [02](02-playlayer.md), [09](09-cocos2d-tweening.md) |
| `m_groundLayer2` position + stopAllActions | PlayLayer | [02](02-playlayer.md) |
| `m_middleground` position + stopAllActions | PlayLayer | [02](02-playlayer.md) |

### POD-prefix memcpy of GJGameState
Lines 162 (capture) and 225 (restore). Captures all POD bytes up to `offsetof(GJGameState, m_spawnChannelRelated0)` — every field BEFORE that boundary. See [04](04-gjgamestate.md) for the field-by-field breakdown of what this includes.

---

## Sim-local tracking sets

| Container | Type | Lifetime | Purpose | Audit ref |
|---|---|---|---|---|
| `m_activated` | `unordered_map<EnhancedGameObject*, OrbPreSimFlags>` | per-runPlan | sim-local "orb activated" + pre-sim flag snapshot for restore | [06](06-gameobject-base.md) |
| `m_simDestroyed` | `unordered_set<GameObject*>` | per-runPlan | objects sim destroyed; filtered at collision pass | [06](06-gameobject-base.md) |
| `m_simP1Dead`, `m_simP2Dead` | bool | per-runPlan | per-sim-player death flags |
| `m_simP1OwnRings`, `m_simP2OwnRings` | CCArray* (retained) | level lifetime | private `m_touchingRings` for sim, prevents sharing real's array | [01](01-playerobject.md) |

---

## Settings currently exposed (`mod.json`)

| Setting | Type | Default | Effect | Wired at |
|---|---|---|---|---|
| `show-trajectory` | bool | true | enable visualization (simulate()) | `src/main.cpp:25,39` |
| `trajectory-iterations` | int | 300 | iterations per branch | `src/main.cpp:26,42` |
| `pads` | bool | true | include pad collision in sim filter | `src/main.cpp:27,45` |
| `orbs` | bool | true | include orb collision in sim filter | `src/main.cpp:28,48` |
| `portals` | bool | true | include portal collision in sim filter | `src/main.cpp:29,51` |
| `bot-enabled` | bool | false | enable bot | `src/main.cpp:32,60` |
| `show-bot-trajectory` | bool | true | enable bot viz | `src/main.cpp:33,63` |
| `debug-visuals` | enum | off | candidate trace dots/lines | `src/main.cpp:34,66` |
| `bot-divergence-threshold` | double | 1.0 | divergence guard threshold | `src/main.cpp:35,69` |
| `bot-search-interval` | int | 1 | search every N visual frames | `src/main.cpp:36,72` |

**Not exposed (no current setting):**
- Coin filter (always on, hardcoded — see [11](11-leak-candidates.md) and `src/Hooks.cpp:176`)
- Spatial cull (always on, hardcoded constants — see `src/Hooks.cpp:142-186`)
- Per-trigger-family suppression (always master-suppress non-speed-mod)
- Audio suppression (not implemented — see C1 in [11](11-leak-candidates.md))
- Per-snapshot-family disable (debug toggles, none implemented)
- Per-clearSimRingState-field disable (debug toggles, none implemented)

---

## Coverage map: every state mutation surface

For each known state-mutating surface from the audits, where it's currently handled:

| Surface | Coverage mechanism | File:line | Audit |
|---|---|---|---|
| Sim's button input | Direct pushButton/releaseButton in runPlan loop | `src/Trajectory.cpp:739-750` | n/a |
| Real's button input during sim | Suppressed in BotBGLHook::handleButton | `src/BotHooks.cpp:78` | n/a |
| Mode portal traversal | Sim runs super (physics); m_isDualMode + portal Y snapshotted | `src/Hooks.cpp` (no toggle) + snapshot | [03](03-gjbasegamelayer.md) |
| Speed mod portal | save/restore real speeds + activatedByPlayer flags; propagate new speed to sim | `src/Hooks.cpp:444-477, 517-546` | [01](01-playerobject.md), [03](03-gjbasegamelayer.md) |
| Orb activation | spoof engine flags + sim-local tracking; clearActivated restores at runPlan end | `src/Trajectory.cpp:388-445` | [06](06-gameobject-base.md) |
| Breakable block break | sim-local m_simDestroyed set; filtered at collision pass | `src/Hooks.cpp:118,163` | [06](06-gameobject-base.md) |
| Coin collection | unconditionally filtered out of sim collision pass | `src/Hooks.cpp:176` | [02](02-playlayer.md) |
| Portal markers | snapshot/restore m_lastActivatedPortal1/2 | `src/Trajectory.cpp:139-140, 186-187` | [04](04-gjgamestate.md) |
| Camera state | snapshot/restore + source suppression of camera tweens | `src/Hooks.cpp:243-376` + snapshot | [09](09-cocos2d-tweening.md) |
| Particles | source-suppress on spawnParticle/spawnParticleTrigger/lightningFlash | `src/Hooks.cpp:271-296` | [09](09-cocos2d-tweening.md) |
| Ground bar animation | snapshot position + stopAllActions on restore | `src/Trajectory.cpp:153-158, 205-216` | [09](09-cocos2d-tweening.md) |
| Mode-portal Y tween | source-suppress animatePortalY + snapshot m_portalY value | `src/Hooks.cpp:322` + snapshot | [03](03-gjbasegamelayer.md) |
| Trigger fire (non-speed-mod) | source-suppress at EffectGameObject::triggerObject/triggerActivated | `src/Hooks.cpp:517-546` | [07](07-trigger-subclasses.md) |
| Visual effects per player | source-suppress on incrementJumps, playSpiderDashEffect, playBumpEffect, flashPlayer | `src/Hooks.cpp:389-411` | [01](01-playerobject.md) |
| Ring visuals | source-suppress on triggerActivated/powerOnObject/spawnCircle | `src/Orbs.cpp:23-37` | [07](07-trigger-subclasses.md) |
| Shine visual | source-suppress on GameObject::playShineEffect | `src/Hooks.cpp:550` | [06](06-gameobject-base.md) |
| HardStreak trail | source-suppress on addPoint | `src/Hooks.cpp:597` | n/a |
| Sim death | mark sim-dead flag, skip super for non-anticheat | `src/Hooks.cpp:50` | [02](02-playlayer.md) |
| Dual mode toggle | sim path: skip super; m_isDualMode snapshotted | `src/Portals.cpp:18` + snapshot | [03](03-gjbasegamelayer.md) |
| Camera-limit teleport | sim path: skip super; camera state snapshotted | `src/Portals.cpp:23` + snapshot | [03](03-gjbasegamelayer.md) |
| Time mod cache | snapshot/restore m_timeModRelated + m_timeModRelated2 | `src/Trajectory.cpp:160-161, 218-219` | [04](04-gjgamestate.md) |
| GJGameState POD prefix | memcpy snapshot/restore at boundary | `src/Trajectory.cpp:162, 225` | [04](04-gjgamestate.md) |

---

## Gaps from the audits — methods/fields NOT in this coverage map

See [11-leak-candidates.md](11-leak-candidates.md) for the ranked list. Quick recap:

- **Audio** — entire surface (143 entry points)
- **`m_effectManager`** — trigger state container on GJBaseGameLayer
- **`m_collisionLog{Top,Bottom,Left,Right}`** — shared CCDictionary on PlayerObject
- **6 high-impact GJGameState post-POD scalars** — `m_totalTime`, `m_levelTime`, `m_commandIndex`, etc.
- **`m_currentSlope`** — not in explicit copy
- **`m_lastJumpTime`/`m_lastFlipTime`/`m_lastSpiderFlipTime`** — robot bug suspects
- **Checkpoint surface** — practice-mode leak
- **End/complete paths** — `levelComplete`, `showEndLayer`
- **`updateColor`** virtual — bypasses trigger gate
- **`toggleGroupTriggered`/`spawnGroup`/`spawnObject`** virtuals — defense-in-depth
- **`PlayLayer::updateScreenRotation(int)`** overload — second overload missed
- **CCAction loops** — `runRotateAction`, `runBallRotation*`, `runNormalRotation`
- **Persistent stat** — `commitJumps`
