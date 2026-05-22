# Frozen files policy

## What's frozen

These three files are NOT modified going forward:

- `src/Portals.cpp`
- `src/Pads.hpp`
- `src/Orbs.cpp`

They contain hooks that are working correctly and have been tested across many levels. New behavior that *would* logically belong in them is added to `src/Hooks.cpp` instead, even if that means a hook for the same class lives in two files.

## Why

Each of these files has subtle physics tuning baked in over many iterations. A "small refactor" or "tidy-up" pass on them has, historically, broken physics that took multiple test rounds to diagnose. The cost-benefit of touching them is asymmetric: small upside, large downside.

Examples that *would* have lived in `Portals.cpp` but were added to `Hooks.cpp` instead:
- The portal particle suppression hooks (C-section work: `spawnParticle`, `spawnParticleTrigger`, `lightningFlash`, etc.).
- The camera CCAction suppression (`cameraMoveX`, `cameraMoveY`, `updateCameraOffsetX/Y`, `updateStaticCameraPos*`).
- Any future portal-side `noEffects` forwarding.

Examples for `Orbs.cpp`:
- Per-iter `clearSimRingsPerIter` scaffolding (E3) lives in `Trajectory.cpp` rather than as orb-side state changes.
- Any future orb activation diagnostic / dedupe logic goes in `Hooks.cpp::TrajEnhancedHook`.

## How to know if it's safe to break the rule

If a code change to a frozen file is *strictly necessary* (the same effect cannot be achieved by hooks in `Hooks.cpp`), pause and verify with the user before editing. Default is: extend, don't modify.

## References

- `src/Hooks.cpp` — the canonical extension point
- Any commit that says "follow-up to FROZEN file" and explains why a Hooks.cpp addition wasn't sufficient
