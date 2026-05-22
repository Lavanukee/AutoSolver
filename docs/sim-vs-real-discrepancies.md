# Sim vs Real — Discrepancy Catalogue & Toggle Reference

**Audience.** Anyone working on this mod. For every place sim behaves differently from real, this lists:

1. **Code item** — exact file:line + method/predicate that creates the divergence
2. **Manifest** — what you'd see in-game when the discrepancy IS in effect (current behavior) and what would happen if it were removed
3. **Subcategories** — finer granularity where one hook covers a family (e.g. trigger types)
4. **Toggle blueprint** — exact code to add an on/off switch
5. **Toggle behavior** — what flipping the switch looks like in-game

**Reading order.** Sections 1–11 grouped by domain: physics inputs (1–5), trigger/effect dispatch (6), audio/visual side effects (7), scene-graph mutation safety (8), driver-level (9), creation/lifecycle (10), known-unisolated suspects (11). Section 12 lists the proposed unified setting surface.

The mod's invariant: **sim runs on the real `PlayLayer` instance, NOT a copy.** Every isolation mechanism below exists because of that constraint. Removing one without replacement = sim mutations leak into the real game.

---

## Boilerplate for adding a new toggle

You'll do this repeatedly below. The pattern:

1. Add a setting to `mod.json` `settings` block (with default).
2. In `TrajectorySimulator` (`src/Trajectory.hpp`): add `bool m_xxx{default};` + `setXxx(bool)` + `xxx() const`.
3. In `src/main.cpp` `$execute`: initial read via `mod->getSettingValue<bool>("xxx")` + `listenForSettingChanges<bool>("xxx", ...)`.
4. In the hook (`src/Hooks.cpp` or similar): gate the suppression on `s.xxx()`.

For numeric settings use `int64_t` (Geode setting type) and `static_cast<int>` at the boundary.

For a TRIGGER-FAMILY toggle (sub-toggle inside an existing suppression), the gate is inside `TrajEffectHook::triggerObject` — add a switch on the object's runtime type (cast to subclass, or check `m_speedModType`, `m_targetGroupID`, `m_animationID`, etc.) and consult the per-family flag before deciding whether to suppress.

---

## 1. Object filtering at collision

**Code item.** `src/Hooks.cpp:127-191` — `TrajBaseLayerHook::collisionCheckObjects` replaces the engine's full object list with a filtered subset before delegating to super, but ONLY when `sim().isSimulating()` is true.

**Manifest with filter active (today).** Sim's physics sees a smaller world than real: only objects in a window around the sim's position, minus coins, optionally minus pads/orbs/portals, minus anything sim has marked destroyed this run.

**Manifest if filter removed.** Sim would collide with every object real does. Including coins (which then get COLLECTED via the engine's collision-pass coin-collect path, mutating real coin progress for the level). Sim would re-collide with breakable blocks it had already broken earlier in the same plan (sim physics can't penetrate something the engine considers solid). Far-future and far-past objects would burn CPU without contributing to a decision the bot can act on within the planning horizon.

