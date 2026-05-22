# Leak candidates — ranked

Synthesized from all 9 source audits. Each row is a discrepancy where sim COULD mutate state real player/game sees, AND we don't currently isolate it. Ranked by severity (`critical` → `low`).

**Severity meanings:**
- `critical` — real-game state corruption visible to user (audio, persistent stat, visible-tween, save file)
- `high` — sim's predicted trajectory diverges from real; bot picks wrong path
- `medium` — edge-case divergence; specific level structure needed to trigger
- `low` — theoretical, no known symptom yet

**Origin tag** points at the source audit doc for full context.

---

## Critical — real-game state corruption

### C1. Audio surface entirely unguarded
**Origin:** [08-audio-fmod.md](08-audio-fmod.md). 143 audio entry points enumerated; ~30 direct unguarded call sites.
**Symptom:** every sim probe (multiple per visual frame) produces sound — death effects, mode-switch toggles, gravity flips, ring-jumps, ground hits, bump effects, teleports. Continuous noise during bot operation.
**Today's partial mitigation:** `TrajEffectHook::triggerObject` transitively blocks SFX/Song trigger fires, but only those that go through `triggerObject`. Any audio path on `PlayerObject` (`playDeathEffect`, `playBumpEffect`, mode toggles, etc.) or `GJBaseGameLayer` (`activatedAudioTrigger`, `processQueuedAudioTriggers`) is wide open.
**Recommended fix:** engine-level FMOD hook. `FMODAudioEngine::playEffect`/`playEffectAdvanced`/`playEffectAsync`/`playMusic`/`queuePlayEffect`/`queueStartMusic`/`update` (drain). Single setting `sim-suppress-audio` (default on) gates them all.
**Why not gate per-method?** PlayerObject's `toggleSpiderMode` / `flipGravity` / `bumpPlayer` etc. do real physics work for sim — we WANT them to run. Audio is incidental side effect inside. Engine-level kill is the right cut-point.

### C2. `m_effectManager` not captured by snapshot
**Origin:** [03-gjbasegamelayer.md](03-gjbasegamelayer.md).
**Symptom:** the entire color/pulse/timer/spawn trigger state container — if anything mutates this during sim that the `TrajEffectHook` gate doesn't catch, the next real tick reads sim-mutated state. Levels with color/pulse triggers would visibly drift.
**Today's mitigation:** indirect — `TrajEffectHook::triggerObject` blocks all non-speed-mod triggers, which prevents the standard write path. NOT defense-in-depth.
**Recommended fix:** add `m_effectManager` to `LayerStateSnapshot`. It's a pointer (`EffectManager*`), but the SCOPE of changes happens to fields owned BY the EffectManager — full deep-copy/restore is non-trivial. Alternative: snapshot the high-traffic substructures (`m_unorderedMapInt_OpacityEffectAction`, `m_vectorToggleTriggerAction`, etc.). Best alternative: add defense-in-depth hooks on `GJBaseGameLayer::updateColor`, `toggleGroupTriggered`, `spawnGroup`, `spawnObject` virtuals.

### C3. `updateColor` virtual not hooked
**Origin:** [03-gjbasegamelayer.md](03-gjbasegamelayer.md).
**Symptom:** writes directly to `m_effectManager` color state. Bypasses our trigger gate.
**Recommended fix:**
```cpp
class $modify(TrajColorHook, GJBaseGameLayer) {
    void updateColor(cocos2d::ccColor3B& color, float fadeTime, int colorID, bool blending,
                     float opacity, cocos2d::ccHSVValue& copyHSV, int colorIDToCopy,
                     bool copyOpacity, EffectGameObject* callerObject, int unk1, int unk2) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::updateColor(color, fadeTime, colorID, blending, opacity,
                                     copyHSV, colorIDToCopy, copyOpacity, callerObject, unk1, unk2);
    }
};
```

### C4. `m_collisionLog{Top,Bottom,Left,Right}` on PlayerObject — shared CCDictionary leak
**Origin:** [01-playerobject.md](01-playerobject.md).
**Symptom:** if `copyAttributes(base)` shallow-copies these four pointers from base to sim (typical CCObject* behavior), sim's `storeCollision` writes mutate REAL player's collision-dedupe dictionaries. Real then phantom-skips collisions it should see. Hard to attribute because the symptom is delayed and silent.
**Today's mitigation:** none — `resetCollisionLog(true)` is called on sim each tick (`src/Trajectory.cpp:607,727,734`) but that ALSO clears real's log if pointer is shared.
**Recommended fix:** at `createSimPlayer` time, replace sim's `m_collisionLog{Top,Bottom,Left,Right}` with private CCDictionary instances (same pattern as `m_touchingRings` in `adoptOwnRings`). Verify post-`copyAttributes` that the pointer wasn't re-aliased.

