# Issue: Speed-mod portal leaks from sim to real player

> **Resolved in 1.0.2 + 1.0.3.** 1.0.2 fixed the sim → real leak by
> snapshotting `m_timeModRelated`/`m_timeModRelated2`. 1.0.3 fixed the
> regression that 1.0.2 exposed (sim players never received speed updates
> because the engine's fan-out only writes `m_player1`/`m_player2`).
>
> **Four failed propagation routes before the shipping one:**
>
> 1. *Layer-level mirror.* Calling `simP1()->updateTimeMod` /
>    `simP2()->updateTimeMod` from inside `TrajLayerSpeedHook::updateTimeMod`
>    after super. Dead code: instrumentation already showed
>    layer->updateTimeMod is reached only with `players=false` during sim
>    portal crossings, so the hook never ran with the right gate set.
>
> 2. *EffectGameObject capture-and-copy.* Save real-player speeds before
>    super, run super, copy the engine's post-super real-player values
>    onto sim, then restore real. Failed because the engine's super never
>    directly writes the sim player's `m_playerSpeed` (sim is invisible
>    to the engine), AND the per-player `TrajPlayerObjectHook` virtual
>    gate blocks any virtual `updateTimeMod` call to real — so post-super
>    real-player values are unchanged from pre-super, and there's no
>    delta to copy onto sim.
>
> 3. *Calling `PlayerObject::updateTimeMod(newSpeed, /*noEffects=*/true)`
>    on each sim player.* Sets `m_playerSpeed` correctly but has unwanted
>    side effects on a single call: clamps `m_yVelocity` and/or schedules
>    a transition that locks vertical motion. Symptom: sim's trajectory
>    goes flat horizontal at GROUND level past the speed portal.
>
> 4. *Run super THEN write `m_playerSpeed`/`m_playerSpeedAC` directly.*
>    Idea was: let super update layer-level state (`m_speedObjects`,
>    `m_timeModRelated`) and the portal's flags (we save/restore those),
>    then bypass `updateTimeMod` for the per-player part by writing the
>    speed fields ourselves. Failed differently: trajectory goes flat
>    horizontal at PORTAL-ENTRY height (not ground), suggesting super
>    itself does some undocumented mutation during speed-mod trigger that
>    locks the sim's y-motion — separate from `updateTimeMod`'s clamp.
>
> **Shipping fix:** Skip super entirely during sim. `TrajEffectHook::propagateSpeedToSim`
> computes the new speed value from the portal's `m_speedModType` via
> `LevelTools::valueForSpeedMod` and writes `m_playerSpeed`/`m_playerSpeedAC`
> directly on each sim player. No super call, no engine-side mutations,
> no real-player save/restore needed. Layer-level state stays at
> pre-trigger values — exactly what we want, since the real player will
> re-traverse the portal later and the engine should fire it then with
> no leftover state. Sim's `update()` reads `m_playerSpeed` directly per
> tick for X advance; the layer-level array (`m_speedObjects`) is
> irrelevant to sim physics because sim doesn't run layer->update.
> Per-runPlan activation gate (`simShouldSkip`) still in place so the
> direct write fires once per portal per plan, matching the real
> player's one-shot crossing semantics.
>
> Document kept for the diagnosis trail; the routes section is the
> working hypothesis space, not a current TODO.

---


## Symptom

When the sim crosses a speed-mod portal mid-search, the **real player's**
`m_playerSpeed` flips to the post-portal value immediately, even though the
real player has not yet reached the portal. The real player visibly
accelerates / decelerates on the spot, before the bot's plan ever calls for
input that would carry the real player into the portal.

This is observed for speed-mod portals only. Mode-switch portals (cube/ship/
ball/wave/etc.), gravity portals, level-flip portals, and dual-mode portals
do **not** exhibit this leak — the sim crosses them, the snapshot+source-
suppress design holds, and the real player only changes mode/gravity/etc.
when it actually reaches the portal.

## Confirmed leak path (from a prior backtrace, before the rollback)

