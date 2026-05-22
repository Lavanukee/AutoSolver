# Issue: Ball portal treated as wall on attempt 1 (self-heals on attempt 2)

## Symptom

On the **first attempt of a level** that contains a ball portal, the bot's
predicted (orange) path enters the ball-portal region and behaves as if the
portal were a solid obstacle:

- Path heads DOWN through the floor at the portal's X coordinate, as if
  gravity is acting and ground collision has stopped working.
- Path then oscillates (zig-zag) along the bottom of the playable area for
  the rest of the prediction horizon.

On the **second attempt of the same level** (after dying / resetting), the
sim correctly transitions into ball mode at the portal — predicted path
shows the bouncing arcs characteristic of ball-mode physics. No code change
between attempts; the sim just starts working.

This rules out: object misclassification (would persist), input-plan bugs
(inputs are re-searched per attempt), and visual-only artifacts (the bot
also commits to and follows the broken path on attempt 1, so the sim's
*scoring* says the broken path is the longest survivor — meaning the sim's
internal state genuinely treats ball-mode physics as not-yet-engaged).

## Where it shows up

Reproduced visually on the test level captured in the screenshots from the
2026-05-09 session — first ball portal in the level. Likely reproduces on
any level whose first sim-reachable ball portal precedes the player's first
death.

Possibly applies to OTHER mode portals too (wave, ufo, robot, spider,
swing) — the diagnosis below should be tested against those before
assuming ball-portal-specific.

## Why this is odd

Mode-portal physics during sim is supposed to flow through the engine's
super path on the collision tick. There is no per-mode-portal hook that
gates sim activation differently between attempts. The bot's cached
`m_best` plan is cleared in `Bot::onPlayLayerReset` so each attempt starts
with a fresh search. The only thing that DOES NOT get reset between
attempts is the sim `PlayerObject` itself — `TrajectorySimulator::onPlayLayerReset`
explicitly does NOT recreate `m_simP1` / `m_simP2`; it only clears
per-attempt flags (`m_simulating`, `m_simP1Dead`, etc.). So state stored on
the sim `PlayerObject` from attempt 1's runs persists into attempt 2.

That's the strongest signal: whatever attempt 1's first sim run leaves on
the sim PlayerObject is what makes attempt 2 correct. Identify that
mutation → identify the missed-init on level entry.

## Likely root causes (ranked)

### #1 — Sim PlayerObject sub-state not fully initialized by `PlayerObject::create`
**Mechanism:** `createSimPlayer` builds the sim via
`PlayerObject::create(1, 1, pl, pl, true)` — pure cube defaults
(`Trajectory.cpp:165`). The engine creates per-mode child sprites
(ball, ship, wave, etc.) and per-mode physics fields lazily, often on first
mode-toggle for that vehicle. When the sim crosses a ball portal mid-sim,
the engine's super invokes `toggleBallMode` on the sim. If a required
child (e.g., the ball sprite, or a physics-related child node) isn't
present yet, the toggle may set the `m_isBall` flag but skip part of the
mode-transition (e.g., velocity reset, hitbox swap), leaving the sim in a
half-cube/half-ball state. Subsequent sim ticks read the half-state and
produce broken physics — gravity-fall + ground-clip → the observed
"down through floor + oscillation" pattern.

After attempt 1, the lazy children/state are now constructed on the sim
PlayerObject and persist across `onPlayLayerReset`. Attempt 2's first
ball-portal crossing finds the state ready and the toggle completes.

**How to distinguish:** at `onPlayLayerInit`, force-construct each
gamemode by calling `toggleBallMode(true)`, `toggleDartMode(true)`, etc.
on each sim PlayerObject (then toggle them all back to cube). If attempt 1
now matches attempt 2, this is the cause.

### #2 — `LayerStateSnapshot` captures a stale level-load default
**Mechanism:** `LayerStateSnapshot` snapshots `GJGameState` POD fields
(including `m_lastActivatedPortal1/2` per `Trajectory.cpp:99`) at the
start of each `runPlan` and restores after. On attempt 1, the very first
snapshot is taken before the engine's PlayLayer-load init has fully
populated some field (e.g., a portal pointer that should be null but is
left at construction default). Sim's portal handler reads this stale field
and treats the ball portal as already-activated / different-portal, so the
mode-toggle path is skipped. After attempt 1, the engine's reset path
writes the field to its proper "no portal active" value and the snapshot
on attempt 2 captures the correct state.

**How to distinguish:** log every `GJGameState` field captured by
`LayerStateSnapshot` at the start of attempt 1's first sim run and at the
start of attempt 2's first sim run. Diff. Any field that differs is a
candidate.

### #3 — Sim `PlayerObject` is never registered as a "real" player in layer-tracked sets
**Mechanism:** the engine's mode-portal collision path may check the
target player against `m_pl->m_player1` / `m_player2` (or a layer-side
"known players" set) before applying mode toggles. The sim is created via
`PlayerObject::create` and added as a child of `m_objectLayer`, but it's
never registered as `m_player1`/`m_player2`. The engine path may have an
"is this a real player?" guard that fails silently for the sim. Why this
would self-heal on attempt 2 is unclear — possibly the engine's reset
path runs through a code path that *does* register the sim, or possibly
this hypothesis is wrong.