### C5. Checkpoint surface (practice-mode leak)
**Origin:** [02-playlayer.md](02-playlayer.md).
**Symptom:** in practice mode, sim crossing a `CheckpointGameObject` could trigger `markCheckpoint` / `storeCheckpoint` / `queueCheckpoint` on real PlayLayer — real player's practice checkpoint gets placed by sim probe.
**Today's mitigation:** indirect — `TrajEffectHook::triggerObject` blocks the trigger fire. But `CheckpointGameObject::triggerObject` and the `checkpointActivated` virtual on `GJBaseGameLayer` have separate code paths.
**Recommended fix:**
```cpp
class $modify(TrajCheckpointHook, GJBaseGameLayer) {
    void checkpointActivated(CheckpointGameObject* obj) {
        if (sim().isSimulating()) return;
        GJBaseGameLayer::checkpointActivated(obj);
    }
};
```
Also gate `PlayLayer::markCheckpoint`, `PlayLayer::storeCheckpoint`, `PlayLayer::createCheckpoint`.

### C6. Persistent-stat writes (`m_uncommittedJumps`, `commitJumps`, `incrementJumps`)
**Origin:** [02-playlayer.md](02-playlayer.md), [01-playerobject.md](01-playerobject.md).
**Symptom:** sim path that hits `incrementJumps` or `commitJumps` corrupts the player's save file on next flush.
**Today's mitigation:** `TrajPlayerObjectHook::incrementJumps` (`src/Hooks.cpp:389`) — gated. But `PlayLayer::commitJumps` is NOT hooked.
**Recommended fix:**
```cpp
class $modify(TrajPlayLayerStatHook, PlayLayer) {
    void commitJumps() {
        if (sim().isSimulating()) return;
        PlayLayer::commitJumps();
    }
};
```

### C7. Level-complete paths (`levelComplete`, `showEndLayer`, `showNewBest`, `activateEndTrigger`, `activatePlatformerEndTrigger`)
**Origin:** [02-playlayer.md](02-playlayer.md), [07-trigger-subclasses.md](07-trigger-subclasses.md).
**Symptom:** sim crossing an `EndTriggerGameObject` could cascade into `activateEndTrigger` → `levelComplete` → real end-screen + progress write.
**Today's mitigation:** `EndTriggerGameObject` is suppressed only because of `!isSpeedMod()` in `TrajEffectHook`. Loosening that gate (e.g., letting another trigger family fire) would unguard end-trigger.
**Recommended fix:** explicit per-class hard-gate on `EndTriggerGameObject::triggerObject` so it can never fire for sim regardless of master toggle. AND gate `PlayLayer::levelComplete`, `showEndLayer`, `showNewBest`.

### C8. `toggleGroupTriggered`, `spawnGroup`, `spawnObject` virtuals not hooked
**Origin:** [03-gjbasegamelayer.md](03-gjbasegamelayer.md).
**Symptom:** these are the top-level trigger-fanout virtuals on `GJBaseGameLayer`. Currently blocked at `EffectGameObject::triggerObject` source, but no defense-in-depth at the layer-side dispatch.
**Recommended fix:** add layer-side gates so even a bypassed trigger source can't fan out to group state changes:
```cpp
void toggleGroupTriggered(int group, bool activate, gd::vector<int> const& remapKeys,
                          int triggerID, int controlID) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::toggleGroupTriggered(group, activate, remapKeys, triggerID, controlID);
}
void spawnGroup(int group, bool ordered, double delay, gd::vector<int> const& remapKeys,
                int triggerID, int controlID) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::spawnGroup(group, ordered, delay, remapKeys, triggerID, controlID);
}
void spawnObject(GameObject* object, double delay, gd::vector<int> const& remapKeys) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::spawnObject(object, delay, remapKeys);
}
```