### 1.a Spatial cull window
**Code.** `src/Hooks.cpp:142-186`. Constants `kAheadBlocks=80`, `kBackBlocks=30` (× 30 units = 2400 ahead, 900 behind, sim's X position). Back side is width-aware (drops only if `obj.x + obj.width < minX`); front side is NOT width-aware (wide moving platforms originating far ahead could be culled).
**Toggle blueprint.** Add `m_spatialCull{true}` + setter/getter; replace constants with `s.cullAheadBlocks()` / `s.cullBackBlocks()` calls reading members; gate the cull block on `s.spatialCull()`.
**Toggle behavior.**
- ON (current): sim only collides with objects within the window. Symmetric for performance and prediction stability.
- OFF: sim collides with every object in `vec`. Used to confirm cull isn't dropping something we need; expected to be expensive at high object counts but verifiable correct.

> Note: the prior implementation of `cull-ahead-blocks`/`cull-back-blocks` mod settings produced no observable FPS change. Most likely cause: PlayLayer's own section-indexing (`m_leftSectionIndex`/`m_rightSectionIndex`) already culls upstream, so `vec` is already small by the time we see it. The setting is correctness-symmetric but performance-neutral; see `project_bookmarked_bugs.md`.

### 1.b Sim-destroyed filter
**Code.** `src/Hooks.cpp:163` filters out anything in `s.isSimDestroyed(obj)`. The set is fed by `TrajBaseLayerHook::destroyObject` (`src/Hooks.cpp:118`) intercepting all calls during sim and writing to `TrajectorySimulator::m_simDestroyed`.
**Manifest.** A breakable block sim has destroyed earlier in this runPlan is gone from sim's collision view → sim "breaks through" the same way real eventually would.
**Toggle blueprint.** Add `m_destroyIsolation{true}`. Two gates needed: in `destroyObject`, when off, just call super (real-game-affecting). In `collisionCheckObjects`, when off, skip the `isSimDestroyed` filter line.
**Toggle behavior.**
- ON (current): sim physics models breakability without mutating real level. The `destroyObject` super never fires from sim; real player still has to break the block itself.
- OFF: sim's `destroyObject` lets super run → real level mutates whenever sim "breaks" something. Breaks the no-leakage invariant; useful only for proving the sim-destroyed path matters.

### 1.c Type-family filters
**Code.** `src/Hooks.cpp:166-177`. `wantsPads/Orbs/Portals` gate the standard families; coins (UserCoin=31, SecretCoin=22) are dropped UNCONDITIONALLY.
**Manifest.**
- Pads/orbs/portals off: sim ignores those object types entirely (passes through, no bounce/activation). Used to diagnose whether they're contributing to a bug.
- Coins always off: sim never "collects" a coin. The collision-pass coin-collect path doesn't fire, so real coin progress isn't touched by sim crossings.
**Existing toggles.** `pads`, `orbs`, `portals` mod settings (bool). Coin filter has NO toggle — should it? Adding `m_coinIsolation{true}` is one line: gate the unconditional drop on `s.coinIsolation()`.
**Toggle behavior.**
- pads/orbs/portals: bot ignores that category when off; trajectory predictions exclude the bounce/activation.
- coin-isolation (proposed): OFF lets sim collect coins. NOT recommended for normal use; you'd see coin counter increment from sim crossings, breaking the no-leakage invariant.

---

## 2. Player state copy (real → sim) at run start

**Code item.** `src/Trajectory.cpp:538-590` (`runBranch`) and `src/Trajectory.cpp:631-670` (`runPlan::initSim`). Calls in this order: `copyAttributes(base)` → explicit per-field copies → position resync → `clearSimRingState` → `clearActivated` → `clearSimDestroyed` → `clearSimDead`.

**Manifest with copy active.** Sim starts each branch/plan from "as close to real as we know how to make it" — same position, velocity, gravity, mode, slope state, ground state, jump-buffer state, slide state.

**Manifest if copy removed.** Sim drifts from real. After 1–2 ticks the sim is on a different ballistic arc, gravity flipped wrong, mode mismatched, etc. Visible as the orange trajectory being wildly disconnected from the green real-player path even immediately at frame 0.

### 2.a Engine-supplied copy via `copyAttributes`
**Code.** `sim->copyAttributes(base)` — engine method at PlayerObject vtable. Covers a large but incomplete set of fields (empirically derived: the explicit copies that follow in lines 555-586 are fields we found `copyAttributes` dropping).
**Toggle blueprint.** Not toggleable directly (engine internal). To diagnose "is `copyAttributes` covering field X?" — set X on `sim` BEFORE calling `copyAttributes` to a sentinel value, call `copyAttributes`, then read X. If still sentinel, `copyAttributes` doesn't cover it.

### 2.b Explicit per-field copy list
**Code.** `src/Trajectory.cpp:545-586` and `:634-666` (must stay in sync between runBranch and runPlan::initSim). Fields grouped by purpose:
- Slope/sliding state: `m_isSliding`, `m_maybeSlopeForce`, `m_slopeAngle`, `m_slopeSlidingMaybeRotated`, `m_isOnIce`, `m_maybeGoingCorrectSlopeDirection`, `m_maybeUpsideDownSlope`, `m_isOnSlope`, `m_wasOnSlope`, `m_slopeVelocity`
- Ground state: `m_groundObjectMaterial`, `m_stateOnGround`, `m_lastGroundObject`, `m_preLastGroundObject`, `m_currentSlope2`, `m_isOnGround2/3/4`, `m_lastLandTime`, `m_lastGroundedPos`
- Collision pointers: `m_collidedObject`, `m_collidingWithLeft`, `m_collidingWithRight`
- Jump buffer: `m_jumpBuffered`, `m_wasJumpBuffered`, `m_stateJumpBuffered`
- Other: `m_vehicleSize`, `m_gravityMod`, `m_isOnGround`

**Toggle blueprint.** Add `m_explicitFieldCopies{true}`. Gate the entire block in both runBranch and runPlan::initSim. Easier as a single helper `seedExplicitFields(sim, base)` that no-ops when off.
**Toggle behavior.**
- ON (current): sim inherits these fields; trajectories should track real well across mode/slope transitions.
- OFF: pre-Round-1 behavior. Wave gamemode Y-offset bug returns (`docs/issue-wave-ground-y-offset.md`); slope physics drift; jump-buffer mismatch produces ghost jumps. Diagnostic only.

### 2.c Position resync
**Code.** `sim->setPosition(base->getPosition())` immediately after the per-field copies. Belt-and-suspenders for wave mode where `copyAttributes` may not push CCNode position.
**Toggle.** Not worth a toggle — single-line, no side effects.

---

## 3. Per-run sim state reset (`clearSimRingState`)

**Code item.** `src/Trajectory.cpp:459-497`. Runs AFTER copy, so it OVERWRITES whatever the copies brought in. Called at start of each runBranch/runPlan.

**Manifest with reset active.** Each new run starts with sim's orb/dash/pad-related state cleared, regardless of what the real player is currently doing.

**Manifest if reset removed.** Sim inherits real's currently-touching-an-orb state, mid-dash state, mid-pad-bounce state. Sim's first frame would re-fire the orb activation real has already consumed → sim gets extra impulse real doesn't get. This was the original pre-clearSimRingState bug pattern (+10.4 yVel divergence in the prior session's `divergence.log`).

### 3.a Per-field breakdown (each field is independently togglable)
| Field reset | Engine role | If kept (no clear) | Recommended toggle | Default |
|---|---|---|---|---|
| `m_dashRing = nullptr` | Pointer to active dash orb | Sim renders the dash-raycast visual; sim inherits real's dash | `m_clearDashRing` | on |
| `m_isDashing = false`, `m_dashX/Y/Angle/StartTime = 0` | Mid-dash flag + dash vector | Sim continues real's dash physics through frame 0 | `m_clearDashState` | on |
| `m_padRingRelated = false` | Pad/ring linkage cache | Sim sees real's last pad/ring pair as active | `m_clearPadRingRelated` | on |
| `m_ringJumpRelated = false` | Ring-jump flag | Sim thinks it just did a ring-jump | `m_clearRingJumpRelated` | on |
| `m_ringRelatedSet.clear()` | Container of touched rings | Sim treats real's recent rings as already-touched | `m_clearRingRelatedSet` | on |
| `m_stateRingJump = false`, `m_stateRingJump2 = false` | Ring-jump state | Sim's ring-jump state pre-set | `m_clearRingJumpState` | on |
| `m_touchedRing = false`, `m_touchedCustomRing = false` | Per-tick edge bools | Sim's first frame thinks an orb was just touched | `m_clearTouchedRingFlags` | on |
| `m_touchingRings = ownArray; .removeAllObjects()` | Currently-overlapping rings | **CRITICAL** — without re-pointing, sim shares real's CCArray; the clear empties REAL'S array. NOT optional. | (always on, no toggle) | n/a |
| `m_touchedRings.clear()` | Per-frame touched-set | Sim sees real's most recent touched rings | `m_clearTouchedRings` | on |
| `m_jumpPadRelated.clear()` | Engine's "this player consumed this pad" map | Without clear, sim inherits real's "consumed" set; with clear, engine RE-fires pads sim overlaps that real had consumed → sim gets +10.4 yVel real doesn't | `m_clearJumpPadRelated` | **on, with caveat** |