The leak fires from a **post-sim, normal frame** — `isSimulating() == false`
at the moment of the write — through this stack:

```
frame 0: TrajPlayerObjectHook::updateTimeMod        (our hook)
frame 1..5: Geode trampoline / hook-dispatch frames (unsymbolicated)
frame 6: GJBaseGameLayer::update + 0xe8             (engine per-frame update)
frame 7: TrajBaseLayerHook::update + 0x78           (our hook on layer update)
frame 8..9: Geode trampoline frames
frame 10: PauseBGLHook::update                       (eclipse mod, OUTER)
```

Key facts:

1. The leak path is `GJBaseGameLayer::update + 0xe8`, **NOT**
   `GJBaseGameLayer::updateTimeMod`. So `TrajLayerSpeedHook::updateTimeMod`
   never fires for this leak — the engine calls `PlayerObject::updateTimeMod`
   directly from inside its per-frame update, somewhere around offset +0xe8
   inside the binary.

2. The leak fires when `isSimulating == false`. So the per-player gate in
   `TrajPlayerObjectHook::updateTimeMod`
   (`if (s.isSimulating() && !s.isSimPlayer(this)) return;`)
   does **not** block it — the gate only fires during sim.

3. During the immediately-prior sim, the sim player's `m_playerSpeed`
   stayed at the *pre*-portal value (e.g. 0.9) — i.e. the sim never even
   reached `m_playerSpeed = 1.1`. Yet on the next real frame, the engine
   computes `1.1` specifically for the real player at this leak site.

4. This means: the engine's per-frame "what is the current effective speed
   for this player?" recompute, after the sim has ended and the snapshot
   has been restored, **still** returns the post-portal speed for the real
   player. Some piece of state survives the snapshot/restore cycle.

## Pre-existing baseline protections (already in commit 2e9cf71 — current HEAD)

Three layers of defense exist at HEAD; none of them block the observed
leak path.

### 1. Per-player virtual gate
`Hooks.cpp::TrajPlayerObjectHook::updateTimeMod` —
```cpp
void updateTimeMod(float speed, bool noEffects) {
    auto& s = sim();
    if (s.isSimulating() && !s.isSimPlayer(this)) return;
    PlayerObject::updateTimeMod(speed, noEffects);
}
```
Blocks `realP1->updateTimeMod()` calls during sim. Catches the per-player
virtual dispatch path.

### 2. Layer-level redirect with save/restore
`Hooks.cpp::TrajLayerSpeedHook::updateTimeMod` —
```cpp
void updateTimeMod(float speed, bool players, bool noEffects) {
    if (s.isSimulating()) {
        // save real m_playerSpeed/m_playerSpeedAC for both players
        GJBaseGameLayer::updateTimeMod(speed, players, noEffects);
        // restore real m_playerSpeed/m_playerSpeedAC
        return;
    }
    GJBaseGameLayer::updateTimeMod(speed, players, noEffects);
}
```
Catches the engine's binary `updateTimeMod` impl that inline-writes
`m_player1->m_playerSpeed` before delegating to the per-player virtual.

### 3. EffectGameObject save/restore wrapper
`Hooks.cpp::TrajEffectHook::triggerObject` and `triggerActivated` —
```cpp
saveRestoreRealPlayerSpeeds([&]{
    EffectGameObject::triggerObject(layer, uniqueID, remapKeys);
});
m_activatedByPlayer1 = prev1;  // also restores per-portal activation flags
m_activatedByPlayer2 = prev2;
```
Wraps the speed-mod portal's `triggerObject` super so the inline writes
during portal activation are reverted.

### 4. LayerStateSnapshot at runPlan / runBranch boundaries
`Trajectory.cpp::LayerStateSnapshot` snapshots and restores:
- camera fields (zoom, position, angle, edges, shake, step-diff)
- `m_isDualMode`, `m_dualRelated`
- `m_levelFlipping`, `m_gravityRelated`, `m_portalY`
- `m_lastActivatedPortal1/2` (on `m_gameState`)
- time-warp fields, channel fields
- **`m_speedObjects` (CCArray pointer set)** — the array the engine consults
  per tick to compute current effective speed.