**How to distinguish:** trace the engine's mode-portal handler in disasm /
through bindings. Look for any check of player identity before
`toggleBallMode` is called. If none, this isn't the cause.

### #4 — First-attempt-only `PlayLayer::update` ordering: bot searches before engine has ticked the layer once
**Mechanism:** `Bot::setLevelReady(true)` is set somewhere after PlayLayer
init. If the bot starts evaluating plans BEFORE the engine's first
`PlayLayer::update` tick, layer-side state that the engine populates
during its first update (e.g., `m_speedObjects`, `m_groupNodes`,
`m_groundLayer` position) may be at default. Sim physics then uses the
default state and produces wrong predictions. After the first real tick
runs, state is populated; on attempt 2 the layer has been fully ticked,
so the first sim run uses correct state.

**How to distinguish:** log `levelReady`'s set point and check whether
the engine's first `PlayLayer::update` has run by then. If sim runs
happen before the first engine tick, gate `m_levelReady` on at least one
real engine update having completed.

### #5 — `m_activated` set leaks ball-portal entry across runs
**Mechanism:** `TrajectorySimulator::m_activated` is the per-runPlan
"already triggered" set used by `TrajEffectHook::simShouldSkip`. It IS
cleared at the start of each `runPlan`/`runBranch` (per the speed-portal
fix). But if for some reason the ball portal enters the set on the very
first sim run and a subsequent `runPlan` doesn't clear before checking,
the portal would silently no-op for the rest of attempt 1. Lower
likelihood — `runPlan` always clears `m_activated` before sim work — but
worth ruling out by adding an assertion that the set is empty at runPlan
start.

## Diagnostic steps

1. **Confirm the symptom is on the SIM, not the visualizer.** Run the bot
   on a ball-portal level. Watch the log for `# [DIVERGE]` rows around the
   portal X on attempt 1 — if the sim's predicted Y on tick N+1 is well
   below the floor and reality's Y is above, the sim itself is broken
   (not just rendering).
2. **Test hypothesis #1 directly.** In `createSimPlayer`, after the
   `PlayerObject::create` call, force-toggle each mode on/off:
   ```cpp
   p->toggleBallMode(true);   p->toggleBallMode(false);
   p->toggleDartMode(true);   p->toggleDartMode(false);
   p->toggleShipMode(true);   p->toggleShipMode(false);
   // etc. for all gamemodes
   ```
   If attempt 1 now matches attempt 2, the missing init was lazy
   sub-state construction. (This is a diagnostic test, not necessarily
   the right fix — but it isolates the cause.)
3. **Snapshot diff.** Add a one-shot log dump of `GJGameState` and the
   layer's portal-related fields at: (a) the moment `setLevelReady(true)`
   fires, (b) right before attempt 1's first sim run, (c) right before
   attempt 2's first sim run. Diff (b) vs (c). The differing field is the
   stale state.
4. **Bisect with mode-portal mix.** If reproducing requires a ball
   portal specifically, also test wave, ship, ufo, robot, spider, swing
   on isolated test levels to confirm whether this is ball-portal-only
   or applies to all mode portals. (Hypothesis #1 predicts: applies to
   any mode the sim hasn't been transitioned into yet.)

## Fix approach (once diagnosed)

If hypothesis #1 is confirmed: change `createSimPlayer` to explicitly
warm up every gamemode after creation (toggle each on then off), so the
sim's sub-state is fully constructed before any sim run. This is
single-pass at level-load and doesn't affect sim physics, since the
final state is "everything off → cube" matching `PlayerObject::create`'s
default.

If hypothesis #2 or #4: extend `LayerStateSnapshot` with the stale field
or gate `setLevelReady` on at least one engine tick having run.

## What to NOT do prematurely

- **Don't** add a per-mode-portal hook that manually calls
  `toggleBallMode` on the sim — that's a monkey patch around the real
  initialization gap and will rot when new mode portals are added.
- **Don't** force `m_simP1`/`m_simP2` to be recreated on every
  `onPlayLayerReset` — it would mask hypothesis #1 by warming up via
  attempt-2's create, but it would do so by paying the full
  PlayerObject construction cost on every reset, and it doesn't fix
  attempt 1.
- **Don't** clear the cached best path when a bad-looking path is
  detected — symptom-bypass that hides the underlying sim bug.

## References

- `src/Trajectory.cpp:164-171` — `createSimPlayer`
- `src/Trajectory.cpp:204-239` — `onPlayLayerInit` (creates sim players)
- `src/Trajectory.cpp:241-249` — `onPlayLayerReset` (does NOT recreate
  sim players; this is the persistence path)
- `src/Trajectory.cpp:85-145` — `LayerStateSnapshot` capture/restore
- `src/Hooks.cpp:215-225` — comment block describing mode-portal sim
  handling assumptions (super does the work)
- `src/Portals.cpp` — FROZEN; only `toggleDualMode` / `checkCameraLimitAfterTeleport`
  are gated for sim, no mode-toggle gating
- Bindings: `PlayerObject::toggleBallMode`, `toggleDartMode`,
  `toggleShipMode`, etc. — the lazy-init suspects per hypothesis #1