**Caveat on `m_jumpPadRelated`.** Clearing it solves "sim ignores pads real has already used" but creates "sim re-fires pads real has used since prior tick." There's a deeper fix: don't clear; instead, seed sim's map from base's map at copyAttributes time. If `copyAttributes` doesn't carry it, do `sim->m_jumpPadRelated = base->m_jumpPadRelated;` explicitly. With the seed, sim sees the same consumed-pad set as real → no double-fire.

**Toggle blueprint (per-field).**
```cpp
// in Trajectory.hpp
struct SimClearFlags {
    bool dashRing{true};
    bool dashState{true};
    bool padRingRelated{true};
    bool ringJumpRelated{true};
    bool ringRelatedSet{true};
    bool ringJumpState{true};
    bool touchedRingFlags{true};
    bool touchedRings{true};
    bool jumpPadRelated{true};
};
SimClearFlags m_clear;
SimClearFlags const& clearFlags() const { return m_clear; }
void setClear(SimClearFlags v) { m_clear = v; }
```
Then in `clearSimRingState`, gate each line on the corresponding flag.

**Toggle behavior.** Each field becomes a per-suspicion diagnostic toggle. Standard use: leave defaults until a specific bug repro suggests sim is dropping or carrying too much state, then flip the specific field and observe.

---

## 4. Per-tick sim state reset (`clearPerTickRingOverlap`)

**Code item.** `src/Trajectory.cpp:499-512`. Called BEFORE every `m_pl->checkCollisions(sim, ...)` inside the runPlan/runBranch loop.

**Manifest.** Mirrors what the engine does for real internally each tick: clear the "currently overlapping rings" set, clear the per-tick edge bools. `checkCollisions` then re-populates the set with rings actually overlapping THIS tick.

**Without it.** `m_touchingRings` accumulates across ticks. Sim's ringJump fires on rings sim has already left → "ghost orb activation" (the `ba898c8` regression). Visible as sim trajectory snapping upward off an orb sim is no longer over.

**Toggle.** Not recommended. This is symmetric with engine behavior for real; clearing here matches the engine's own per-tick clear, so it's not really a discrepancy — it's "do for sim what engine does for real."

If you want one anyway: `m_perTickRingOverlapClear{true}` gating both lines.

---

## 5. Layer-state snapshot/restore (`LayerStateSnapshot`)

**Code item.** `src/Trajectory.cpp:34-227`. Captured at runPlan/runBranch start, restored at exit (RAII-style via destructor-like manual restore at scope end).

**Purpose.** ANY field the sim's super calls mutate that is NOT covered here will leak into the real game. The snapshot is the rollback boundary.

**Manifest with snapshot.** Sim crosses portals/triggers DURING the run (mutations persist tick-to-tick within one plan, which is correct for sim physics), but at plan end real game state is bit-identical to plan start.

**Manifest without snapshot.** Each sim run permanently mutates the layer. Camera zoom drifts, dual mode flips at random, level flip ping-pongs, time warp accumulates, ground bars slide off-screen, speed multipliers stack.

### 5.a Captured field families
| Family | Fields | Why captured |
|---|---|---|
| Camera | `m_cameraZoom`, `m_targetCameraZoom`, `m_cameraOffset`, `m_cameraPosition`, `m_cameraPosition2`, `m_cameraAngle`, `m_targetCameraAngle`, `m_cameraEdgeValue0/1/2/3`, `m_cameraShakeEnabled`, `m_cameraShakeFactor`, `m_cameraStepDiff` | Camera triggers move camera during sim; restore prevents real camera from snapping |
| Mode | `m_isDualMode`, `m_dualRelated` | Sim crossing dual portal flips dual mode globally |
| Gravity/flip | `m_levelFlipping`, `m_gravityRelated` | Sim crossing flip portal toggles level flip |
| Mode-portal Y | `m_portalY`, `m_middleGroundOffsetY` | Mode portals slide the gameplay area Y bound |
| Portal markers | `m_lastActivatedPortal1`, `m_lastActivatedPortal2` | Sim crossing portal stores marker |
| Time warp | `m_timeWarp`, `m_queuedTimeWarp`, `m_timeWarpRelated` | Sim crossing time-warp trigger changes game speed |
| Trigger channels | `m_currentChannel`, `m_rotateChannel` | Trigger group dispatch channels |
| Speed objects | `m_speedObjects` (full pointer-set snapshot/restore) | Sim entering speed portal adds to array; without restore real speeds permanently for the level |
| Ground bars | `m_groundLayer` / `m_groundLayer2` / `m_middleground` positions + `stopAllActions` on each | Mode portals start CCAction tweens; restore + stopAllActions cancels the tween + restores position |
| Time-mod cache | `m_timeModRelated`, `m_timeModRelated2` | Per-tick speed cache; without restore real game runs at sim-determined speed next tick |
| **POD prefix** | All ~30 m_unkPoint + m_unkFloat/Int/Bool + named POD fields up to `m_spawnChannelRelated0` | Brute-force memcpy of every POD byte in the prefix; ensures any field we didn't enumerate is still restored |

**Toggle blueprint (per family).**
```cpp
// in TrajectorySimulator
struct SnapshotFlags {
    bool camera{true};
    bool mode{true};
    bool gravity{true};
    bool portalY{true};
    bool portalMarkers{true};
    bool timeWarp{true};
    bool channels{true};
    bool speedObjects{true};
    bool groundBars{true};
    bool timeModCache{true};
    bool podPrefix{true};   // master fallback — overrides per-field, leave on
};
```
Gate each `capture()` block AND its `restore()` block on the corresponding flag. (Capture without restore wastes work; restore without capture is undefined.)

**Toggle behavior.** OFF for any family = real game state in that family leaks from sim. Diagnostic only. Use case: bisect "which family is responsible for visible artifact X" by flipping families off one at a time.

