# Portal physics vs. effects

## The mistake

Portal hooks initially routed *all* sim activity straight to `return` — assuming "no effects" meant "skip everything for sim". Result: gamemode-switch portals (cube↔ship, cube↔wave, etc.) silently failed for sim. The sim crossed a wave portal and stayed in cube mode; the bot picked plans that "survived" because the wrong physics was applied to the sim.

## The split

Portal-induced state changes are two distinct concerns:

- **Physics state** (gamemode, gravity, scale, time-warp, dual-mode flag, level-flip flag): **MUST** apply to sim. The sim's whole point is to run engine-faithful physics; skipping these produces a sim in the wrong physics regime.
- **Effects** (particles, camera tweens, lightning flashes, sound, screen-shake): **MUST NOT** apply to sim. Real player still needs to see them once when *they* cross the portal. Sim crossings firing them produces visual ghost-effects and can leak CCActions past the sim's snapshot scope.

## What we ended up doing

Each portal/mode-switch hook has the form:

```cpp
void someToggle(...) {
    if (sim().isSimulating()) {
        if (!s.isSimPlayer(p)) return;     // not our sim → drop
        Engine::someToggle(..., /*noEffects=*/true);  // sim → physics only
        return;
    }
    Engine::someToggle(...);                // real player → full
}
```

The `noEffects` parameter on engine functions like `flipGravity`, `toggleDualMode`, `toggleFlipped`, `updateTimeMod` does the suppress-particles-but-do-physics part for free (G-L survey F1). Where the engine doesn't have a `noEffects` parameter, we hooked the particle/camera methods directly: `spawnParticle`, `spawnParticleTrigger`, `lightningFlash`, `playSpeedParticle`, `cameraMoveX/Y`, `updateCameraOffsetX/Y`, `updateStaticCameraPos*`, `flashPlayer`. Pattern: `if (sim().isSimulating()) return; super(...)`.

## What's still leaking (per surveys)

- `PlayerObject::spawnPortalCircle`, `spawnDualCircle`, `spawnScaleCircle`, `spawnCircle`, `spawnFromPlayer` (M-R survey) — direct particle methods on PlayerObject that bypass our GJBaseGameLayer hooks. Suppress at the PlayerObject level if leaks persist.
- The `noEffects` parameter forwarding could replace some hooks for cleaner code (G-L F1).

## References

- `src/Hooks.cpp` — search `flipGravity`, `toggleDualMode`, `toggleFlipped`, `spawnParticle`, `cameraMoveX`, `flashPlayer`
- `src/Portals.cpp` — FROZEN, see [06](06-frozen-files-policy.md)
- Bindings: `flipGravity` G-L L1343, `toggleDualMode` L3287, `toggleFlipped` L3296, `updateTimeMod` L3926