So the design *should* be: sim adds a portal to `m_speedObjects` during
its traversal → snapshot at `runPlan` end removes it → real player's
next frame sees the pre-sim speedObjects → no leak.

But the leak fires anyway.

## Approaches tried in the previous session — and why each failed

### A. Tighten the per-player virtual gate
Tried: variations on the `TrajPlayerObjectHook::updateTimeMod` gate (pass-through
during sim only for sim players). Already in baseline.

**Why it failed:** the leak fires *post-sim* (`isSimulating == false`), so
the gate's condition is always false and never blocks. The gate is correct
but addresses a different leak path.

### B. Layer-level `updateTimeMod` redirect
Tried: adding `TrajLayerSpeedHook::updateTimeMod` to save/restore real
player speeds around the layer's super call. Already in baseline.

**Why it failed:** the engine path that produces the leak is
`GJBaseGameLayer::update + 0xe8 → PlayerObject::updateTimeMod`, **not**
through the layer's `updateTimeMod`. The redirect never fires for this
leak because the engine's per-frame update doesn't go through
`updateTimeMod` — it calls `PlayerObject::updateTimeMod` directly from
some inlined "compute current speed for player" code.

### C. EffectGameObject `m_activatedByPlayer1/2` save/restore
Tried: in `TrajEffectHook::triggerObject` and `triggerActivated`, save and
restore the portal's per-player activation flags around the super call.
Already in baseline.

**Why it doesn't address the leak:** this fix is necessary so the real
player's later real-crossing of the portal *does* fire (without it, the
sim's earlier crossing leaves `m_activatedByPlayer1 = true` and the
engine refuses to re-activate). Necessary for correctness, but it's about
making the portal *fire again* on real crossing — not about preventing
speed leakage in the meantime.

### D. Extend `LayerStateSnapshot` with per-player speed fields
Tried: add to capture/restore —
- `PlayerObject::m_playerSpeed`
- `PlayerObject::m_speedMultiplier`
- `PlayerObject::m_playerSpeedAC`
- `PlayerObject::m_lastActivatedPortal`
- `PlayerObject::m_lastPortalPos`
- `PlayerObject::m_somethingPlayerSpeedTime`

for both `m_player1` and `m_player2`, snapshotted at `runPlan` /
`runBranch` capture and re-applied at restore.

**Why it failed:** the leak still fired on the next real frame. So either
(a) the engine's per-frame recompute reads from a state slot we still
haven't identified, or (b) restore is *itself* being undone — e.g. some
CCAction or CCSchedule is still running on the real player and rewrites
the field after restore lands.

### E. Diagnostic instrumentation: bracket samplers, BOT-CROSS-TICK detection, layer-level redirect with backtrace dump
Tried: instrumented `updateTimeMod` gates and redirects with backtrace
captures via `execinfo.h` + `dladdr`, sampled `m_playerSpeed` at multiple
points in the engine's tick (update, updateEnterEffects, postUpdate,
processCommands, applyEnterEffect).

**Conclusions from logs:**
- The layer-level `updateTimeMod` was reached only with `players == 0` —
  meaning the real-player fan-out pathway through the layer's
  `updateTimeMod` never fired in the observed runs. The redirect was dead
  code for this scenario.
- The per-player gate's `GATED` branch never fired during sim — meaning
  `PlayerObject::updateTimeMod` was never called on the real player while
  sim was running. The leak isn't a sim-time write at all.
- The leak signature appeared exactly once per session, at the precise
  tick the sim's traversal reached the portal, and showed up as a
  post-sim normal-frame write.

So the entire instrumentation set narrowed the leak to **a per-frame
recompute that reads non-snapshotted state** — not a sim-time write that
slips through a gate.

## Working analogies — how *other* portal types isolate correctly