### 5.b Fields/state NOT in the snapshot (leak candidates)
| What | Where mutated | Risk | Mitigation today |
|---|---|---|---|
| `m_groupNodes` per-group flags | Color/move/toggle triggers via `toggleGroupTriggered` | High — trigger suppression today blocks this at source (§6) | `TrajEffectHook::triggerObject` skips non-speed-mod for sim |
| `m_effectManager` state (EffectManagerState) | Pulse/spawn/toggle/count/collision/timer trigger actions | High — same as above | Same as above |
| CCAction tweens on color-action / opacity-action / move-action / rotate-action objects | Triggers spawn ColorActions that step independently | Medium — actions live attached to ColorAction objects, not directly the layer; snapshot can't reach them | Trigger suppression at source |
| `m_holdingButtons` map on `m_player1` / `m_player2` | If sim's super calls `pushButton` on real (shouldn't, but if any path does) | Low — gated by isSimPlayer | None additional |
| Real player's `m_playerSpeed` / `m_playerSpeedAC` | Speed portals via inline write in `updateTimeMod` super | Mitigated — `TrajLayerSpeedHook` saves/restores around super | `src/Hooks.cpp:444-477` |
| Real player's other fields (m_lastPortalPos, m_isDashing, etc.) if any engine path writes them during sim | Unknown — would be a bug | Unknown | None — would need diagnostic |
| Spawn-trigger queue | spawnGroup / spawnObject delayed spawns | Medium — sim crossing a spawn trigger could queue a real-game spawn | Trigger suppression blocks at source |
| Item counts | `pickupItem`, `processItems`, `m_itemCountMap` | Sim hitting a pickup increments a counter the real game also reads | Trigger suppression blocks the pickup pathway |
| Coin scoring | `collectedObject` for coin triggers | High — covered by §1.c coin filter | Coin drop in `collisionCheckObjects` |
| Audio trigger queue | `clearActivatedAudioTriggers`, `processQueuedAudioTriggers` | Medium — if any sim path queues audio, real game plays it | No isolation — see §7 |

---

## 6. Trigger / effect dispatch (the per-family suppression surface)

**Code item.** `src/Hooks.cpp:479-547` — `TrajEffectHook::triggerObject` and `triggerActivated`. Whole-family gate: if `isSimulating()` AND NOT `isSpeedMod()`, return without calling super.

**Manifest today.** Sim never sees any non-speed-mod trigger fire. The sim's trajectory is computed AS IF the level had no triggers except speed mods. Speed mods get a special save-restore wrapper (`saveRestoreRealPlayerSpeeds`) so sim picks up the new speed while real is left untouched.

**Trade-off.** Sim's prediction is wrong on levels where triggers matter (color triggers that toggle block visibility, move triggers that slide hazards, scale triggers that shrink the playable area, etc.). The bot picks paths assuming the trigger never fired. But: if we let triggers fire for sim, they fire on REAL groups — color/move/scale/toggle of every visible block, mutating real game state mid-prediction.

**The right fix is per-family granular suppression**: for each trigger type, decide which side effects matter to sim physics (must fire) vs. which are real-game mutation (must suppress / save-restore). The current code does this only for speed mods. Below: every other family + recommended treatment.

### 6.a Trigger family inventory

Triggers are subclasses of `EffectGameObject` (and through it, `EnhancedGameObject`). The dispatch lives in `EffectGameObject::triggerObject` (per-trigger virtual) — every subclass overrides this. The bindings give us:

