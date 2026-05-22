# Sim state isolation — `LayerStateSnapshot`

## The problem

`PlayLayer::checkCollisions(sim, dt, false)` and `sim->update(dt)` both mutate fields on `PlayLayer::m_gameState` (camera position/zoom/angle, dual-mode flag, level-flip flag, gravity mod, portal-entry markers, time-mod fields). Without a guard, the sim's run leaves those fields perturbed, and the real player's next frame inherits camera deflection, an inappropriate dual-mode flag, etc.

## What we ended up doing

A POD `LayerStateSnapshot` struct in `Trajectory.cpp` captures the engine-mutated subset of `m_gameState` at scope entry and writes it back at scope exit (RAII destructor). Two scope levels:

- **Per-`runBranch`**: trajectory viz path. The hold/release branches are independent — neither should see the other's mutations.
- **Per-`runPlan`**: bot-search path. Each candidate plan starts from the same baseline; later plans must not see earlier plans' state.

NOT per-`simulate()`. Wrapping at the outer level meant nested mutations from one branch's portal could leak across runBranch boundaries before being restored, and we wanted every branch / plan run to be a hermetic unit.

## What's in the snapshot

The fields we know the engine mutates during sim:
- Camera: `m_cameraPosition`, `m_cameraZoom`, `m_cameraAngle`, `m_targetCameraAngle`, `m_cameraEdgeValue0..3`
- Mode: `m_isDualMode`, `m_isFlipped`, `m_currentSpeedType`
- Per-tick markers: `m_unkPlayer1*` portal markers, `m_isPracticeMode` (engine sets this transiently in some paths)
- Gravity / physics mod: `m_gravityMod` derivatives that propagate from triggers

## What's NOT in the snapshot — and why

Per the G-L survey (L92-120), `GJGameState` has 29 unidentified `m_unkPoint*` fields in the camera region — likely camera-tween intermediates, but unverified. Adding them to the snapshot is a cheap follow-up. Also unsnapshotted: `m_extraDelta`, `m_isBetweenSteps`, `m_clickBetweenSteps`, `m_clickOnSteps`, `m_currentStep`, `m_tweenActions` map. The map case is intentional — `tweenActions.insert` from sim is suppressed at hook level (TrajEffectHook), so the map should never be modified during sim. Other unsnapshotted fields: M-R survey notes the engine has built-in `takeStateSnapshot()`/`compareStateSnapshot()` infra we could possibly delegate to.

## Anti-pattern: per-hook restores

Earlier iteration: each portal hook saved `m_isFlipped` before super, restored after. This worked for solo mutations but stacked nested portals (cross-portals during gravity-flip + dual-mode change in the same tick) corrupted each other's saves. The whole-state snapshot at the outer scope is the only correct level.

## References

- `src/Trajectory.cpp` — search for `LayerStateSnapshot`
- Bindings: `GJGameState.hpp` (G-L survey L92-120 unkPoint fields, L201 m_activatedObjectIDs)
- `bindings-notes/README.md` finding 8 (snapshot extension list)
