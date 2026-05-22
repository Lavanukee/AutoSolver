# `m_touchingRings` shared CCArray hazard

## The bug

`PlayerObject::m_touchingRings` is `CCArray*` — a pointer field. The constructor allocates one. `copyAttributes(base)` is a member-by-member copy; for pointer fields it shallow-copies the pointer, so post-`copyAttributes` the sim and the real player point to the *same* `CCArray`.

The sim's per-branch reset called `m_touchingRings->removeAllObjects()` — which emptied the array shared with the real player. The engine's collision pass for the sim then refilled it with sim-position overlaps. Net effect: real player's "rings I'm currently touching" was being clobbered every sim run, and orb activation logic that consults this set saw garbage.

Symptom: real player would sometimes activate orbs the sim had touched but the real player wasn't near, and miss orbs the real player was touching but the sim had cleared. Not deterministic — depends on which sim branch ran last vs. which orbs the real player overlapped this tick.

## What we ended up doing

Each sim player gets its own retained `CCArray*`, stored on the simulator (`m_simP1OwnRings`, `m_simP2OwnRings`). `clearSimRingState` (full reset) and `clearSimRingsPerIter` (per-runPlan-iter reset) both:

1. Restore `sim->m_touchingRings` to point at the sim's own retained array.
2. Call `removeAllObjects()` on that array.

Because the restore happens *after* every `copyAttributes`, the sim's pointer is corrected before the next clear ever runs. The real player's array is never touched.

`m_touchedRings` (note: singular `Ring`s, an `unordered_set` not a CCArray) does NOT have this problem — it's a stack-allocated container, not a pointer. Cleared with `.clear()`.

## Why one reset point, not per-iter

`clearSimRingState` runs once per `runBranch` / `runPlan`, after `copyAttributes`. Resets the *full* ring/dash state set: `m_isDashing`, `m_dashAngle`, `m_dashStartTime`, `m_padRingRelated`, `m_ringJumpRelated`, etc. — anything the engine writes during a multi-frame ring/dash sequence that we don't want bleeding from one branch to the next.

A short-lived per-iter clear (`clearSimRingsPerIter`, 2026-05) cleared `m_touchingRings`/`m_touchedRings` before every `checkCollisions` to address sim's "false-hit" symptom (sim activates an orb at a position it's not currently near). It helped some cases but broke multi-activate orbs and high-CPS scenarios that rely on `m_touchedRings` persisting per-tick activation dedupe across plan iterations. **Reverted.** The false-hit symptom is real but the root cause is elsewhere — likely related to:

- Engine fields that `copyAttributes` doesn't carry (the M-R survey calls out `m_jumpBuffered` family, `m_holdingButtons`, `m_isOnGround2/3/4`, `m_lastPortalPos`, `m_touchedGravityPortal` as "engine-forgets" candidates).
- `GJGameState::m_activatedObjectIDs` (G-L survey L201) — the per-object dedupe map that's not in `LayerStateSnapshot`.
- Any path through orb activation that bypasses `EnhancedGameObject::activatedByPlayer` (where our hook lives).

The orb-activation diagnostic at `markActivated` (logs sim-to-orb distance > 30 units on first activation) is the right next step: collect data, then patch at the actual source.

## References

- `src/Trajectory.cpp::adoptOwnRings` (own-array allocation)
- `src/Trajectory.cpp::clearSimRingState` (full reset, post-copyAttributes)
- `src/Trajectory.cpp::markActivated` (orb-far-activate diagnostic)
- Bindings: `PlayerObject::m_touchingRings`, `m_touchedRings`, `m_dashRing`, `m_padRingRelated`, `m_ringRelatedSet`
