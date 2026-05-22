# Lessons learned

Each file captures a non-obvious decision made during development — why we landed where we did, and what the alternative looked like. The intent is so future agents (and future me) don't re-derive it from the diff.

These are NOT "how the code works" — that's what the code is for. They're the *why*.

- [01 — Frame independence](01-frame-independence.md) — moving input injection to `PlayerObject::update` PRE-super, why `pushButton`/`releaseButton` over `handleButton`+queue
- [02 — Sim state isolation](02-sim-state-isolation.md) — `LayerStateSnapshot` scope (per-runBranch + per-runPlan, not per-simulate)
- [03 — `m_touchingRings` shared CCArray](03-touchingRings-shared-pointer.md) — `copyAttributes` shallow-copies a pointer field; sim's clear was clobbering real's array
- [04 — Portal physics vs. effects](04-portal-physics-vs-effects.md) — sim MUST call super on mode-switch portals (physics) but should suppress particles
- [05 — Reentrancy and level entry](05-reentrancy-and-level-entry.md) — `m_inSearch` / `m_inSimulate` / `m_levelReady` and the `setupHasCompleted` timing window
- [06 — Frozen files policy](06-frozen-files-policy.md) — `Portals.cpp` / `Pads.hpp` / `Orbs.cpp` are not edited; extend via `Hooks.cpp` instead