### C9. Endless-tween CCAction leaks: `runRotateAction`, `runBallRotation*`, `runNormalRotation`
**Origin:** [09-cocos2d-tweening.md](09-cocos2d-tweening.md).
**Symptom:** these run `CCRepeatForever` actions on the PlayerObject sprite. If sim's super spawns one on the real player (sim's calls go through PlayerObject methods on its OWN instance, BUT some methods accept other-player as argument), real player's sprite spins endlessly.
**Today's mitigation:** none. The visuals our hooks suppress are individual one-shot tweens — these are looped.
**Recommended fix:** hook each on `PlayerObject` and gate when not a sim player.

### C10. PlayLayer's `updateScreenRotation(int)` overload not hooked (only the `float` version is)
**Origin:** [02-playlayer.md](02-playlayer.md), `src/Hooks.cpp:251`.
**Symptom:** if the engine ever invokes the `PlayLayer` int-overload during sim, screen rotation tween leaks to real game.
**Recommended fix:** add hook for PlayLayer's specific overload alongside the existing GJBaseGameLayer hook.

---

## High-impact — sim trajectory divergence

### H1. GJGameState post-POD scalars not captured
**Origin:** [04-gjgamestate.md](04-gjgamestate.md). 6 high-impact fields:
- `m_totalTime` (double) — advanced each tick
- `m_levelTime` (double) — advanced each tick
- `m_commandIndex` (uint) — cursor into level command queue
- `m_currentProgress` (uint) — progress counter
- `m_unkUint3` (typed float) — neighbor-correlation suspect
- `m_unkUint64_1` (double) — likely a tick counter

**Symptom:** sim advances these during runPlan. Without restore, the real game's next tick reads sim-advanced values. Strong suspect for the `project_bookmarked_bugs.md` speed-portal sim-side regression — if sim's `m_levelTime` advances past a portal, real game's per-tick speed-mod cache lookup skews.

**Recommended fix (3 lines each in `src/Trajectory.cpp` `LayerStateSnapshot`):**
```cpp
// in struct
double m_totalTime{0.0};
double m_levelTime{0.0};
unsigned int m_commandIndex{0};
unsigned int m_currentProgress{0};
float m_unkUint3{0.f};
double m_unkUint64_1{0.0};

// in capture()
m_totalTime = gs.m_totalTime;
m_levelTime = gs.m_levelTime;
// ... etc

// in restore()
gs.m_totalTime = m_totalTime;
// ... etc
```

**Test plan:** apply, run levels with speed-mod portals, confirm sim's predicted trajectory respects post-portal speed (the bookmarked symptom is sim ignoring speed changes).

### H2. `m_currentSlope` not in explicit-copy list (only `m_currentSlope2` is)
**Origin:** [01-playerobject.md](01-playerobject.md).
**Symptom:** if `copyAttributes` misses `m_currentSlope`, sim's slope physics resolves against null/stale slope reference. Subtle slope-edge divergence.
**Recommended fix:** add `sim->m_currentSlope = base->m_currentSlope;` to both `runBranch` (`src/Trajectory.cpp:545-586`) and `runPlan::initSim` (`src/Trajectory.cpp:631-670`).

### H3. Jump/flip timestamp fields untracked
**Origin:** [01-playerobject.md](01-playerobject.md), [05-playercheckpoint.md](05-playercheckpoint.md).
**Fields:**
- `m_lastJumpTime`
- `m_lastFlipTime`
- `m_lastSpiderFlipTime`
- `m_hasEverJumped`
- `m_fixRobotJump`
- `m_jumpRelatedAC2`
- `m_somethingPlayerSpeedTime`

**Symptom:** all of these are timing fields the engine consults for jump/flip throttle / anticheat / mode-specific physics. None are in `PlayerCheckpoint`. The robot mid-air "fresh full jump" bug (`project_bookmarked_bugs.md`) is consistent with one of these (especially `m_fixRobotJump` or `m_jumpRelatedAC2`) being stale on sim.