The user's question: speed portals leak; mode-switch / gravity / camera /
flip portals don't. What is structurally different about speed portals?

### Mode-switch portals (cube / ship / ball / wave / robot / spider / ufo / dart)

**Source-suppress the visible-state CCActions:**
```cpp
// Hooks.cpp
void animatePortalY(...)         { if (sim().isSimulating()) return; super(...); }
void animateInGroundNew(...)     { if (sim().isSimulating()) return; super(...); }
void animateInDualGroundNew(...) { if (sim().isSimulating()) return; super(...); }
void animateOutGroundNew(...)    { if (sim().isSimulating()) return; super(...); }
```

**Snapshot the per-tick physics fields:** `m_isDualMode`, `m_levelFlipping`,
`m_portalY` are in `LayerStateSnapshot`.

**Why this works:** the mode change itself is a one-shot write to a small
set of fields the engine reads each tick (the sim updates them, snapshot
restores them). The *visual* transition is an animation kicked off by
`animate*` functions; suppressing those at source means no CCAction
survives past sim end to mutate the real screen.

### Gravity portals

**Per-player + noEffects forwarding:**
```cpp
void flipGravity(PlayerObject* p, bool flip, bool noEffects) {
    if (s.isSimulating()) {
        if (!s.isSimPlayer(p)) return;            // not our sim → drop
        GJBaseGameLayer::flipGravity(p, flip, true);  // sim → physics only
        return;
    }
    super(p, flip, noEffects);
}
```

**Plus:** `playGravityEffect` suppressed at source on `PlayLayer`.

**Why this works:** `flipGravity` takes a player parameter, so we can
gate per-player. The `noEffects=true` parameter cleanly skips particles
during sim. The per-player flag (`PlayerObject::m_isUpsideDown` or analog)
is mutated only on the sim player.

### Camera tweens (shake / move / static / rotation)

**Source-suppress every action-kicking entry point:**
`shakeCamera`, `moveCameraToPos`, `cameraMoveX/Y`, `updateScreenRotation`,
`updateCameraOffsetX/Y`, `updateStaticCameraPos*`, all return early
during sim. Camera-position fields are snapshotted, but these are
secondary — the *primary* defense is suppressing the action source.

**Why this works:** camera tweens are CCActions running on the layer;
they outlive a snapshot. The only complete fix is to never start them
during sim.

### Level-flip portals (`toggleFlipped`)

```cpp
void toggleFlipped(bool flip, bool noEffects) {
    if (sim().isSimulating()) {
        super(flip, /*noEffects=*/true);   // force noEffects for sim
        return;
    }
    super(flip, noEffects);
}
```
**Plus** `m_levelFlipping` snapshotted.

**Why this works:** same combination — let physics flip during sim
(field gets mutated → snapshot restores), force `noEffects=true` to skip
the visible flip animation.

## Why speed-mod portals are structurally different

Three properties make speed portals harder than every working analogy:

1. **The state is a HISTORY, not a one-shot toggle.**
   - Mode portal: `m_isDualMode = true` is a single field write. Snapshot
     captures `false`, restores `false`. End of story.
   - Speed portal: appends to `m_speedObjects` (a CCArray of all crossed
     speed-mod portals). The engine reads this array per tick and
     selects the entry whose X-range contains the player. Snapshot has
     to restore the *contents* of the array — and even if it does, the
     engine may have cached derived values that don't get re-read from
     the array.

2. **The fan-out has no per-player parameter.**
   - `flipGravity(player, flip, noEffects)` takes a player → gate by
     `isSimPlayer(player)`.
   - `GJBaseGameLayer::updateTimeMod(speed, players, noEffects)` takes
     **no player parameter** — when `players == true`, the engine fans
     out to BOTH `m_player1` and `m_player2` regardless of who crossed.
     There's no "this is a sim crossing, only update simP1" pathway.

