# Bloodbath Ball: null-cause death at x=746

## Symptom

On Bloodbath Ball, the bot dies consistently at exactly (746.2, 313.8),
percent=14.64. Every attempt hits this same spot. Reproducible bit-exactly
across runs.

## Telemetry signature

```
[TEL] real_death pos=(746.2,313.8) percent=14.64
[TEL] death  who=real1 percent=14.64 player_pos=(746.2,313.8) cause_obj_type=-1 anticheat=0
```

`cause_obj_type=-1` = `gameObject` argument to `destroyPlayer` was **nullptr**.
`anticheat=0` = it's not the engine's `m_anticheatSpike`. Some other engine
path is killing with no cause object.

## Divergence trace

For the bot's committed plan (`plan_start=497 plan_len=268 end_pos=(946,294.9)`),
sim's predicted trajectory matches real's actual trajectory **bit-exactly**
all the way to (746.158, 313.839):

```
tick=572 pred=(745.112,311.467) actual=(745.112,311.467) d=0.0000 yVel=10.41  simYVel=10.41
tick=573 pred=(746.158,313.839) actual=(746.158,313.839) d=0.0000 yVel=10.54  simYVel=10.54
```

Same X, Y, yVel, gravity, speed, gnd flags, button state. Yet on tick 574
real dies; sim survives, sim's plan continues to x=946 where sim eventually
dies.

This is a **death-detection asymmetry**, not a position/velocity divergence.

## What was tried and didn't change the death position

All of these produced the EXACT same `pos=(746.2,313.8)` death:

1. **Disabling per-tick `updateEffects` + move actions** — verified the
   effect-manager update cadence isn't the cause.
2. **Adding `m_pl->checkSnapshot()` per sim tick** with `m_player1` swapped
   to sim. checkSnapshot is the engine's player-state-snapshot entry; if
   it ran a cheat check on sim, sim would die at the same spot. Didn't.
3. **Adding full `m_pl->update(dt)` per sim tick** with `m_player1` swapped
   to sim. The intent: any per-tick player validation gated on `player ==
   m_player1` would now fire on sim. Sim's commit plans were byte-identical
   to without the swap — sim's death-detection wasn't enriched.
4. **Player swap only around `sim->update(dt)` + `m_pl->checkCollisions`**.
   Surgical version of #3. No effect.

## Stack trace of the null-cause death

```
#0  TrajPlayLayerHook::destroyPlayer
#1  Geode dispatch
#3  eclipse.eclipse-menu.dylib: ShowHitboxesPLHook::destroyPlayer  (passthrough)
#5  eclipse.eclipse-menu.dylib: VariablesPLHook::destroyPlayer     (passthrough)
#9  Geometry Dash + 1043904  (some engine fn)
#10 Geometry Dash + 1194000  (inside GJBaseGameLayer::update at +0x828)
#13 eclipse.eclipse-menu.dylib: PauseBGLHook::update              (Eclipse's update wrapper)
```

The kill originates inside `GJBaseGameLayer::update` somewhere. The Eclipse
hooks in the chain are passthroughs around `destroyPlayer` — they don't
originate the call. So this is GD's own update-path logic firing
`destroyPlayer(player, nullptr)` for the real player.

## Working hypothesis

Some engine check inside `GJBaseGameLayer::update` (or one of its callees)
fires destroyPlayer with null cause when the player is in an "impossible"
state. Candidates:

- **Anti-cheat impossible-trajectory detection** — engine detects ball at
  yVel=10.5 (much higher than ball-mode max ~5) and kills.
- **Out-of-bounds Y** — ball at y=313 + projected y=324 next tick exceeds
  some level/camera Y bound.
- **Wedged-in-solid detection** — ball clips into a solid block more than
  some tolerance.
- **Eclipse-specific check** in one of Eclipse's many PlayLayer hooks
  (unlikely given the stack analysis but not ruled out).

The check is **not gated on `player == m_player1`** — both player-swap
experiments failed to trigger it on sim. So replicating it via "make sim
look like m_player1" isn't enough.

## What this means for the bot

The plan the bot commits has sim surviving to x=946. Real dies at x=746
during the same plan. Bot has no signal that the plan is bad because sim
doesn't die where real does. Bot keeps committing the same plan; player
keeps dying at the same spot.

This is a **sim has lower fidelity than real for some death-detection
condition**. The fix is either:

1. Identify the specific check in the engine and replicate it in sim.
2. Disable Eclipse Menu temporarily and re-test to A/B confirm Eclipse
   isn't somehow involved (the stack shows passthrough hooks but Eclipse
   has many features and one of its update hooks might be conditionally
   firing the kill).
3. Add an empirical heuristic to sim — e.g. reject plans where predicted
   yVel exceeds a gamemode-realistic max — as a band-aid until the engine
   check is found.

## Reference state

The PlayerCheckpoint sync that landed in commit `e463bbe` resolves a
different bug class entirely (button-hold-duration drift, mode-transition
state). It does NOT help with this null-cause-death class because the
state IS bit-exactly matching between sim and real — the gap is in the
engine's per-tick validation, not in initial state copy.

Next session: try (2) first (easiest to confirm/exclude Eclipse), then
add divergence logging for ball-mode-specific fields like `m_objectSnappedTo`
and `m_isOnGround*` to see if state differs DURING the lead-up despite
positions matching. Per-tick effect manager updates and player swap are
both known to not help.