**Recommended fix:** add to explicit-copy list (already tried `m_lastJumpTime` etc. in the recent reverted commit — bookmark notes that didn't fix the robot bug, but should still be in the copy list for correctness; the bug may have a different root cause).

### H4. `m_speedMultiplier` distinct from `m_playerSpeed`, untouched anywhere
**Origin:** [01-playerobject.md](01-playerobject.md).
**Symptom:** `TrajLayerSpeedHook` carefully handles `m_playerSpeed`/`m_playerSpeedAC`, but `m_speedMultiplier` (line 14547) is a distinct field that no current code touches. May be the field speed-mod portals actually write through.
**Recommended fix:** investigate via instrumented log: log all three speed fields before/after a speed portal crossing for both real and sim. Confirm which field is the source of truth.

### H5. `m_maxGameplayY` not captured, `updateMaxGameplayY` not hooked
**Origin:** [03-gjbasegamelayer.md](03-gjbasegamelayer.md).
**Symptom:** ball-portal ceiling-update leak. Sim's ball-mode physics computes a ceiling Y; if sim writes this, real game's ceiling moves.
**Recommended fix:** add `m_maxGameplayY` to `LayerStateSnapshot`. Hook `updateMaxGameplayY` if the field is set through that virtual rather than direct write.

### H6. `m_vehicleSize` + `m_groundObjectMaterial` must stay in explicit copy
**Origin:** [05-playercheckpoint.md](05-playercheckpoint.md).
**Symptom:** if we switch to the checkpoint round-trip path and drop the explicit-copy list, these two fields STOP being copied — they're not in `PlayerCheckpoint`. Vehicle size affects hitbox dimensions; ground material affects ice vs normal physics.
**Recommended fix:** keep the explicit copy lines for these two even if everything else moves to checkpoint:
```cpp
sim->loadFromCheckpoint(checkpoint);  // covers 28 of 30 explicit fields
sim->m_vehicleSize          = base->m_vehicleSize;
sim->m_groundObjectMaterial = base->m_groundObjectMaterial;
```

### H7. `m_recordString` (replay buffer) not captured
**Origin:** [03-gjbasegamelayer.md](03-gjbasegamelayer.md).
**Symptom:** if sim's `handleButton` ever reaches the recording path (our `TrajBaseLayerHook::handleButton` is `recordRealButton`-only, so probably safe), sim's button events get logged into the replay buffer.
**Today's mitigation:** sim's button injection uses `pushButton/releaseButton` directly on the sim PlayerObject, bypassing `handleButton`. Low risk in practice.
**Recommended fix:** snapshot `m_recordString` length at sim start and truncate at restore if longer. Or hook `recordAction` directly.

### H8. Camera dimensions outside `m_gameState` POD prefix
**Origin:** [03-gjbasegamelayer.md](03-gjbasegamelayer.md).
**Fields:** `m_cameraFlip`, `m_cameraWidthOffset`, `m_cameraHeightOffset` (GJBaseGameLayer lines 7984-7986).
**Symptom:** if anything during sim writes these, real camera dimensions shift.
**Recommended fix:** add to `LayerStateSnapshot`. They're scalars; same 3-line pattern as H1.

### H9. `m_zoomValue` / `updateZoom` not hooked, `m_objectLayer` no `stopAllActions`
**Origin:** [03-gjbasegamelayer.md](03-gjbasegamelayer.md), [09-cocos2d-tweening.md](09-cocos2d-tweening.md).
**Symptom:** sim crossing a zoom trigger spawns CCAction on `m_objectLayer`. Our snapshot restore stops actions on ground/middleground but not the object layer.
**Recommended fix:** add `m_objectLayer->stopAllActions()` to `LayerStateSnapshot::restore`. Also hook `updateZoom` for source-side suppression.

---

## Medium-impact — edge cases

### M1. GJGameState medium-impact post-POD fields (10 total)
**Origin:** [04-gjgamestate.md](04-gjgamestate.md). Containers and rarely-mutated fields. Lower priority than H1 scalars.

### M2. `ForceBlockGameObject` not handled
**Origin:** [07-trigger-subclasses.md](07-trigger-subclasses.md).
**Symptom:** doesn't fire through `triggerObject` — fires via `PlayerObject::collidedWithObject`. If sim hits one, the force is applied to sim's player. Probably WANTED for sim accuracy. But if the force-block's super writes to anything shared, leak.
**Recommended fix:** investigate the engine's actual force-application path. Probably no fix needed.

### M3. `RotateGameplayGameObject` physics-affecting
**Origin:** [07-trigger-subclasses.md](07-trigger-subclasses.md).
**Symptom:** rotates gameplay direction/gravity for player. Currently suppressed entirely; on levels using this, sim's trajectory is WRONG (sim doesn't see the rotation).
**Recommended fix:** save-restore pattern around super (snapshot PlayerObject velocity/gravity fields).