3. **The recompute happens every tick, not once at portal-cross.**
   - Mode portal: `m_isDualMode = true` happens once. After that, the
     engine just *reads* the flag.
   - Speed portal: every tick, the engine evidently re-derives "what
     speed should this player have right now?" from `m_speedObjects` (or
     a cached derivative) and writes it to `PlayerObject::m_playerSpeed`.
     This is the `update + 0xe8` path. Even after a perfect snapshot
     restore of `m_speedObjects`, if the engine reads from a different
     cache (per-player or per-layer), the recompute lands on the wrong
     answer.

## Where the leak likely lives — candidate caches not yet snapshotted

The previous session captured per-player `m_playerSpeed`, `m_speedMultiplier`,
`m_playerSpeedAC`, `m_lastActivatedPortal`, `m_lastPortalPos`,
`m_somethingPlayerSpeedTime`. The leak persisted, so something else is
the cache that survives restore. Candidates ranked by likelihood:

### **#1 — `GJGameState::m_timeModRelated` and `m_timeModRelated2`** (NEW, unverified)
Bindings `GeometryDash.bro:8978-8979`:
```
float m_timeModRelated;
bool  m_timeModRelated2;
```
These live on `GJGameState`, alongside the other layer-level fields we
*do* snapshot (`m_levelFlipping`, `m_isDualMode`, `m_lastActivatedPortal1/2`,
camera fields, time-warp fields). Our `LayerStateSnapshot` captures a
hand-picked subset of GJGameState — it does not capture
`m_timeModRelated*`. The naming strongly suggests they're the layer-level
"current time-mod" cache that the engine's per-frame update reads and
applies to players.

**This is the most likely leak source not yet investigated.** If the
sim's traversal of a speed portal mutates `gs.m_timeModRelated` (and not
just `m_speedObjects`), and our snapshot doesn't restore it, then the
engine's next-frame "compute current time-mod and apply to players"
path will read the sim-mutated value and write it to the real players.

Verify by adding both fields to capture/restore and rerunning the
reproducing level.

### #2 — Other unsnapshotted `GJGameState` fields
Our snapshot captures a hand-picked subset of GJGameState. The class has
~150 fields, many `m_unk*`. Speed-portal traversal could mutate any of
them. After exhausting `m_timeModRelated*`, the next move is to capture
the entire POD subset of GJGameState (snapshot a `memcpy` of the whole
struct, restore via `memcpy` back) and see whether the leak goes away.
If it does, bisect the fields to find the specific cache.

### #3 — A CCAction / CCSchedule running on `m_player1`
`m_somethingPlayerSpeedTime` field name suggests an interpolation
duration. If `PlayerObject::updateTimeMod` schedules a CCAction-style
speed transition (rather than a single instantaneous write), our
save/restore wrappers neutralize the instantaneous write — but the
scheduler keeps writing every tick afterwards.

Verify by disassembling `PlayerObject::updateTimeMod` (m1
`0x374e1c` per bindings line 14604) and looking for `runAction` /
`schedule` calls. If found, `stopAction` on sim-end is a possible fix.

### #4 — A per-tick lookup function that ignores `m_speedObjects`
The leak path is `GJBaseGameLayer::update + 0xe8`. Disassemble
`GJBaseGameLayer::update` (m1 `0x1229e8` per bindings line 7322) at
+0xe8 to identify the call. If it's a non-virtual helper that walks
level objects directly (not through `m_speedObjects`), the snapshot's
array restore is irrelevant to that recompute — we need to find the
*field* the helper actually reads.

## Hypothesis for new direction