| Subclass | What it does in real | Affects sim physics? | Recommended treatment |
|---|---|---|---|
| **`StartPosObject`** | Marks alternate start point | No (engine handles before play) | Suppress (no-op) |
| **`CheckpointGameObject`** | Records checkpoint | No (sim doesn't use checkpoints) | Suppress |
| **`EndTriggerGameObject`** | Ends the level | Yes — but sim shouldn't trigger "level complete" for real | Suppress |
| **`CameraTriggerGameObject`** | Camera zoom/move/edges | No (camera doesn't affect sim collision) | Suppress |
| **`AdvancedFollowTriggerObject`** | Camera follow-player advanced | No | Suppress |
| **`AdvancedFollowEditObject`** | Edit follow trigger | No | Suppress |
| **`ColorTriggerGameObject`** (BGColor / GColor / G2Color / objColor / lineColor / 3DLColor — all dispatched via EffectGameObject base with `m_targetColorIndex`) | Changes color of color channels (background, ground, objects) | No (color doesn't change hitboxes) | Suppress |
| **`GradientTriggerObject`** | Gradient overlay | No | Suppress |
| **`PulseTriggerGameObject`** (via base) | Pulse color | No | Suppress |
| **Move triggers** (via base; identified by `m_moveOffset`, `m_useMoveTarget`, `m_targetGroupID`) | Moves a group of objects | **Yes** — if the group contains blocks/hazards, sim needs to see them at moved position. **Critical.** | Save-restore (run super, capture group positions pre, restore post). Complex — needs per-group snapshot. Today: suppressed (sim sees blocks at static positions). |
| **Toggle trigger** (`m_activateGroup`) | Toggles a group enabled/disabled | **Yes** — disabled blocks shouldn't collide | Save-restore (capture per-group enabled flags). Today: suppressed. |
| **Stop trigger** | Stops other triggers | Indirect | Suppress |
| **Pulse / Fade** (color action) | Color changes | No | Suppress |
| **Animation trigger / `KeyframeAnimTriggerObject`** | Triggers object animation | Maybe (if animation affects hitbox bounds) | Suppress (rarely affects collision) |
| **`KeyframeGameObject`** | Keyframe data | Container; no direct dispatch | n/a |
| **`SpawnTriggerGameObject`** | Triggers another group's trigger | Chain to whatever it spawns | Suppress (else recursive trigger leak) |
| **`RandTriggerGameObject` / `ChanceTriggerGameObject` / `SequenceTriggerGameObject`** | Random/sequence spawn | Same as Spawn | Suppress |
| **Rotate / RotateGameplay** (`RotateGameplayGameObject`) | Rotates a group OR the entire gameplay | **Yes** — RotateGameplay literally changes gravity direction | Save-restore for gameplay-rotate; suppress group-rotate. Today: suppressed. |
| **`SFXTriggerGameObject`** | Plays SFX | No (audio only) | Suppress |
| **`SongTriggerGameObject`** | Changes song | No | Suppress |
| **`SpawnParticleGameObject`** | Spawns particle | No (visual only) | Suppress (already done at `spawnParticleTrigger`) |
| **`ShaderGameObject`** | Activates shader | No | Suppress |
| **`ForceBlockGameObject`** | Adds force to player | **Yes** — physics-modifying | Run for sim (sim should feel the force) — currently suppressed; bug if hit |
| **`GameOptionsTrigger`** | Toggles options | No | Suppress |
| **`ArtTriggerGameObject`** | Art swap | No | Suppress |
| **`AudioLineGuideGameObject`** | Audio guideline | No | Suppress |
| **`CountTriggerGameObject` / `ItemTriggerGameObject` / `TimerTriggerGameObject`** | Item/timer state | Indirect (some block visibility depends on items) | Suppress; consider save-restore if a level uses item-gated collision |
| **`EventLinkTrigger`** | Custom event | Indirect | Suppress |
| **`PlayerControlGameObject`** | Disables player input | **Yes** — sim should also be input-disabled | Run for sim (only when target is sim player) — currently NOT special-cased |
| **`ObjectControlGameObject`** | Object enable/disable | **Yes** (if controlling collision objects) | Save-restore. Today: suppressed. |
| **`TriggerControlGameObject`** | Trigger control | Indirect | Suppress |
| **`EnterEffectObject`** | Enter-area effect | No | Suppress |
| **`LabelGameObject`** | Label text | No | Suppress |
| **`TransformTriggerGameObject`** | Group scale/rotate transform | **Yes** if affecting collision objects | Save-restore. Today: suppressed. |
| **`UISettingsGameObject`** | UI settings | No | Suppress |
| **`SmartGameObject` / `TextGameObject`** | Text/smart object | No | Suppress |
| **`EnhancedTriggerObject`** | Generic enhanced | Catch-all | Suppress |

### 6.b Identifying trigger family at the hook

Inside `TrajEffectHook::triggerObject` / `triggerActivated`, `this` is the `EffectGameObject*`. To branch by family:
```cpp
if (this->m_speedModType != 0) { /* speed mod path */ }
else if (auto* mt = dynamic_cast<MoveGameObject*>(this)) { /* move trigger */ }
// ...
```
But `dynamic_cast` has cost. Faster: check `m_objectID` against known trigger object IDs (e.g. 200=ColorTrigger, 901=MoveTrigger, etc. — these are the in-editor object IDs). The mapping lives in GD's level format documentation; the bindings don't expose it cleanly. Most reliable cheap discriminator inside the hook is a switch on a small set of fields:
- `m_speedModType != 0` → speed mod
- `m_targetGroupID != 0 && m_useMoveTarget` → move trigger (or via `m_moveOffset` non-zero)
- `m_activateGroup` set → toggle trigger
- `m_animationID != 0` → animate
- `m_secretCoinID != 0` → coin pickup trigger
- `m_pulseMode != 0` → pulse
- Otherwise check via subclass-specific casts only for the families that have unique class types (SFX, Song, ForceBlock, RotateGameplay, etc.)

### 6.c Toggle blueprint — single master + per-family

```cpp
// Trajectory.hpp
struct TriggerFamilyFlags {
    bool master_suppressTriggers{true};  // if false, ALL triggers run (very dangerous)
    bool color{true};       // suppress color/gradient/pulse
    bool move{true};        // suppress move triggers (or run with group-pos save-restore)
    bool toggle{true};      // suppress group toggle
    bool rotate{true};      // suppress rotate (group + gameplay)
    bool spawn{true};       // suppress spawn / chance / sequence / random
    bool camera{true};      // suppress camera triggers
    bool animate{true};
    bool sfx{true};
    bool song{true};
    bool particle{true};
    bool shader{true};
    bool item{true};
    bool timer{true};
    bool event{true};
    bool playerControl{true};
    bool objectControl{true};
    bool transform{true};
    bool forceBlock{true};  // SHOULD be false (sim should feel force) — leave for diagnosis
    bool gameplayRotate{true};
    bool other{true};       // catch-all for unclassified
};
TriggerFamilyFlags m_triggerFlags;
TriggerFamilyFlags const& triggerFlags() const { return m_triggerFlags; }
```

Then in `TrajEffectHook::triggerObject`:
```cpp
void triggerObject(GJBaseGameLayer* layer, int uniqueID, gd::vector<int> const* remapKeys) {
    if (!sim().isSimulating()) {
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
        return;
    }
    auto const& f = sim().triggerFlags();
    if (!f.master_suppressTriggers) {
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);  // unsafe, all leak
        return;
    }
    auto family = classifyTrigger(this);  // helper using the field tests above
    bool suppress = true;
    switch (family) {
        case TF::SpeedMod:      suppress = false; /* speed-mod save-restore wrapper */ break;
        case TF::Color:         suppress = f.color; break;
        case TF::Move:          suppress = f.move; break;
        // ...
    }
    if (suppress) return;
    // Family is "run super for sim"
    bool prev1 = m_activatedByPlayer1, prev2 = m_activatedByPlayer2;
    saveRestoreRealPlayerSpeeds([&]{
        EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
    });
    m_activatedByPlayer1 = prev1; m_activatedByPlayer2 = prev2;
}
```

**Toggle behavior — examples.**
- **Color OFF, Move ON, others default**: sim ignores color changes (no effect on physics anyway), but move triggers run → blocks move both for sim and real. Real game sees blocks repositioned mid-sim → leakage. Useful if a level uses move triggers to put a block in sim's path that the bot needs to know about, AND you accept the leak.
- **Toggle ON, others suppressed**: sim sees toggled-group visibility/collision changes. Real also sees them (leak). Useful for diagnosis: "is the bug because sim doesn't see a hidden block?"
- **master OFF**: all triggers fire for sim, real game becomes a chaotic mess. ONLY for proving "trigger suppression is necessary" — never for play.

### 6.d Subclass-specific extra hooks (currently in place)

| Method | File:line | Purpose |
|---|---|---|
| `RingObject::triggerActivated` | `src/Orbs.cpp:23` | Ring activation visual (ring is also an EffectGameObject, but visuals are suppressed even when activation runs) |
| `RingObject::powerOnObject` | `src/Orbs.cpp:28` | Ring power-on visual |
| `RingObject::spawnCircle` | `src/Orbs.cpp:33` | Ring visual circle |
| `GameObject::playShineEffect` | `src/Hooks.cpp:550` | Shine visual on any GameObject |
| `EnhancedGameObject::activatedByPlayer` | `src/Hooks.cpp:557` | Orb activation core — sim path marks engine flags via `markActivated` then skips super (so visuals don't fire); restored at end of run |
| `EnhancedGameObject::hasBeenActivatedByPlayer` | `src/Hooks.cpp:582` | Combines sim-local activation set with engine's real-player flag for correct multi/single-activate semantics |

---

## 7. Audio, visual, particle, and animation suppressions

**Code item.** Various `if (sim().isSimulating()) return;` gates throughout `src/Hooks.cpp` and `src/Orbs.cpp`.

**Manifest today.** When sim crosses something that would normally spawn a sound/particle/flash/screen-shake/camera tween, real game sees nothing.

**Manifest if removed.** Every sim probe (multiple per visual frame) would emit sound, particles, and screen effects on the real game. Unplayable.

### 7.a Visual / particle (all currently suppressed for sim)
| Method | File:line | What it would do (if not suppressed) | Toggle name (proposed) |
|---|---|---|---|
| `PlayLayer::playEndAnimationToPos` | `Hooks.cpp:55` | Triggers end-of-level animation on real screen | `m_suppressEndAnim` |
| `PlayLayer::playPlatformerEndAnimationToPos` | `Hooks.cpp:60` | Platformer end animation | `m_suppressPlatformerEndAnim` |
| `PlayLayer::playGravityEffect` | `Hooks.cpp:69` | Gravity-portal particle burst | `m_suppressGravityEffect` |
| `GJBaseGameLayer::shakeCamera` | `Hooks.cpp:243` | Screen shake CCAction (would continue past sim end) | `m_suppressShakeCamera` |
| `GJBaseGameLayer::moveCameraToPos` | `Hooks.cpp:247` | Camera move | `m_suppressMoveCameraToPos` |
| `GJBaseGameLayer::updateScreenRotation` | `Hooks.cpp:251` | Screen rotation tween | `m_suppressScreenRotation` |
| `GJBaseGameLayer::playFlashEffect` | `Hooks.cpp:259` | Screen flash | `m_suppressFlashEffect` |
| `GJBaseGameLayer::spawnParticle` (returns nullptr) | `Hooks.cpp:271` | Particle on scene tree | `m_suppressSpawnParticle` |
| `GJBaseGameLayer::spawnParticleTrigger` (×2) | `Hooks.cpp:277, 281` | Particle from trigger | `m_suppressParticleTrigger` |
| `GJBaseGameLayer::lightningFlash` (×2) | `Hooks.cpp:286, 290` | Lightning visual | `m_suppressLightning` |
| `GJBaseGameLayer::playSpeedParticle` | `Hooks.cpp:301` | Speed-portal particle | `m_suppressSpeedParticle` |
| `GJBaseGameLayer::animatePortalY` | `Hooks.cpp:322` | Mode-portal Y tween (also affects gameplay area; snapshot covers value, suppression cancels CCAction) | `m_suppressAnimatePortalY` |
| `GJBaseGameLayer::toggleFlipped` (forces noEffects=true for sim) | `Hooks.cpp:231` | Flip-Y tween | `m_suppressFlipAnimation` |
| `GJBaseGameLayer::cameraMoveX/Y` | `Hooks.cpp:337, 341` | Camera X/Y tween | `m_suppressCameraMove` |
| `GJBaseGameLayer::updateCameraOffsetX/Y` | `Hooks.cpp:345, 351` | Camera offset tween | `m_suppressCameraOffset` |
| `GJBaseGameLayer::updateStaticCameraPos` | `Hooks.cpp:357` | Static camera pos | `m_suppressStaticCamera` |
| `GJBaseGameLayer::updateStaticCameraPosToGroup` | `Hooks.cpp:365` | Static camera to group | `m_suppressStaticCameraToGroup` |
| `PlayerObject::incrementJumps` | `Hooks.cpp:389` | Jump counter (persists to stats) | `m_suppressIncrementJumps` |
| `PlayerObject::playSpiderDashEffect` | `Hooks.cpp:394` | Spider dash visual | `m_suppressSpiderDashEffect` |
| `PlayerObject::playBumpEffect` | `Hooks.cpp:399` | Pad bump visual | `m_suppressBumpEffect` |
| `PlayerObject::flashPlayer` | `Hooks.cpp:407` | Mode-portal player flash | `m_suppressFlashPlayer` |
| `HardStreak::addPoint` | `Hooks.cpp:597` | Trail rendering | `m_suppressHardStreakPoint` |

**Toggle blueprint.** Pattern is identical for all: `bool m_xxx{true}; bool xxx() const { return m_xxx; }`, then in the hook `if (sim().isSimulating() && sim().xxx()) return;`. A master `m_suppressAllVisuals{true}` makes this a one-flag emergency switch.

**Toggle behavior.**
- ON (current, default): clean real-game screen during sim. Recommended for all play.
- OFF: real game shows sim's visuals. Particles, flashes, screen shakes on every sim probe. Unwatchable but proves the suppression is real.

### 7.b Audio — NOT HOOKED ANYWHERE
**Code item.** None. The mod has zero `playSfx` / FMOD / sound hooks.

**Manifest.** If any sim-side super call invokes audio playback (e.g., a SFX trigger fire path inside an unblocked super call), the user hears it.

**Today's mitigation.** Indirect — `TrajEffectHook` suppresses non-speed-mod triggers (which is where SFX/Song triggers route through). But `EnhancedGameObject::activatedByPlayer` super is skipped for sim (orb activation), and the engine might or might not play orb-touch SFX from somewhere else — that path isn't audited.

**Recommended hooks to add.**
- `FMODAudioEngine::playEffect` / `playMusic` / `playSfx` — gate on `sim().isSimulating()` to suppress.
- Per-class `activatedAudioTrigger` (`GJBaseGameLayer::activatedAudioTrigger`) — gate.
- `activateSongTrigger`, `activateSFXTrigger` — gate.

**Toggle blueprint.**
```cpp
class $modify(TrajAudioHook, GJBaseGameLayer) {
    void activatedAudioTrigger(SFXTriggerGameObject* obj) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        GJBaseGameLayer::activatedAudioTrigger(obj);
    }
    // ... activateSongTrigger, activateSFXTrigger, etc.
};
```

**Toggle behavior.**
- `m_suppressAudio` ON (would be default): no sim-driven sounds. Same as today (indirect) but explicit.
- OFF: sim probes audible. Each candidate × frame × audio-trigger = potentially many SFX/sec.

---

## 8. Hook gating — sim/real branched (different code per side)

**Code item.** Methods where sim and real both run but down different paths. Not suppression — divergent logic.

| Method | File:line | Sim path | Real path |
|---|---|---|---|
| `PlayLayer::destroyPlayer` | `Hooks.cpp:50` | Mark sim dead (`markSimDeadIfSimPlayer`), skip super UNLESS anticheat spike | Super |
| `GJBaseGameLayer::flipGravity` | `Hooks.cpp:193` | Sim player: super with forced noEffects=true. Real player during sim: skip. | Real path: super with real noEffects |
| `GJBaseGameLayer::canBeActivatedByPlayer` | `Hooks.cpp:203` | Sim player: true. Real player during sim: false. | Super |
| `PlayerObject::updateTimeMod` | `Hooks.cpp:421` | Sim player: super. Real player during sim: skip (the inline-write bypass) | Super |
| `GJBaseGameLayer::updateTimeMod` (layer-level) | `Hooks.cpp:445` | Save real speeds → super → propagate post-super speeds to sim players → restore real speeds | Super |
| `EffectGameObject::triggerObject` / `triggerActivated` (speed-mod) | `Hooks.cpp:517, 533` | Save `m_activatedByPlayer1/2` → save-restore-speeds wrapper around super → restore activation flags | Super |
| `EnhancedGameObject::activatedByPlayer` | `Hooks.cpp:557` | Spoof engine flags (m_activated + m_activatedByPlayer1/2), skip super (no visuals/audio) | Super |
| `EnhancedGameObject::hasBeenActivatedByPlayer` | `Hooks.cpp:582` | Combine sim-local set + engine's real-player flag check | Super |
| `GJBaseGameLayer::playerTouchedRing` | `Orbs.cpp:15` | Sim player during sim: super (sim physics needs ring touch). Real player during sim: skip. | Super outside sim |
| `GJBaseGameLayer::destroyObject` | `Hooks.cpp:118` | Mark sim-destroyed, skip super | Super |
| `GJBaseGameLayer::toggleDualMode` | `Portals.cpp:18` | Sim player: skip (snapshot handles m_isDualMode restore) | Super |
| `GJBaseGameLayer::checkCameraLimitAfterTeleport` | `Portals.cpp:23` | Sim player: skip (camera snapshotted) | Super |
| `BotPlayerObjectHook::update` button injection | `BotHooks.cpp:104` | Sim's button set inside runPlan from plan vector (not via this hook) | Real's button set PRE-super from current cached plan |

**Toggleability.** Each row could have a "force sim down real path" toggle, but the resulting behavior is undefined/broken (the divergent path exists because the real path would mutate something we can't roll back). Not recommended to expose. The audit value is knowing these exist.

---

## 9. Save-restore wrappers around sim's super calls

**Code item.** Where sim NEEDS super to run (for sim physics correctness) but super has known side effects on real player flags. Save→super→restore.

| Wrapper | File:line | Saved/restored fields |
|---|---|---|
| `TrajLayerSpeedHook::updateTimeMod` | `Hooks.cpp:444-477` | Real `m_player1->m_playerSpeed` + `m_playerSpeedAC`, real `m_player2` same. Plus: post-super propagation of new speed to sim players. |
| `TrajEffectHook::saveRestoreRealPlayerSpeeds` (helper) | `Hooks.cpp:508-515` | Real `m_player1->m_playerSpeed` + `m_player2->m_playerSpeed` (no AC) |
| `TrajEffectHook::triggerObject` (speed-mod branch) | `Hooks.cpp:517-531` | Object's `m_activatedByPlayer1` + `m_activatedByPlayer2` |
| `TrajEffectHook::triggerActivated` (speed-mod branch) | `Hooks.cpp:533-546` | Object's `m_activatedByPlayer1` + `m_activatedByPlayer2` |

**Asymmetry note.** `TrajLayerSpeedHook` saves both `m_playerSpeed` AND `m_playerSpeedAC`. `saveRestoreRealPlayerSpeeds` saves only `m_playerSpeed`. The two reach the same engine path but the AC field can drift through the second wrapper.

**Toggle.** A per-wrapper "bypass save-restore" toggle is straightforward to add but its primary use is diagnostic ("does X mutate field Y? bypass and check post-sim Y") — not for play. Not recommended for the settings UI.

---

## 10. Sim creation / lifecycle

**Code item.** `src/Trajectory.cpp:236-258`.

```cpp
PlayerObject* p = PlayerObject::create(1, 1, pl, pl, true);
p->setPosition({0.f, 105.f});
p->setVisible(false);
p->setID("trajectory-sim-player"_spr);
pl->m_objectLayer->addChild(p);
```

**Manifest.** Two sim players (P1, P2) are created at PlayLayer setup. They live for the level's lifetime, are seeded fresh from base at the start of every runPlan/runBranch.

**Known gaps.**
- **Lazy per-mode sub-sprites**: PlayerObject creates ship/ball/wave/UFO/robot/spider/swing child sprites lazily on first mode toggle. If sim hasn't toggled into mode X yet, sim's first frame in mode X has incomplete sub-state. The `docs/issue-ball-portal-first-attempt-wall.md` repro is the case where sim's first ball-portal crossing predicts wrong, and self-heals on the second attempt.
- **Sim not registered in PlayLayer's player set**: Any engine code that checks `== m_player1` or `== m_player2` (rather than calling the sim through the same code path as a player) will skip sim.
- **Icon IDs hardcoded to (1,1)**: physics-irrelevant per visual evidence, but worth noting.

**Toggle: warm modes at creation.**
```cpp
// Trajectory.cpp createSimPlayer
if (sim().warmModes()) {
    p->toggleShipMode(true, true); p->toggleShipMode(false, true);
    p->toggleBirdMode(true, true); p->toggleBirdMode(false, true);
    // ... ball, wave, ufo, robot, spider, swing
}
```
**Toggle behavior.**
- `m_warmModes` ON: pre-builds all mode sub-sprites at creation. First crossing of any mode portal hits initialized state.
- OFF (current): first crossing initializes. Risks the ball-portal-first-attempt symptom.

User-side empirical: warming modes was tried and did NOT change the bug repro frame-for-frame, suggesting the bug isn't lazy-init. The toggle still has diagnostic value.

**Toggle: per-reset recreation.**
```cpp
// In onPlayLayerReset, optionally:
if (sim().recreateOnReset()) {
    m_simP1->removeFromParent(); m_simP2->removeFromParent();
    m_simP1 = createSimPlayer(m_pl); m_simP2 = createSimPlayer(m_pl);
    adoptOwnRings(m_simP1, m_simP1OwnRings); adoptOwnRings(m_simP2, m_simP2OwnRings);
}
```
**Toggle behavior.**
- ON: every attempt starts with fresh sim PlayerObjects. Slow but clean.
- OFF (current): sim PlayerObjects persist across attempts. Faster but state-coupled.

---

## 11. Driver-level asymmetries

**Code item.** Where the input/timing pipelines for real and sim differ structurally.

### 11.a Input flip ordering
- Real (bot-driven): `BotPlayerObjectHook::update` flips button PRE-super of `PlayerObject::update`, AFTER the engine's per-tick `checkCollisions` has already run with the OLD button state.
- Sim (`runPlan`): flips button AFTER `m_pl->checkCollisions(sim, ...)`, BEFORE `sim->update(...)`.

These should be equivalent (both put the button into effect just before the tick's `update`). The alignment was verified to kill the `+0.216 yVel/tick` divergence signature; see `db565f8`.

**Toggle.** Not exposed. Changing the order on either side reintroduces the divergence.

### 11.b Visual vs physics frame rate
- Bot search runs at 60Hz from `updateCamera`.
- Bot input injection runs at 240Hz from `PlayerObject::update`.
- Sim's runPlan runs at 240Hz (each `sim->update(m_frameDt)` is one physics tick).

The `feedback_staircase_desync_hypothesis` memory tracks an open theory about 60Hz-search vs 240Hz-inject phase alignment causing staircase failures.

**Toggle.** Bot search interval (`m_searchInterval`) already exists — values > 1 mean search every N visual frames. Off-frames currently get no work (a `runProbe` experiment was reverted as the recent revert).

### 11.c `handleButton` recording
`Hooks.cpp:94-97` records real button into `TrajectorySimulator::m_player1Pressed` / `m_player2Pressed`. These fields exist but are NEVER READ anywhere. Dead state — removable.

---

## 12. Proposed unified setting surface

If we wired every per-discrepancy toggle into the mod settings, the panel grows large. A tiered approach:

### Tier 1 — currently exposed (keep)
- `bot-enabled`
- `show-trajectory` / `show-bot-trajectory`
- `pads` / `orbs` / `portals` (collision-filter type families)
- `trajectory-iterations` / `bot-search-interval`
- `debug-visuals` / `bot-divergence-threshold`

### Tier 2 — proposed master toggles (add to settings UI)
- `m_suppressVisuals` (master for all §7.a visual suppressions)
- `m_suppressAudio` (new — §7.b)
- `m_suppressTriggers` (master for §6 trigger suppression)
- `m_destroyIsolation` (§1.b)
- `m_coinIsolation` (§1.c, currently hardcoded)

### Tier 3 — debug-only (advanced settings, hidden by default)
- Per-family trigger flags (§6.c) — `m_triggerFlags.color`, `.move`, `.rotate`, ...
- Per-family snapshot flags (§5.a)
- Per-field clearSimRingState flags (§3.a)
- Per-method visual suppression flags (§7.a)
- `m_warmModes` (§10)
- `m_recreateOnReset` (§10)

### Tier 4 — diagnostic-only (no UI, manual code edit)
- `m_spatialCull` (§1.a)
- `m_explicitFieldCopies` (§2.b)
- Save-restore bypass switches (§9)

**Implementation note.** Tier 2 is small enough to add immediately and gives the user a "flip everything on/off in this domain" control. Tier 3 should go behind a "Show advanced toggles" checkbox to avoid cluttering the panel for normal use. Tier 4 stays in-source — adding 20 booleans for things only used during one specific bug investigation isn't worth the surface area.

---

## 13. Quick reference — files that gate sim behavior

| File | Role |
|---|---|
| `src/Hooks.cpp` | Main hook surface: PlayLayer / GJBaseGameLayer / PlayerObject / EffectGameObject / EnhancedGameObject / GameObject / HardStreak |
| `src/Orbs.cpp` | Ring-specific hooks (RingObject overrides + `playerTouchedRing` gate) |
| `src/Portals.cpp` | Portal-specific hooks (dual mode + camera limit) — FROZEN |
| `src/Trajectory.cpp` | `LayerStateSnapshot`, `clearSimRingState`, `clearPerTickRingOverlap`, `markActivated`, `clearActivated`, `markSimDestroyed`, `runBranch`, `runPlan` |
| `src/Trajectory.hpp` | Toggle members + accessors live here |
| `src/main.cpp` | `$execute` wires every mod setting → simulator setter |
| `src/Pads.hpp` / `Orbs.hpp` / `Portals.hpp` | `isPad/isOrb/isPortal` type classifiers used by §1 filter |
| `mod.json` | Setting declarations |

**Discrepancy isn't here?** Two possibilities: (a) it's a side effect of an engine path we haven't audited (more bindings work), or (b) it's an emergent property of an already-listed mechanism (e.g., "sim sees stale checkpoint" is a consequence of §10 lifecycle + §5 snapshot). For (a), the next pass should grep bindings for any method on `GJBaseGameLayer`/`PlayerObject`/`PlayLayer` that touches `m_player1`/`m_player2` and isn't already in §8 or §9.