### M4. 12+ state-suspect PlayerObject fields not in PlayerCheckpoint
**Origin:** [05-playercheckpoint.md](05-playercheckpoint.md). Already covered partially under H3. Other entries: `m_stateRingJump`, `m_isDead`, `m_holdingButtons`, `m_inputsLocked`, `m_isPlatformer`, `m_isOutOfBounds`, `m_lastCheckpointTime`, `m_hasEverHitRing`, `m_playerSpeedAC`.

### M5. GameObject family non-virtual trigger helpers bypass `triggerObject` gate
**Origin:** [06-gameobject-base.md](06-gameobject-base.md). `playTriggerEffect`, `triggerEffectFinished`, `resetSpawnTrigger`, `updateSpecialColor`, `stateSensitiveOff` on EffectGameObject are NOT routed through `triggerObject`. If anything calls them directly, sim mutates trigger runtime state.

### M6. GameObject color/state mutators bypass trigger gate
**Origin:** [06-gameobject-base.md](06-gameobject-base.md). `setObjectColor`, `setGlowColor`, `setChildColor`, `updateMainColor`, `updateSecondaryColor`, `saveActiveColors`, `groupWasDisabled`, `groupWasEnabled`, `resetGroupDisabled`, `resetMoveOffset`, `resetColorGroups`. These mutate object/group state directly.

### M7. EnhancedGameObject `resetObject`, `powerOnObject`, `powerOffObject`, `stateSensitiveOff`, `animationTriggered`, `updateState`
**Origin:** [06-gameobject-base.md](06-gameobject-base.md). Power/state/animation mutators on shared animated objects.

### M8. Particle/animation spawns from GameObject family
**Origin:** [06-gameobject-base.md](06-gameobject-base.md). `playDestroyObjectAnim`, `playPickupAnimation` (×2 overloads), `spawnDefaultPickupParticle`, `createAndAddParticle`. Same risk class as `playShineEffect` (which IS hooked).

### M9. PlayerObject mode-switch portal-circle spawns
**Origin:** [09-cocos2d-tweening.md](09-cocos2d-tweening.md). `PlayerObject::spawnCircle`, `spawnDualCircle`, `spawnPortalCircle`, `spawnScaleCircle` spawn one-shot circle visuals. `RingObject::spawnCircle` IS gated but the PlayerObject variants aren't.

### M10. Area-effect tweens
**Origin:** [09-cocos2d-tweening.md](09-cocos2d-tweening.md). `triggerAreaEffect`, `triggerAreaEffectAnimation` cover `EnterEffectInstance::animateValue` tweens.

### M11. Shader command tweens
**Origin:** [09-cocos2d-tweening.md](09-cocos2d-tweening.md). `GJBaseGameLayer::triggerShaderCommand` covers 16+ ShaderLayer tween triggers in a single hook point.

### M12. PlayerObject ghost/streak tweens
**Origin:** [09-cocos2d-tweening.md](09-cocos2d-tweening.md). `PlayerObject::toggleGhostEffect`, `fadeOutStreak2`, `animatePlatformerJump`, `exitPlatformerAnimateJump`.

---

## Low — theoretical, no observed symptom

### L1. GJGameState low-impact post-POD (13 fields)
**Origin:** [04-gjgamestate.md](04-gjgamestate.md). Write-once-during-setup or rarely-read.

### L2. `m_isSecondPlayer` on sim P2
**Origin:** [01-playerobject.md](01-playerobject.md).
**Symptom:** if `copyAttributes` doesn't preserve, sim P2 runs P1 code paths. Likely OK since it's bool and `copyAttributes` is empirically fairly thorough for primitive fields — but unverified.

### L3. `m_doNot` field (suspicious name) on PlayLayer line 14954
**Origin:** [02-playlayer.md](02-playlayer.md). Literal field name. Purpose unclear. Worth investigating but no observed leak.

### L4. `m_keyColors` / `m_keyOpacities` / `m_keyPulses` / `m_blendingColors` / `m_colorKeyDict` color palette
**Origin:** [02-playlayer.md](02-playlayer.md). Not in snapshot. Color triggers gated upstream via TrajEffectHook — but no defense-in-depth.

---

## By-category summary