The pattern that works for every non-leaking portal type is **suppress
the side-effect AT SOURCE** + **snapshot the per-tick physics fields**.
For speed portals, we've been doing the snapshot half but trying to let
the source (the engine's `updateTimeMod` fan-out) run in full and "save/
restore around it." The save/restore approach has not held — the engine
has a recompute path that bypasses everything we save and restore.

Two routes worth investigating before any more snapshot extensions:

### Route 1: Source-suppress the layer-level `updateTimeMod` fan-out, manually update sim only

In `TrajLayerSpeedHook::updateTimeMod`:
- During sim, **do not call super**.
- Instead, write `simP1->m_playerSpeed = speed` (and `simP2` if dual)
  directly. The engine's per-tick recompute on the sim player still
  needs to produce the correct ongoing speed — but that recompute reads
  `m_speedObjects`, which the snapshot already restores.

The risk: skipping super may also skip something `m_speedObjects`-side
(adding the portal to the array). If `m_speedObjects` mutation is in
`EffectGameObject::triggerObject` itself (not in `updateTimeMod`),
super-skip is safe. If `updateTimeMod` is what *adds to* `m_speedObjects`,
we'd skip the array-append and break sim physics.

To verify before taking this route: grep the engine binary / bindings
for what mutates `m_speedObjects` and at which call site.

### Route 2: Identify the actual leak site at `update + 0xe8` and gate it

The leak path's symbolicated frame says `GJBaseGameLayer::update + 0xe8`.
That's a specific instruction inside the binary `update` impl. Disassembly
(`otool -tV` on the binary, jump to `update`'s offset, locate +0xe8) will
show exactly which function the engine calls there. Likely candidates:

- `PlayerObject::updateTimeMod` direct call (we already see this in the
  stack — the question is *where the speed value comes from*).
- A non-virtual helper on `GJBaseGameLayer` like `getCurrentTimeMod()` or
  `recomputeSpeedForPlayer()` that walks `m_speedObjects` against the
  player's X.

If it's a non-virtual helper, hooking it is harder (Geode hooks virtuals).
But identifying it tells us what to snapshot.

### Route 3: Examine if a CCAction is at play

Check `PlayerObject::updateTimeMod`'s binary impl for any
`runAction` / `CCActionTween` / `schedule` calls. If `updateTimeMod`
schedules an ongoing transition (rather than just writing
`m_playerSpeed = speed`), then our save-and-restore wrappers neutralize
the instantaneous write but the scheduler keeps writing every tick
afterwards. Fix: cancel the scheduled action on sim player teardown,
or never let `updateTimeMod` run on the real player at all.

## What the diagnostic loop should produce

Concrete next-session steps to advance, in order:

1. Disassemble `GJBaseGameLayer::update` at the m1 offset (from bindings:
   `update` is on `GJBaseGameLayer`; need its address). Inspect what's
   at `+0xe8` — is it a direct call, an indirect virtual, an inline
   compute?
2. Inspect `PlayerObject::updateTimeMod`'s binary impl — does it
   `runAction` / `schedule`, or is it a straight field write?
3. Verify whether `m_speedObjects` array contents on the *real* layer at
   the moment of the leak match pre-sim state. If they don't, the
   snapshot's array-restore is wrong somehow (e.g. `removeAllObjects`
   not actually clearing because the array's retained elsewhere).
4. Only after one of those three points produces a concrete answer,
   pick a route from the hypothesis section.

## References

- `src/Hooks.cpp` — TrajPlayerObjectHook + TrajLayerSpeedHook (per-player
  gate + layer-level defense-in-depth save/restore)
- `src/Hooks.cpp` — TrajEffectHook::captureSpeedAndPropagateToSim
  (the actual shipping fix — capture real → propagate to sim → restore real)
- `src/Hooks.cpp` — playSpeedParticle source-suppression
- `src/Trajectory.cpp:32-139` — LayerStateSnapshot capture/restore
- `src/Trajectory.cpp:564, 647` — runPlan capture / restore call sites
- `src/Trajectory.cpp:486, 500` — runBranch capture / restore call sites
- Bindings: PlayerObject `m_playerSpeed` line 14818, `m_lastActivatedPortal`
  14834, `m_lastPortalPos` 14820, `m_playerSpeedAC` 14896,
  `m_somethingPlayerSpeedTime` 14895
- Bindings: GJBaseGameLayer `m_speedObjects` 15087,
  `updateTimeMod(speed, players, noEffects)` 7755
- `docs/lessons-learned/04-portal-physics-vs-effects.md` — the
  physics-vs-effects split for non-speed portals
