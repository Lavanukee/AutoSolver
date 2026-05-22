# Bindings notes

Two-pass strategy:

- **`survey/`** — first-pass pokes head into every binding header (A-F, G-L, M-R, S-Z splits, ~640 files). One line per file at minimum; deeper notes when something jumped out as bot-relevant. Use as a haystack search target.
- **`deep-dive/`** — second-pass on bot-touching headers only. Adds nothing the survey doesn't already say; just collected here for fast lookup.

## Top cross-cutting findings (consolidated from the surveys)

These are the actionable items the survey turned up that aren't yet acted on in code, ordered by likely impact:

1. **`GJBaseGameLayer::getModifiedDelta(dt)`** (L1604, G-L:F3). If the engine multiplies dt by a time-warp factor here and our sim feeds raw `m_frameDt`, every timewarp/dual-portal divergence is mechanical. The 240Hz substep machinery (`m_isBetweenSteps`, `m_clickBetweenSteps`, `m_clickOnSteps`, `m_currentStep`, `m_extraDelta`, `shouldUseSubstepForButton(dt)` at L3026) is also unsnapshotted — directly relevant to the user's staircase-desync hypothesis (sim 60 Hz scheduling vs. real 240 Hz substep alignment).

2. **`PlayerCheckpoint::m_jumpBuffered`** (M-R survey). The engine's own checkpoint struct preserves jump-buffer state, but `copyAttributes` does NOT carry `m_jumpBuffered` from real to sim. A real player landing one tick before a stair tile has its press buffered for the post-landing frame; sim's player at the same state has no buffer → sim "lands clean" while real "lands and jumps", and the bot picks a path that survives in sim but dies in reality. Probable D2 root cause for staircase residuals.

3. **`GJGameState::m_activatedObjectIDs`** (G-L:F4, L201, `map<pair<int,int>, int>`). Engine's per-object dedupe — most likely a per-orb activation count. Not in `LayerStateSnapshot`. If sim writes, real player can't reactivate same orb; if real activates, sim's `hasBeenActivatedByPlayer` reads stale.

4. **`m_calcNonEffectObjects`/`m_solidCollisionObjects`/`staticObjectsInRect`** (G-L:F5). The engine already maintains a per-frame physics-active list; spatial cull in `Hooks.cpp::collisionCheckObjects` could read these instead of filtering the full vector. Lower-priority — the existing 80-block window already trims most fat.

5. **`flipGravity`/`toggleDualMode`/`toggleFlipped`/`updateTimeMod` accept `noEffects=true`** (G-L:F1, L1343/L3287/L3296/L3926). Cleaner than hooking 12 particle/camera methods individually — pass `noEffects=true` from sim to suppress at the source. (Existing C-section hooks still work; this is a future refactor option.)

6. **`removeTemporaryParticles()` / `m_temporaryParticles`** (G-L L2729/L4209). Engine's own end-of-cycle particle cleanup. Defensive sim-end flush option.

7. **PlayerObject direct portal particle methods** (M-R survey: `spawnPortalCircle`, `spawnDualCircle`, `spawnScaleCircle`, `spawnCircle`, `spawnFromPlayer`). Bypass the EnhancedGameObject::activatedByPlayer path entirely. Suppress at the PlayerObject level if portal effects still leak after the GJBaseGameLayer hooks.

8. **Layer-side camera fields not snapshotted**: `m_staticCameraShake`, `m_skipCameraShake`, `m_resumeTimer`, `m_cameraWidthOffset`, `m_cameraHeightOffset`, `m_cameraUnzoomedHeightOffset`, `m_targetCameraHeightOffset`, `m_calculateTargetHeightOffset`. Plus 29 `m_unkPoint*` fields in GJGameState L92-120 (likely camera-tween intermediates). Cheap snapshot extension.

9. **`PlayLayer::takeStateSnapshot()` / `compareStateSnapshot()`** (M-R). Engine has built-in state-snapshot infrastructure. Worth investigating whether we can replace our hand-rolled `LayerStateSnapshot` with the engine's mechanism — would auto-pick up future engine fields.

10. **`m_queuedButtons` cloning** (G-L:F7, L4278/L4279/L4282). If `processQueuedButtons` is called from the sim path with `clearInputQueue=true`, the real-game queue is wiped. Audit whether sim ever drains the real queue.

## Layout

```
bindings-notes/
  README.md              this file
  survey/
    A-F.md               first-pass binding header survey
    G-L.md               (heaviest — covers GJBaseGameLayer + GJGameState + GameObject)
    M-R.md
    S-Z.md
  deep-dive/             (deferred — surveys cover the bot-touching surface adequately)
```