| Category | Critical | High | Medium | Low | Total |
|---|---|---|---|---|---|
| Audio | 1 | 0 | 0 | 0 | 1 |
| Player state (copy / checkpoint) | 1 (C4) | 5 (H2-H4, H6) | 1 (M4) | 1 (L2) | 8 |
| Layer-state snapshot gaps | 1 (C2) | 4 (H1, H5, H8, H9) | 1 (M1) | 1 (L1) | 7 |
| Trigger dispatch | 3 (C3, C5, C8) | 0 | 4 (M2, M3, M5, M6) | 1 (L4) | 8 |
| Persistent-stat / save-file | 1 (C6) | 0 | 0 | 0 | 1 |
| End/complete paths | 1 (C7) | 0 | 0 | 0 | 1 |
| CCAction tween leak | 1 (C9) | 0 | 3 (M9, M10, M12) | 0 | 4 |
| Replay/recording | 0 | 1 (H7) | 0 | 0 | 1 |
| GameObject family setters | 0 | 0 | 3 (M5, M6, M7) | 0 | 3 |
| Particle spawn from objects | 0 | 0 | 1 (M8) | 0 | 1 |
| Misc | 1 (C10) | 0 | 1 (M11) | 1 (L3) | 3 |
| **Total** | **10** | **10** | **14** | **4** | **38** |

---

## Recommended action plan (priority order)

### Phase 1 — defense-in-depth (high signal, low risk)
1. **C3** — hook `updateColor` (one method, one-line gate)
2. **C8** — hook `toggleGroupTriggered`, `spawnGroup`, `spawnObject` (three layer virtuals)
3. **C5** — hook `checkpointActivated`
4. **C6** — hook `PlayLayer::commitJumps`
5. **C7** — hook `PlayLayer::levelComplete`, `showEndLayer`, `showNewBest`
6. **C10** — hook `PlayLayer::updateScreenRotation(int)` overload

Each of these is a one-line `if (sim().isSimulating()) return;` hook. Time investment: ~30 min total. Risk: zero (current state isn't using these paths anyway; we just close the door).

### Phase 2 — speed-portal bookmark fix attempt (might unblock the bookmarked bug)
7. **H1** — add 6 high-impact GJGameState post-POD scalars to LayerStateSnapshot.

Test against the bookmarked speed-portal bug repro after this change.

### Phase 3 — audio (cosmetic, user-facing)
8. **C1** — hook `FMODAudioEngine` engine-level methods. Add `sim-suppress-audio` setting (default on).

### Phase 4 — robot mid-air bookmark fix attempt (might unblock the bookmarked bug)
9. **H3** — confirm `m_fixRobotJump`, `m_jumpRelatedAC2`, etc. are still in copy list. They were in the recent reverted commit; the bug bookmark says copying them didn't help, but they should be in the copy list for correctness independent of the bug.

10. **H4** — instrument `m_speedMultiplier` to see if speed portals write through it.

### Phase 5 — broader player-state safety
11. **C4** — replace sim's `m_collisionLog{Top,Bottom,Left,Right}` with private CCDictionary instances at `createSimPlayer`.
12. **H2** — add `m_currentSlope` to explicit copy.
13. **H5, H8, H9** — additional layer-state snapshot expansions.

### Phase 6 — long tail
14. Medium-impact items as bugs surface.
15. Low-impact items only if specifically pointed at.

---

## How this list was produced

Synthesized from 9 audit agents run 2026-05-10:
- Agent 1 → [01-playerobject.md](01-playerobject.md)
- Agent 2 → [02-playlayer.md](02-playlayer.md)
- Agent 3 → [03-gjbasegamelayer.md](03-gjbasegamelayer.md)
- Agent 4 → [04-gjgamestate.md](04-gjgamestate.md)
- Agent 5 → [05-playercheckpoint.md](05-playercheckpoint.md)
- Agent 6 → [06-gameobject-base.md](06-gameobject-base.md)
- Agent 7 → [07-trigger-subclasses.md](07-trigger-subclasses.md)
- Agent 8 → [08-audio-fmod.md](08-audio-fmod.md)
- Agent 9 → [09-cocos2d-tweening.md](09-cocos2d-tweening.md)

Total source coverage: ~1300 methods + ~1300 fields across PlayerObject / PlayLayer / GJBaseGameLayer / GJGameState / PlayerCheckpoint / GameObject family / 42 trigger subclasses / FMODAudioEngine / CCAction. Each leak entry here links back to the originating audit for full context.

**Drift advisory:** the audits read `_deps/bindings-src/bindings/2.208/GeometryDash.bro`. We compile against `build/_deps/bindings-src/bindings/include/Geode/...` generated headers, which the GJGameState agent found may be newer (added/renamed fields). When implementing a fix, verify field names against the headers your code actually links against — names in this doc may need adjustment.
