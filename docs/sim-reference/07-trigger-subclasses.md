# Trigger subclasses & specialized GameObject subclasses

*Geometry Dash 2.2 has roughly 30 `EffectGameObject` subclasses plus a handful of specialized `GameObject` / `EnhancedGameObject` subclasses. The bot's current strategy gates `EffectGameObject::triggerObject` super on `m_speedModType != 0`, which means every non-speed trigger family is suppressed wholesale during sim. That's safe but coarse: many families either have purely cosmetic effects (shaders, SFX, labels) that are fine to leave suppressed, while others mutate effect-manager state that the sim might benefit from (count/item triggers feed back into pickup-counter checks), and a small handful actually drive player physics (force blocks add velocity; rotate-gameplay changes gravity direction; player-control disables jump/move). Per-family control means: keep the speed-mod save-restore pattern, suppress all the cosmetic families exactly as today, and add `run-for-sim-only` for the small handful of triggers that affect sim physics — possibly with snapshots of any persistent state they mutate. Every per-class entry below lists the parent, the overrides, the unique fields, the firing behavior, the sim-impact reasoning, and the recommended treatment plus discriminator.*

## Quick-lookup classification table

| Class | Inherits from | What it does (one line) | Affects sim physics? | Recommended treatment | Discriminator (how to detect at runtime) |
| --- | --- | --- | --- | --- | --- |
| AdvancedFollowTriggerObject | EffectGameObject | Configures advanced-follow group motion (steering, breaking) | No (group motion not player) | suppress | no `triggerObject` override → super walks; use `dynamic_cast` |
| AdvancedFollowEditObject | AdvancedFollowTriggerObject | Edits live advanced-follow command (modX/modY/redirect) | No | suppress | `dynamic_cast<AdvancedFollowEditObject*>` |
| ArtTriggerGameObject | EffectGameObject | Switches background/foreground/middleground art set | No | suppress | `dynamic_cast<ArtTriggerGameObject*>` |
| AudioLineGuideGameObject | EffectGameObject | Editor BPM-guideline rendering helper | No | n/a (editor only) | always suppressed; no runtime trigger |
| CameraTriggerGameObject | EffectGameObject | Switches camera mode (free/static/follow), edge lock | No (camera doesn't feed physics) | suppress | `dynamic_cast<CameraTriggerGameObject*>` |
| ChanceTriggerGameObject | EffectGameObject | Holds chance pool, no direct trigger (base for RandTrigger/Sequence) | No | suppress (no triggerObject override) | `dynamic_cast<ChanceTriggerGameObject*>` |
| CheckpointGameObject | EffectGameObject | Auto-checkpoint marker (platformer) | No (sim is not allowed to set checkpoints) | suppress | `dynamic_cast<CheckpointGameObject*>` |
| CountTriggerGameObject | EffectGameObject | Pickup counter, triggers when count reached | Indirect (gates other triggers in real, not sim) | suppress | `dynamic_cast<CountTriggerGameObject*>` |
| EndTriggerGameObject | EffectGameObject | Triggers level end (win screen) | NO — never let sim fire this | suppress (already covered, but be explicit) | `dynamic_cast<EndTriggerGameObject*>` |
| EnhancedTriggerObject | EffectGameObject | Holds min/max X/Y group IDs for area triggers (config-only) | No (no triggerObject override) | suppress | base for area-triggers; not directly fired |
| EnterEffectObject | EffectGameObject | Sets up area enter/exit animation effect | No (visual area animation) | suppress | `m_enterType != 0` / `dynamic_cast` |
| EventLinkTrigger | EffectGameObject | Fires events on a global event bus | Indirect (event side-effects in real) | suppress | `dynamic_cast<EventLinkTrigger*>` |
| ForceBlockGameObject | EffectGameObject | Applies force to player on touch (physics) | YES — adds velocity to player | run-for-sim-only (need force applied for accurate sim) | `dynamic_cast<ForceBlockGameObject*>`; not actually triggered, hits via collision |
| GameOptionsTrigger | EffectGameObject | Toggles streak/dual-gravity/ground-hide/etc. globally | No directly, but disabling P1 controls could affect sim | suppress | `dynamic_cast<GameOptionsTrigger*>` |
| GradientTriggerObject | EffectGameObject | Background gradient rendering | No | suppress | `dynamic_cast<GradientTriggerObject*>` |
| ItemTriggerGameObject | EffectGameObject | Computes item math (item1 op item2 → target), persistent option | Indirect — feeds count/conditional triggers | suppress (sim shouldn't write items) | `dynamic_cast<ItemTriggerGameObject*>` |
| KeyframeAnimTriggerObject | EffectGameObject | Drives keyframe animations on a group | No | suppress | `dynamic_cast<KeyframeAnimTriggerObject*>` |
| KeyframeGameObject | EffectGameObject | Keyframe waypoint (shape data, not a trigger) | No | suppress | `dynamic_cast<KeyframeGameObject*>` |
| LabelGameObject | EffectGameObject | Renders text/timer/counter label | No | suppress | `dynamic_cast<LabelGameObject*>` |
| ObjectControlGameObject | EffectGameObject | Controls collision/static/dynamic objects | Indirect (changes which objects collide) | suppress | `dynamic_cast<ObjectControlGameObject*>` |
| PlayerControlGameObject | EffectGameObject | Disables player jump/move/rotation/slide | YES if sim hits one during predict | suppress (already suppressed via PlayerObject hooks in sim; firing in real would lock real player) | `dynamic_cast<PlayerControlGameObject*>` |
| RandTriggerGameObject | ChanceTriggerGameObject | Picks random group ID from chance pool, fires once | No directly | suppress (sim shouldn't pick) | `dynamic_cast<RandTriggerGameObject*>` |
| RingObject | EffectGameObject | Orb base class (jump rings) | YES — physics already handled by orb logic | n/a — orb collisions go through PlayerObject path, NOT triggerObject | `dynamic_cast<RingObject*>` |
| RotateGameplayGameObject | EffectGameObject | Rotates gameplay direction / changes gravity | YES — flips gameplay rotation/velocity | run-for-sim-only with snapshot rollback (mutates GJGameState rotation) | `dynamic_cast<RotateGameplayGameObject*>` |
| SequenceTriggerGameObject | ChanceTriggerGameObject | Sequential group activation (1, 2, 3, …) | No | suppress (sim shouldn't advance sequence) | `dynamic_cast<SequenceTriggerGameObject*>` |
| SFXTriggerGameObject | EffectGameObject | Plays sound effect | No (audio only) | suppress | `dynamic_cast<SFXTriggerGameObject*>` |
| ShaderGameObject | EffectGameObject | Shader effects (chromatic/bulge/grayscale/etc.) | No | suppress | `dynamic_cast<ShaderGameObject*>` |
| SongTriggerGameObject | SFXTriggerGameObject | Triggers song / song channel | No | suppress | `dynamic_cast<SongTriggerGameObject*>` |
| SpawnParticleGameObject | EffectGameObject | Spawns a particle burst at target | No (visual) | suppress | `dynamic_cast<SpawnParticleGameObject*>` |
| SpawnTriggerGameObject | EffectGameObject | Spawns a target group with delay, remaps chance keys | No directly; the spawn-cascade IS what causes count/item triggers to fire later, but during sim we deliberately don't want side-effects | suppress | `dynamic_cast<SpawnTriggerGameObject*>` |
| StartPosObject | EffectGameObject | Marks a level start position | No (never fires during play) | n/a | `dynamic_cast<StartPosObject*>` |
| TimerTriggerGameObject | EffectGameObject | Counts down/up, fires another trigger at target | No directly; runs on effect manager | suppress | `dynamic_cast<TimerTriggerGameObject*>` |
| TransformTriggerGameObject | EffectGameObject | Transforms group (move/scale/divide) | No (visual transform of non-player groups) | suppress | `dynamic_cast<TransformTriggerGameObject*>` |
| TriggerControlGameObject | EffectGameObject | Stop/enable/disable other triggers | No directly | suppress | `dynamic_cast<TriggerControlGameObject*>` |
| UISettingsGameObject | EffectGameObject | Edits UI overlay reference X/Y | No (HUD only) | suppress | `dynamic_cast<UISettingsGameObject*>` |
| AnimatedGameObject | EnhancedGameObject (non-trigger) | Wraps an animated sprite tied to an object ID | No (visual animation) | n/a (no triggerObject path) | not under EffectGameObject hook |
| ParticleGameObject | EnhancedGameObject (non-trigger) | Renders a particle system | No | n/a | not under EffectGameObject hook |
| SpecialAnimGameObject | EnhancedGameObject (non-trigger) | Robot/monster pickup with extra sprites | No | n/a | not under EffectGameObject hook |
| EndPortalObject | GameObject | Goal portal (end of level) | YES — touches end the level | special: sim must avoid colliding with it; not via TrajEffectHook | not under EffectGameObject hook; touch-handled by PlayLayer |
| SmartGameObject | GameObject | Adjacency-aware decorative tile | No | n/a | not under EffectGameObject hook |
| TextGameObject | GameObject | Renders editor text-block sprite | No | n/a | not under EffectGameObject hook |
| PlayerObject | GameObject (already audited separately) | The player! | YES (it IS the physics) | n/a — already gated by TrajPlayerObjectHook | not via TrajEffectHook |

---

## Per-class detail

### AdvancedFollowTriggerObject (line 329)
- **Inherits**: `EffectGameObject`
- **Purpose**: Configures advanced follow-behavior (steering forces, break angles, speed ranges, target direction) for groups via the advanced-follow system.
- **Unique virtual overrides**: `customObjectSetup`, `getSaveString` only. Notably **no `triggerObject` override** — when the trigger fires, the base `EffectGameObject::triggerObject` does the dispatch.
- **Unique fields**: `m_delay, m_delayVariance, m_startSpeed, m_startSpeedVariance, m_startSpeedReference, m_startDirection, m_startDirectionVariance, m_startDirectionReference, m_maxSpeed, m_maxSpeedVariance, m_xOnly, m_yOnly, m_maxRange, m_maxRangeVariance, m_property310, m_property311, m_acceleration, m_accelerationVariance, m_property312-315, m_steerForce, m_steerForceVariance, m_steerForceLowEnabled, m_steerForceLow, m_steerForceLowVariance, m_steerForceHighEnabled, m_steerForceHigh, m_steerFroceHighVariance, m_speedRangeLow, m_speedRangeLowVariance, m_speedRangeHigh, m_speedRangeHighVariance, m_breakForce, m_breakForceVariance, m_breakAngle, m_breakAngleVariance, m_breakSteerForce, m_breakSteerForceVariance, m_breakSteerSpeedLimit, m_breakSteerSpeedLimitVariance, m_targetDirection, m_ignoreDisabled`. Also a unique `getAdvancedFollowID()` accessor.
- **What firing does in real**: Registers an advanced follow command in `EffectManagerState`; the engine then steers a group's motion using the configured parameters.
- **Sim impact analysis**: Drives non-player group motion. Group motion does NOT feed back into player physics directly (unless a `ForceBlockGameObject` is in the moving group, but those are handled by their own collision path). Suppressing during sim leaves real game state untouched and sim physics correct.
- **Recommended treatment**: `suppress`.
- **Discriminator**: no cheap field check unique to it; use `dynamic_cast<AdvancedFollowTriggerObject*>`.
- **Code sketch**:
  ```cpp
  if (dynamic_cast<AdvancedFollowTriggerObject*>(this)) return; // suppress
  ```

### AdvancedFollowEditObject (line 305)
- **Inherits**: `AdvancedFollowTriggerObject`
- **Purpose**: Live-edits an advanced-follow command (mod X/Y, redirect direction) without restarting it.
- **Unique virtual overrides**: `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_modX, m_modXVariance, m_modY, m_modYVariance, m_redirectDirection`.
- **What firing does in real**: Modifies parameters of an already-running advanced follow command.
- **Sim impact analysis**: Same as parent — affects group motion only.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<AdvancedFollowEditObject*>`.

### ArtTriggerGameObject (line 598)
- **Inherits**: `EffectGameObject`
- **Purpose**: Switches the active art set (background / middleground / foreground reference).
- **Unique virtual overrides**: `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_artIndex`.
- **What firing does in real**: Calls a setter on GJBaseGameLayer / LevelSettings to swap art assets.
- **Sim impact analysis**: Cosmetic; no physics path. Suppressing is harmless.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<ArtTriggerGameObject*>`.

### AudioLineGuideGameObject (line 673)
- **Inherits**: `EffectGameObject`
- **Purpose**: BPM-aligned vertical guideline rendering, editor visualization only.
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_beatsPerMinute, m_beatsPerBar, m_speed (Speed), m_disabled`.
- **What firing does in real**: Should never fire during play — it's an editor visualization aid. The base `EffectGameObject::triggerObject` would no-op.
- **Sim impact analysis**: None.
- **Recommended treatment**: `suppress` (defensive; effectively never reached).
- **Discriminator**: `dynamic_cast<AudioLineGuideGameObject*>`.

### CameraTriggerGameObject (line 975)
- **Inherits**: `EffectGameObject`
- **Purpose**: Switches camera mode (static/free/follow), camera easing & smoothing.
- **Unique virtual overrides**: `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_exitStatic, m_followObject, m_followEasing, m_edgeDirection, m_smoothVelocity, m_velocityModifier, m_exitInstant, m_previewOpacity`.
- **What firing does in real**: Updates camera state; PlayLayer/GJBaseGameLayer camera transforms change. Does not affect player physics — the camera doesn't read back into m_player1/m_player2.
- **Sim impact analysis**: Pure render-frame transform. Suppressing leaves both sim and real correct.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<CameraTriggerGameObject*>`.

### ChanceTriggerGameObject (line 1807)
- **Inherits**: `EffectGameObject`
- **Purpose**: Base class that holds a `gd::vector<ChanceObject>` chance pool. Used by `RandTriggerGameObject` and `SequenceTriggerGameObject`.
- **Unique virtual overrides**: None — only utility helpers like `editChanceObject`, `remapChanceObjects`, `revertChanceRemap`.
- **Unique fields**: `m_chanceObjects` (vector of ChanceObject).
- **What firing does in real**: This base class itself doesn't define a `triggerObject` override; if you cast to it you actually have either a `RandTrigger` or a `SequenceTrigger`.
- **Sim impact analysis**: Side-effect free at the level of the base class.
- **Recommended treatment**: `suppress` (route through subclass handling).
- **Discriminator**: `dynamic_cast<ChanceTriggerGameObject*>` — useful as a catch-all for the two chance-based subclasses.

### CheckpointGameObject (line 1866)
- **Inherits**: `EffectGameObject`
- **Purpose**: Platformer auto-checkpoint marker.
- **Unique virtual overrides**: `init`, `setupCustomSprites`, `resetObject`, `triggerObject`, `customObjectSetup`, `getSaveString`, `triggerActivated`, `restoreObject`, `updateSyncedAnimation`.
- **Unique fields**: `m_checkpointActivated, m_respawnID`.
- **What firing does in real**: Captures full `CheckpointObject` state — copies all of `m_gameState`, `m_shaderState`, `m_audioState`, `m_effectManagerState`, `m_vectorSavedObjectStateRef`, etc. Pushes onto `PlayLayer`'s checkpoint vector. This is HEAVY state capture.
- **Sim impact analysis**: If sim fires this, the engine snapshots the entire game state and pushes onto the global checkpoint stack — this would persist after the sim ends, corrupting real-game checkpoint history. Must suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<CheckpointGameObject*>`. Field check fallback: this class has a unique `m_checkpointActivated` field but it's hard to read positionally from base pointer.

### CountTriggerGameObject (line 2410)
- **Inherits**: `EffectGameObject`
- **Purpose**: Pickup counter — increments when item count is reached, can act as a multi-fire condition.
- **Unique virtual overrides**: `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_pickupCount, m_pickupTriggerMode, m_multiActivate, m_isOverride, m_pickupTriggerMultiplier`.
- **What firing does in real**: Writes to `EffectManagerState::m_itemCountMap`. Subsequent triggers may gate on the new value.
- **Sim impact analysis**: Mutates EffectManager item-counter state. Without a snapshot at sim boundary, the count would persist after sim and double-fire when real player passes the same trigger. Snapshot/restore would be possible but adds complexity for negligible sim-physics benefit (player physics doesn't read item count directly — pickups feed orbs/jumps via collision paths).
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<CountTriggerGameObject*>`.

### EndTriggerGameObject (line 4367)
- **Inherits**: `EffectGameObject`
- **Purpose**: Triggers the end-of-level sequence (win screen, score popup).
- **Unique virtual overrides**: `init`, `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_noEffects, m_noSFX, m_instant`.
- **What firing does in real**: Calls `PlayLayer::playEndAnimation` / `levelComplete`. Permanent and impossible to revert mid-level.
- **Sim impact analysis**: If sim fired this it would end the real level on sim crossing. Catastrophic. Already suppressed by the base hook (no speed mod) but worth being explicit about.
- **Recommended treatment**: `suppress` (CRITICAL — even if user later adds a "run all triggers" toggle, this one must remain hardcoded suppressed during sim).
- **Discriminator**: `dynamic_cast<EndTriggerGameObject*>`.

### EnhancedTriggerObject (line 4482)
- **Inherits**: `EffectGameObject`
- **Purpose**: Stores min/max X/Y group IDs for area-triggers; **abstract base** with no `triggerObject` override.
- **Unique virtual overrides**: `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_minXID, m_minYID, m_maxXID, m_maxYID`.
- **What firing does in real**: Base class doesn't define triggering behavior; subclasses (none directly listed) handle it.
- **Sim impact analysis**: None for the base.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<EnhancedTriggerObject*>`.

### EnterEffectObject (line 4567)
- **Inherits**: `EffectGameObject`
- **Purpose**: Defines a screen-edge area-enter/exit animation effect (length, offset, move/rotate/scale/tint per direction, easing).
- **Unique virtual overrides**: `customSetup`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: Huge set — `m_enterType, m_length, m_lengthVariance, m_offset, m_offsetVariance, m_offsetY, m_offsetYVariance, m_moveDistance, m_moveDistanceVariance, m_areaScaleX/XVariance, m_areaScaleY/YVariance, m_moveAngle, m_moveAngleVariance, m_startAngle, m_anglePosition, m_relative, m_relativeFade, m_easingInType, m_easingInRate, m_easingInBuffer, m_easingOutType, m_easingOutRate, m_easingOutBuffer, m_moveX/XVariance, m_moveY/YVariance, m_tintChannelID, m_property224, m_directionType, m_xyMode, m_easeOutEnabled, m_modFront, m_modBack, m_areaTint, m_property285, m_effectID, m_areaRotation, m_areaRotationVariance, m_toOpacity, m_fromOpacity, m_inbound, m_hsvEnabled, m_deadzone, m_twoDirections, m_dontEditAreaParent, m_priority, m_unk7d8, m_enterChannel, m_useEffectID, m_unk7e4, m_unk7ec, m_negativeTargetX, m_areaRange, m_unk7fc`.
- **What firing does in real**: Creates an `EnterEffectInstance` (separate class at line 4503) and animates target groups as they enter/exit configured screen areas.
- **Sim impact analysis**: Animates visual groups; no player physics impact.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<EnterEffectObject*>`. Field fallback: `m_enterType` is the unique distinguishing property if the layout offset is known.

### EventLinkTrigger (line 4688)
- **Inherits**: `EffectGameObject`
- **Purpose**: Emits event IDs on a global event bus, can include up to two extra IDs and a remap reset.
- **Unique virtual overrides**: `init`, `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_eventIDs (gd::set<int>), m_resetRemap, m_extraID, m_extraID2`.
- **What firing does in real**: Dispatches into the event manager; other triggers can listen.
- **Sim impact analysis**: Indirect ripples through listener triggers — most listeners are themselves cosmetic. Cleanest to suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<EventLinkTrigger*>`.

### ForceBlockGameObject (line 5140)
- **Inherits**: `EffectGameObject`
- **Purpose**: Applies a physical force vector to a target object on contact (force pad / push field).
- **Unique virtual overrides**: `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_force, m_minForce, m_maxForce, m_relativeForce, m_forceRange, m_forceID`. Plus `calculateForceToTarget(GameObject*)` helper.
- **What firing does in real**: NOT actually invoked through `triggerObject` — the force-apply happens in `PlayerObject::collidedWithObject` collision path when the player overlaps the force block. The "force" is added to the player's velocity directly.
- **Sim impact analysis**: This is one of the rare physics-affecting "triggers." For sim accuracy the force must be applied to the sim's PlayerObject when the sim crosses it. Suppressing the EffectGameObject hook does NOT touch this — the collision path is separate. CONCLUSION: TrajEffectHook is irrelevant to ForceBlockGameObject. Force application is handled by collision logic, which the sim already runs.
- **Recommended treatment**: `run-for-sim-only` semantically — but the actual concern is the collision path, not the trigger path. No TrajEffectHook change needed.
- **Discriminator**: `dynamic_cast<ForceBlockGameObject*>`. Field: `m_force != 0.f` is a heuristic but unreliable (could be zero-valued).

### GameOptionsTrigger (line 6661)
- **Inherits**: `EffectGameObject`
- **Purpose**: Edits `GameOptionsSetting` toggles globally (streak additive, dual gravity unlink, ground hide, P1/P2 hide, P1/P2 controls disable, MG hide, respawn time, death audio, boost slide).
- **Unique virtual overrides**: `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_streakAdditive, m_unlinkDualGravity, m_hideGround, m_hideP1, m_hideP2, m_disableP1Controls, m_disableP2Controls, m_hideMG, m_hideAttempts, m_editRespawnTime, m_respawnTime, m_audioOnDeath, m_noDeathSFX, m_boostSlide`.
- **What firing does in real**: Writes to `GJGameState` / `LevelSettingsObject` flags. Many are cosmetic (hide options), but `m_disableP1Controls` would prevent the sim player from jumping if active.
- **Sim impact analysis**: If sim crossed a "disable P1 controls" GameOptionsTrigger and it fired, the real player wouldn't have controls disabled (state is per-CheckpointObject snapshot). But also the sim shouldn't honor it because the bot needs to predict assuming controls are active in the next attempt. Suppressing is safe.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<GameOptionsTrigger*>`.

### GradientTriggerObject (line 10836)
- **Inherits**: `EffectGameObject`
- **Purpose**: Renders a gradient overlay using 4 anchor color IDs and blending modes.
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_blendingLayer, m_blendingMode, m_gradientID, m_upBottomLeftID, m_downBottomRightID, m_leftTopLeftID, m_rightTopRightID, m_vertexMode, m_disable, m_disableAll, m_previewOpacity`.
- **What firing does in real**: Registers a gradient with `PlayLayer::m_gradientTriggerObjectArray`. Rendering only.
- **Sim impact analysis**: Cosmetic.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<GradientTriggerObject*>`.

### ItemTriggerGameObject (line 11201)
- **Inherits**: `EffectGameObject`
- **Purpose**: Computes math on items: `item1 (op) item2 → target item`, with mod/round/sign/tolerance/timer options.
- **Unique virtual overrides**: `customSetup`, `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_item1Mode, m_item2Mode, m_targetItemMode, m_mod1, m_mod2, m_resultType1, m_resultType2, m_resultType3, m_tolerance, m_roundType1, m_roundType2, m_signType1, m_signType2, m_persistent, m_targetAll, m_reset, m_timer`.
- **What firing does in real**: Reads/writes `EffectManagerState::m_itemCountMap` and `m_timerItemMap`. Feeds into count-trigger gates and label displays.
- **Sim impact analysis**: Indirect — could change conditional trigger gating. Sim's PlayerObject physics never reads items directly. Safer to suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<ItemTriggerGameObject*>`.

### KeyframeAnimTriggerObject (line 11309)
- **Inherits**: `EffectGameObject`
- **Purpose**: Drives keyframe animations (time/position/rotation/scale modulation factors) for a group.
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_timeMod, m_positionXMod, m_positionYMod, m_rotationMod, m_scaleXMod, m_scaleYMod`.
- **What firing does in real**: Modulates an active keyframe animation.
- **Sim impact analysis**: Animation of decorative groups only.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<KeyframeAnimTriggerObject*>`.

### KeyframeGameObject (line 11334)
- **Inherits**: `EffectGameObject`
- **Purpose**: Waypoint object marking a position in a keyframe path (with curve/loop/proximity flags and a `m_spawnDelay`).
- **Unique virtual overrides**: `init`, `setOpacity`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_shadowObjects, m_previewSprite, m_keyframeGroup, m_keyframeIndex, m_referenceOnly, m_proximity, m_curve, m_closeLoop, m_timeMode, m_unk760, m_spawnDelay, m_previewArt, m_keyframeActive, m_autoLayer, m_direction, m_revolutions, m_lineOpacity`.
- **What firing does in real**: Editor-side placeholder; doesn't fire at runtime in the trigger sense.
- **Sim impact analysis**: None.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<KeyframeGameObject*>`.

### LabelGameObject (line 11411)
- **Inherits**: `EffectGameObject`
- **Purpose**: Renders a text label (configurable alignment, kerning, time-counter / item-counter display modes).
- **Unique virtual overrides**: `init`, `setOpacity`, `setupCustomSprites`, `addMainSpriteToParent`, `resetObject`, `customObjectSetup`, `getSaveString`, `setObjectColor`, `updateTextKerning`, `getTextKerning`. **No `triggerObject` override.**
- **Unique fields**: `m_label, m_labelDirty, m_labelString, m_labelColorLocked, m_alignment, m_showSecondsOnly, m_shownSpecial, m_isTimeCounter, m_kerning, m_updateLabel`.
- **What firing does in real**: Renders text overlay (item/timer display).
- **Sim impact analysis**: HUD only.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<LabelGameObject*>`.

### ObjectControlGameObject (line 13621)
- **Inherits**: `EffectGameObject`
- **Purpose**: Toggles object-level controls (collision, static, dynamic).
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: none documented in this class itself beyond inheritance.
- **What firing does in real**: Modifies properties of target objects (which collide, which are dynamic).
- **Sim impact analysis**: Indirect physics effect — disabling collision on a block during sim would alter sim collision results. Currently suppressed; if this becomes important the sim would need snapshot/restore.
- **Recommended treatment**: `suppress` (with note: if levels using this start failing search, revisit).
- **Discriminator**: `dynamic_cast<ObjectControlGameObject*>`.

### PlayerControlGameObject (line 14203)
- **Inherits**: `EffectGameObject`
- **Purpose**: Disables specific player actions: jump, move, rotation, slide.
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_stopJump, m_stopMove, m_stopRotation, m_stopSlide`.
- **What firing does in real**: Writes to PlayerObject control flags that affect input handling.
- **Sim impact analysis**: If sim crossed this and it fired, the SIM player would be control-disabled (good for sim accuracy) but so would the REAL player after sim ends (bad — would persist). Without snapshot/restore of control flags, suppressing is the safer default.
- **Recommended treatment**: `suppress`. If sim accuracy in levels using this becomes critical, save-restore the player control fields around the super call.
- **Discriminator**: `dynamic_cast<PlayerControlGameObject*>`.

### RandTriggerGameObject (line 15182)
- **Inherits**: `ChanceTriggerGameObject`
- **Purpose**: Picks a random group ID from its chance pool and fires.
- **Unique virtual overrides**: `init`, `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: none unique beyond accessors `getRandomGroupID()` and `getTotalChance()`.
- **What firing does in real**: Calls into spawn dispatch on a randomly chosen group ID.
- **Sim impact analysis**: Non-deterministic — sim firing this would burn through the level's RNG and bias real-game behavior. Strict suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<RandTriggerGameObject*>`.

### RingObject (line 15386)
- **Inherits**: `EffectGameObject`
- **Purpose**: Base for all orbs (yellow/blue/pink/green/black/dash/teleport/spider rings). The "trigger" terminology is misleading — orbs are activated by the player TOUCHING them, with the actual behavior dispatched via `PlayerObject` collision / orb-claim path, not `triggerObject`.
- **Unique virtual overrides**: `setScale, setRotation, resetObject, customObjectSetup, getSaveString, setRScale, triggerActivated, shouldDrawEditorHitbox, powerOnObject`. **No `triggerObject` override** — orbs go through `triggerActivated(xPosition)` after a successful player touch.
- **Unique fields**: `m_claimTouch, m_isSpawnOnly`.
- **What firing does in real**: `triggerActivated` spawns the "activated" circle, awards the orb effect (jump for yellow, dash for green, etc.) to the player.
- **Sim impact analysis**: The orb's behavior on the player IS the physics — sim NEEDS the player to consume orbs to predict accurately. The sim handles this through `m_touchingRings`/orb-claim state already (see `Orbs.cpp`). The `TrajEffectHook::triggerActivated` gate currently suppresses non-speed-mod, which means orb `triggerActivated` calls during sim are blocked. The sim instead relies on its own touched-ring handling.
- **SURPRISE**: RingObject lives under EffectGameObject inheritance even though it's the orb base. Its `triggerActivated` is the canonical orb-activation entry point.
- **Recommended treatment**: `suppress` is the current behavior and is correct, because sim orb consumption is handled via `m_touchingRings` + `PlayerObject` orb-claim path independently. Per-family branching should explicitly note that RingObject is "handled elsewhere" rather than just suppressed.
- **Discriminator**: `dynamic_cast<RingObject*>`. The `m_claimTouch` field is unique but only present after a touch.

### RotateGameplayGameObject (line 15411)
- **Inherits**: `EffectGameObject`
- **Purpose**: Rotates gameplay direction (changes the gravity axis / movement direction by 90°/180° etc.), can override velocity, can lock the change to a single channel.
- **Unique virtual overrides**: `init`, `updateStartValues`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_moveDirection, m_groundDirection, m_editVelocity, m_overrideVelocity, m_velocityModX, m_velocityModY, m_changeChannel, m_channelOnly, m_targetChannelID, m_instantOffset, m_dontSlide`. Also helper `updateGameplayRotation()`.
- **What firing does in real**: Writes to `GJGameState` rotation/direction fields and (if `m_editVelocity`/`m_overrideVelocity`) modifies player velocity components. THIS IS A PHYSICS-AFFECTING TRIGGER.
- **Sim impact analysis**: If sim crosses one and it fires, the GJGameState rotation changes — but `GJGameState` is snapshotted by `LayerStateSnapshot` at runPlan boundaries, so it would roll back. The player velocity override is also written through PlayerObject directly. Like speed portals, this needs careful handling: ideally run-for-sim-only with snapshot rollback of any per-player state.
- **Recommended treatment**: `run-for-sim-only` candidate — similar pattern to speed-mod save-restore. Sim needs the gameplay rotation to update its physics prediction; real game state must not be affected. Snapshot the changed PlayerObject velocity/gravity fields around the super call.
- **Discriminator**: `dynamic_cast<RotateGameplayGameObject*>`.

### SequenceTriggerGameObject (line 16037)
- **Inherits**: `ChanceTriggerGameObject`
- **Purpose**: Fires its targets in sequence (1, 2, 3, …) with optional reset and unique-remap modes.
- **Unique virtual overrides**: `init`, `resetObject`, `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_sequenceState (SequenceTriggerState), m_minInt, m_sequenceMode, m_resetMode, m_reset, m_sequenceTotalCount, m_uniqueRemap`.
- **What firing does in real**: Advances the sequence pointer in `m_sequenceState` and dispatches the next target.
- **Sim impact analysis**: Sim advancing the sequence would cause real-game crossings to fire the wrong member. Suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<SequenceTriggerGameObject*>`.

### SFXTriggerGameObject (line 18277)
- **Inherits**: `EffectGameObject`
- **Purpose**: Plays a sound effect with full FMOD config: pitch, volume, fade in/out, reverb preset, looping, proximity-based volume, etc.
- **Unique virtual overrides**: `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: huge audio config block — `m_soundPath, m_soundID, m_pitch, m_speed, m_pitchIndex, m_volume, m_start, m_fadeIn, m_end, m_fadeOut, m_reverb, m_fastFourierTransform, m_loop, m_stopLoop, m_dontReset, m_unique, m_override, m_sfxUniqueID, m_volumeNear, m_volumeMedium, m_volumeFar, m_minDistNear, m_minDistMedium, m_minDistFar, m_proximityMode, m_cameraDistance, m_preload, m_ignoreVolumeTest, m_minInterval, m_sfxGroup, m_stop, m_changeSpeed, m_changeVolume, m_groupID, m_unk788, m_reverbPreset, m_reverbEnabled, m_soundDuration, m_applyDisabled, m_speedVariance, m_pitchVariance, m_volumeVariance, m_pitchSteps`.
- **What firing does in real**: Plays sound via FMOD.
- **Sim impact analysis**: Audio only — sim during search would emit audio spam (already filtered). Suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<SFXTriggerGameObject*>`. Field: `m_soundID != 0`.

### ShaderGameObject (line 18375)
- **Inherits**: `EffectGameObject`
- **Purpose**: Triggers shader effects (chromatic / bulge / glitch / grayscale / hue-shift / invert / motion-blur / pixelate / radial-blur / sepia / shockwave / split-screen / lens-circle / etc.) with timing and easing.
- **Unique virtual overrides**: `customSetup`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_speed, m_strength, m_outer, m_timeOff, m_waveWidth, m_targetX, m_targetY, m_fadeIn, m_fadeOut, m_screenOffsetX, m_screenOffsetY, m_invert, m_inner, m_maxSize, m_flip, m_rotate, m_dual, m_useX, m_useY, m_snapGrid, m_hardEdges, m_disableAll, m_zLayerMin, m_zLayerMax, m_animate, m_relative, m_editorDisabled`.
- **What firing does in real**: Dispatches into `ShaderLayer::trigger*` methods which mutate GPU shader uniforms via `GJShaderState`.
- **Sim impact analysis**: GPU effects only.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<ShaderGameObject*>`.

### SongTriggerGameObject (line 19171)
- **Inherits**: `SFXTriggerGameObject` (NOTE: not directly `EffectGameObject`)
- **Purpose**: Triggers song playback (different from SFX — full song with prep/load-prep flags and song channel).
- **Unique virtual overrides**: `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_unk7a9, m_prep, m_loadPrep, m_songChannel`. Inherits all of SFXTriggerGameObject's audio fields.
- **What firing does in real**: Switches the active song / plays a song on a channel.
- **Sim impact analysis**: Audio only.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<SongTriggerGameObject*>` — must check this BEFORE `SFXTriggerGameObject` cast since it's a subclass.

### SpawnParticleGameObject (line 19192)
- **Inherits**: `EffectGameObject`
- **Purpose**: Spawns a single particle burst at a target group's location with configurable offset/rotation/scale and variance.
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_offset, m_offsetVariance, m_matchRotation, m_rotation, m_rotationVariance, m_scale, m_scaleVariance`.
- **What firing does in real**: Emits a particle effect at the target.
- **Sim impact analysis**: Visual only.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<SpawnParticleGameObject*>`.

### SpawnTriggerGameObject (line 19240)
- **Inherits**: `EffectGameObject`
- **Purpose**: Spawns a target group of objects after a delay, with optional chance-based remap keys (so subsequent triggers fire on remapped targets) and remap reset.
- **Unique virtual overrides**: `init`, `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_remapObjects (vector<ChanceObject>), m_remapKey, m_remapKeys (vector<int>), m_currentDelay, m_spawnDelay, m_delayRange, m_resetRemap`.
- **What firing does in real**: Calls into `TriggerEffectDelegate::spawnGroup` / `spawnObject` to queue group activations.
- **Sim impact analysis**: Spawning a group propagates trigger cascades. During sim, the cascade would mutate EffectManager state and persist. Suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<SpawnTriggerGameObject*>`.

### StartPosObject (line 19373)
- **Inherits**: `EffectGameObject`
- **Purpose**: Marks an alternative start position in the editor (player can choose to start from here in practice).
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_startSettings (LevelSettingsObject*)`.
- **What firing does in real**: NEVER fires during play — `PlayLayer::setup` reads start positions before play begins.
- **Sim impact analysis**: None.
- **Recommended treatment**: `n/a` — but if `triggerObject` is somehow invoked, suppress.
- **Discriminator**: `dynamic_cast<StartPosObject*>`.

### TimerTriggerGameObject (line 19755)
- **Inherits**: `EffectGameObject`
- **Purpose**: Starts a timer that counts up or down, fires another trigger on reaching `m_targetTime`, with `m_dontOverride / m_ignoreTimeWarp / m_timeMod / m_startPaused / m_multiActivate / m_controlType` modes.
- **Unique virtual overrides**: `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_startTime (double), m_targetTime (double), m_stopTimeEnabled, m_dontOverride, m_ignoreTimeWarp, m_timeMod, m_startPaused, m_multiActivate, m_controlType`.
- **What firing does in real**: Inserts entries into `EffectManagerState::m_timerItemMap` and queues `TimerTriggerAction`s.
- **Sim impact analysis**: Mutates EffectManager state; suppress.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<TimerTriggerGameObject*>`.

### TransformTriggerGameObject (line 19869)
- **Inherits**: `EffectGameObject`
- **Purpose**: Transforms target group (scale X/Y, divide flags, relative scale/rotation, move-only mode).
- **Unique virtual overrides**: `triggerObject`, `customObjectSetup`, `getSaveString`.
- **Unique fields**: `m_objectScaleX, m_objectScaleY, m_property450, m_property451, m_onlyMove, m_divideX, m_divideY, m_relativeRotation, m_relativeScale`.
- **What firing does in real**: Applies transform action to target group.
- **Sim impact analysis**: Group transforms only — not player physics.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<TransformTriggerGameObject*>`.

### TriggerControlGameObject (line 19902)
- **Inherits**: `EffectGameObject`
- **Purpose**: Meta-trigger that controls other triggers (stop / enable / disable / etc. via `GJActionCommand` value).
- **Unique virtual overrides**: `triggerObject`, `customObjectSetup`, `getSaveString`. Plus `updateTriggerControlFrame()` helper.
- **Unique fields**: `m_triggerControlFrame (gd::string), m_customTriggerValue (GJActionCommand)`.
- **What firing does in real**: Modifies the state of other queued/registered trigger actions in `EffectManagerState`.
- **Sim impact analysis**: Suppress to keep effect-manager state stable.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<TriggerControlGameObject*>`.

### UISettingsGameObject (line 20136)
- **Inherits**: `EffectGameObject`
- **Purpose**: Sets UI overlay reference X/Y (HUD positioning), with relative-mode flags.
- **Unique virtual overrides**: `init`, `customObjectSetup`, `getSaveString`. **No `triggerObject` override.**
- **Unique fields**: `m_xRef, m_yRef, m_xRelative, m_yRelative`.
- **What firing does in real**: HUD config.
- **Sim impact analysis**: None.
- **Recommended treatment**: `suppress`.
- **Discriminator**: `dynamic_cast<UISettingsGameObject*>`.

---

## Non-trigger EnhancedGameObject subclasses

### AnimatedGameObject (line 481)
- **Inherits**: `EnhancedGameObject`, `AnimatedSpriteDelegate`, `SpritePartDelegate`
- **Purpose**: Wraps an `CCAnimatedSprite` tied to an animation ID (used for monsters, robots, etc. in decoration).
- **Unique virtual overrides**: `setOpacity, setChildColor, resetObject, activateObject, deactivateObject, setObjectColor, animationFinished, displayFrameChanged`.
- **Unique fields**: `m_animatedSprite, m_childSprite, m_eyeSpritePart, m_finishedAnimating, m_playingAnimation, m_currentAnimation, m_notGrounded, m_animationID`. Helpers: `playAnimation, setupAnimatedSize, setupChildSprites, updateChildSpriteColor, updateObjectAnimation`.
- **What firing does in real**: Plays animations triggered by group activation; pure visual.
- **Sim impact analysis**: None — not under EffectGameObject hook.
- **Recommended treatment**: `n/a`.
- **Discriminator**: `dynamic_cast<AnimatedGameObject*>`. Field: `m_animationID != 0`.

### ParticleGameObject (line 13822)
- **Inherits**: `EnhancedGameObject`
- **Purpose**: Renders a CCParticleSystemQuad with editor-configurable particle string and uniform color settings.
- **Unique virtual overrides**: many — `setScale*, setRotation*, setChildColor, customSetup, addMainSpriteToParent, resetObject, deactivateObject, customObjectSetup, getSaveString, claimParticle, unclaimParticle, particleWasActivated, setObjectColor, blendModeChanged, updateParticleColor, updateParticleOpacity, updateMainParticleOpacity, updateSecondaryParticleOpacity, updateSyncedAnimation, updateAnimateOnTrigger`.
- **Unique fields**: `m_particleData (gd::string), m_updatedParticleData, m_particleStruct, m_hasUniformObjectColor, m_popupPage, m_shouldQuickStart, m_respawnResult, m_startingRespawn, m_notPreviewing`. Plus helpers `applyParticleSettings, createAndAddCustomParticle, createParticlePreviewArt, setParticleString, updateParticle, updateParticleAngle, updateParticlePreviewArtOpacity, updateParticleScale, updateParticleStruct`.
- **What firing does in real**: Emits particles, can be activated by player touch or by group triggers.
- **Sim impact analysis**: Visual only.
- **Recommended treatment**: `n/a` — but note the `claimParticle / unclaimParticle / particleWasActivated` flow could leak in sim if it goes through other hooked paths.
- **Discriminator**: `dynamic_cast<ParticleGameObject*>`.

### SpecialAnimGameObject (line 19270)
- **Inherits**: `EnhancedGameObject`
- **Purpose**: Special animated decoration (used for robots/monsters etc. that have skin colors + extra sprite parts).
- **Unique virtual overrides**: `resetObject, customObjectSetup, getSaveString, updateMainColor, updateSecondaryColor, updateSyncedAnimation`.
- **Unique fields**: `m_skipMainColorUpdate, m_skipSecondaryColorUpdate`.
- **What firing does in real**: Plays its animation, syncs colors.
- **Sim impact analysis**: Visual only.
- **Recommended treatment**: `n/a`.
- **Discriminator**: `dynamic_cast<SpecialAnimGameObject*>`.

---

## Specialized GameObject direct subclasses

### EndPortalObject (line 4346)
- **Inherits**: `GameObject`
- **Purpose**: The end-of-level portal that completes the level when the player touches it.
- **Unique virtual overrides**: `init, setPosition, setVisible`. Also `triggerObject(GJBaseGameLayer*)` (non-virtual) plus `getSpawnPos, updateColors, updateEndPos`.
- **Unique fields**: `m_gradientBar (CCSprite*), m_flippedX, m_startPosHeightRelated`.
- **What firing does in real**: Completes the level on player contact.
- **Sim impact analysis**: CATASTROPHIC if sim triggers it — would end the real level. But sim never invokes `triggerObject` on EndPortalObject because EndPortalObject is NOT an EffectGameObject, so `TrajEffectHook` doesn't catch it. The sim must avoid colliding with it through other means (e.g., the sim's PlayerObject collision logic detects the goal and stops, without ending the real level). Verify this is the case in the existing collision-handling code.
- **Recommended treatment**: special — not via TrajEffectHook. Confirm the sim's collision path explicitly checks `dynamic_cast<EndPortalObject*>` and short-circuits.
- **Discriminator**: `dynamic_cast<EndPortalObject*>` or object ID (the "end portal" object ID is fixed).

### SmartGameObject (line 18960)
- **Inherits**: `GameObject`
- **Purpose**: Adjacency-aware decorative block — adjusts its sprite frame based on neighboring blocks.
- **Unique virtual overrides**: `customObjectSetup, getSaveString`.
- **Unique fields**: `m_referenceOnly, m_baseFrame (gd::string), m_smartFrame (gd::string)`. Helper: `updateSmartFrame`.
- **What firing does in real**: Updates its visual sprite frame on level load / neighbor changes.
- **Sim impact analysis**: Visual only.
- **Recommended treatment**: `n/a`.
- **Discriminator**: `dynamic_cast<SmartGameObject*>`.

### TextGameObject (line 19685)
- **Inherits**: `GameObject`
- **Purpose**: Editor-placed text block (decorative text).
- **Unique virtual overrides**: `customObjectSetup, getSaveString, updateTextKerning, getTextKerning`.
- **Unique fields**: `m_text (gd::string), m_kerning`. Helper: `updateTextObject(text, defaultFont)`.
- **What firing does in real**: Renders text in level.
- **Sim impact analysis**: Visual only.
- **Recommended treatment**: `n/a`.
- **Discriminator**: `dynamic_cast<TextGameObject*>`.

### PlayerObject (line 14239)
- **Inherits**: `GameObject`, `AnimatedSpriteDelegate`
- **Purpose**: The player. Already audited separately.
- **Recommended treatment**: `n/a` via TrajEffectHook — gated by `TrajPlayerObjectHook` instead.

---

## Master classification summary table

| Class | Inherits from | What it does | Affects sim physics? | Recommended treatment | Discriminator |
| --- | --- | --- | --- | --- | --- |
| AdvancedFollowTriggerObject | EffectGameObject | Group advanced-follow config | No | suppress | `dynamic_cast<AdvancedFollowTriggerObject*>` |
| AdvancedFollowEditObject | AdvancedFollowTriggerObject | Live-edit follow command | No | suppress | `dynamic_cast<AdvancedFollowEditObject*>` |
| ArtTriggerGameObject | EffectGameObject | Switch art set | No | suppress | `dynamic_cast<ArtTriggerGameObject*>` |
| AudioLineGuideGameObject | EffectGameObject | BPM guideline (editor) | No | n/a | `dynamic_cast<AudioLineGuideGameObject*>` |
| CameraTriggerGameObject | EffectGameObject | Camera mode | No | suppress | `dynamic_cast<CameraTriggerGameObject*>` |
| ChanceTriggerGameObject | EffectGameObject | Chance pool base | No | suppress | `dynamic_cast<ChanceTriggerGameObject*>` |
| CheckpointGameObject | EffectGameObject | Auto-checkpoint | No (must suppress) | suppress | `dynamic_cast<CheckpointGameObject*>` |
| CountTriggerGameObject | EffectGameObject | Item count fire | Indirect | suppress | `dynamic_cast<CountTriggerGameObject*>` |
| EndTriggerGameObject | EffectGameObject | End level | NO — never sim-fire | suppress (critical) | `dynamic_cast<EndTriggerGameObject*>` |
| EnhancedTriggerObject | EffectGameObject | Area-trigger base | No | suppress | `dynamic_cast<EnhancedTriggerObject*>` |
| EnterEffectObject | EffectGameObject | Enter/exit anim | No | suppress | `dynamic_cast<EnterEffectObject*>` |
| EventLinkTrigger | EffectGameObject | Event-bus emit | Indirect | suppress | `dynamic_cast<EventLinkTrigger*>` |
| ForceBlockGameObject | EffectGameObject | Force pad (collision-based) | YES (collision path) | run-for-sim-only via collision (not TrajEffectHook) | `dynamic_cast<ForceBlockGameObject*>` |
| GameOptionsTrigger | EffectGameObject | Toggle global options | Maybe (controls disable) | suppress | `dynamic_cast<GameOptionsTrigger*>` |
| GradientTriggerObject | EffectGameObject | Gradient render | No | suppress | `dynamic_cast<GradientTriggerObject*>` |
| ItemTriggerGameObject | EffectGameObject | Item math op | Indirect | suppress | `dynamic_cast<ItemTriggerGameObject*>` |
| KeyframeAnimTriggerObject | EffectGameObject | Keyframe anim mod | No | suppress | `dynamic_cast<KeyframeAnimTriggerObject*>` |
| KeyframeGameObject | EffectGameObject | Keyframe waypoint | No | suppress | `dynamic_cast<KeyframeGameObject*>` |
| LabelGameObject | EffectGameObject | Text label | No | suppress | `dynamic_cast<LabelGameObject*>` |
| ObjectControlGameObject | EffectGameObject | Object collision toggle | Indirect | suppress | `dynamic_cast<ObjectControlGameObject*>` |
| PlayerControlGameObject | EffectGameObject | Disable jump/move | YES | suppress | `dynamic_cast<PlayerControlGameObject*>` |
| RandTriggerGameObject | ChanceTriggerGameObject | Random group | No (RNG) | suppress | `dynamic_cast<RandTriggerGameObject*>` |
| RingObject | EffectGameObject | Orb base class | YES (via separate path) | suppress (orb path handles it) | `dynamic_cast<RingObject*>` |
| RotateGameplayGameObject | EffectGameObject | Rotate gameplay/gravity | YES | run-for-sim-only (snapshot needed) | `dynamic_cast<RotateGameplayGameObject*>` |
| SequenceTriggerGameObject | ChanceTriggerGameObject | Sequence groups | No | suppress | `dynamic_cast<SequenceTriggerGameObject*>` |
| SFXTriggerGameObject | EffectGameObject | Play SFX | No | suppress | `dynamic_cast<SFXTriggerGameObject*>` |
| ShaderGameObject | EffectGameObject | Shader effect | No | suppress | `dynamic_cast<ShaderGameObject*>` |
| SongTriggerGameObject | SFXTriggerGameObject | Play song | No | suppress | `dynamic_cast<SongTriggerGameObject*>` (check before SFX) |
| SpawnParticleGameObject | EffectGameObject | Particle burst | No | suppress | `dynamic_cast<SpawnParticleGameObject*>` |
| SpawnTriggerGameObject | EffectGameObject | Spawn group cascade | No | suppress | `dynamic_cast<SpawnTriggerGameObject*>` |
| StartPosObject | EffectGameObject | Start position marker | No (never fires) | n/a | `dynamic_cast<StartPosObject*>` |
| TimerTriggerGameObject | EffectGameObject | Timer counter | No | suppress | `dynamic_cast<TimerTriggerGameObject*>` |
| TransformTriggerGameObject | EffectGameObject | Group transform | No | suppress | `dynamic_cast<TransformTriggerGameObject*>` |
| TriggerControlGameObject | EffectGameObject | Stop/enable triggers | Indirect | suppress | `dynamic_cast<TriggerControlGameObject*>` |
| UISettingsGameObject | EffectGameObject | HUD positioning | No | suppress | `dynamic_cast<UISettingsGameObject*>` |
| AnimatedGameObject | EnhancedGameObject | Animated sprite wrapper | No | n/a | not under TrajEffectHook |
| ParticleGameObject | EnhancedGameObject | Particle system | No | n/a | not under TrajEffectHook |
| SpecialAnimGameObject | EnhancedGameObject | Special anim deco | No | n/a | not under TrajEffectHook |
| EndPortalObject | GameObject | Level-end portal | YES (touch ends level) | special — collision path | not under TrajEffectHook |
| SmartGameObject | GameObject | Adjacency-aware deco | No | n/a | not under TrajEffectHook |
| TextGameObject | GameObject | Text block deco | No | n/a | not under TrajEffectHook |
| PlayerObject | GameObject | The player | YES | n/a (TrajPlayerObjectHook) | not under TrajEffectHook |

---

## Field-discriminator cheat sheet

`EffectGameObject` has many discriminator fields in its base class (lines 4010-4261). For per-family suppression without `dynamic_cast` in the hot path, ordered cheapest-first:

- `m_speedModType != 0` → speed mod (already used; the only currently-permitted case)
- `m_animationID != 0` → animate trigger
- `m_secretCoinID != 0` → user coin object (RingObject-related secret coin)
- `m_pulseMode != 0` → pulse trigger (color pulse)
- `m_zoomValue != 0.f` → camera zoom embedded in trigger
- `m_timeWarpTimeMod != 0.f` → time-warp trigger
- `m_gravityValue != 0.f` → gravity trigger
- `m_gravityMod != 0.f` → gravity modulation
- `m_targetGroupID != 0` plus `m_moveOffset != (0,0)` → move trigger
- `m_targetGroupID != 0` plus `m_activateGroup` → toggle / spawn dispatch
- `m_hsvValue` non-default → HSV / color trigger
- `m_followYMod / m_followXMod / m_followYSpeed` non-zero → follow trigger
- `m_cameraEditCameraSettings` true → camera settings embedded
- `m_isTouchTriggered / m_isSpawnTriggered / m_isMultiTriggered / m_collectibleIsPickupItem / m_collectibleIsToggleTrigger` are boolean classifiers within the base.

For subclass-specific discrimination, only a few have a cheap field check that uniquely tags the class:
- `m_soundID != 0` and `m_soundPath != ""` → `SFXTriggerGameObject` (also `SongTriggerGameObject` since it inherits)
- `m_artIndex != 0` → `ArtTriggerGameObject`
- `m_force != 0.f` or non-null `m_forceID` → `ForceBlockGameObject` (with caveat that force can legitimately be zero)
- `m_eventIDs` non-empty → `EventLinkTrigger`
- `m_chanceObjects` non-empty → `ChanceTriggerGameObject` or its subclasses (`RandTrigger`, `SequenceTrigger`)
- `m_text` non-empty → `LabelGameObject` (label) or `TextGameObject` (non-trigger)
- `m_startSettings != nullptr` → `StartPosObject`
- `m_checkpointActivated` exists → `CheckpointGameObject` (but reading derived-class fields from base pointer is unsafe)

In practice, `dynamic_cast` is cheap enough at the trigger-fire rate (a few hundred per second peak) that branching on `dynamic_cast` per-family in `TrajEffectHook::triggerObject` is fine. The optimization opportunities would only matter if you wanted to route ALL non-speed-mod triggers without ANY dynamic_cast in the hot path — and the speed-mod check (`m_speedModType != 0`) already filters that to the rarer non-speed cases.

Recommended hook structure (sketch):

```cpp
void TrajEffectHook::triggerObject(GJBaseGameLayer* layer, int uid, gd::vector<int> const* remap) {
    if (!sim().isSimulating()) {
        EffectGameObject::triggerObject(layer, uid, remap);
        return;
    }
    // Critical: never let sim fire end trigger
    if (dynamic_cast<EndTriggerGameObject*>(this)) return;
    // Speed mod: save/restore real player speed + per-object activation
    if (m_speedModType != 0) {
        bool p1 = m_activatedByPlayer1, p2 = m_activatedByPlayer2;
        saveRestoreRealPlayerSpeeds([&]{
            EffectGameObject::triggerObject(layer, uid, remap);
        });
        m_activatedByPlayer1 = p1; m_activatedByPlayer2 = p2;
        return;
    }
    // RotateGameplay: physics-affecting, but cheaper to suppress until proven needed
    // if (dynamic_cast<RotateGameplayGameObject*>(this)) { ... save-restore pattern ... }
    // Everything else: suppress
}
```

This keeps the current behavior identical (all non-speed-mod suppressed) while making it easy to add per-family allow-lists or save-restore branches in future.
