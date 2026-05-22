# Issue: Sim mis-survives solid-block side-impacts (false-survival)

## Symptom

The sim sometimes predicts that the player survives a collision with a
regular solid block when, in reality, the same input plan kills the player
at the moment of contact. The bot, trusting the sim's prediction, commits
the plan and the player dies on a path the sim believed was safe.

Two visible variants of the same underlying confusion:

1. **Headhitter-style mis-survival** — sim treats the block as if it were a
   headhitter (pass through side, bonk on top, continue). Path routes
   *into* the block from the side and exits the other side or rides along
   the bottom. Documented as a narrower case in
   [`issue-h-block-misclassification.md`](issue-h-block-misclassification.md).

2. **Side-hit "forced downward" mis-survival** — sim treats a side-impact
   as a glancing blow that pushes the player down (or up) past the block,
   continuing the path. Reality: inner hitbox overlaps the block's side,
   instant death.

The image attached to this report shows variant 2: the player's green
inner kill hitbox is clearly overlapping the upper-left interior of a
red-outlined regular block, the orange predicted path enters the block
region from the lower-left, bends, and continues to the right with
multiple input-transition action markers past the death point. The path
extends beyond the death — so this is **not** a precision issue with
input timing or path-following; the sim's model itself returned "alive"
where reality returned "dead".

## Why this is harder than it looks — the two-hitbox model

GD's player has **two distinct collision shapes**, and which one is
checked depends on what the player is colliding with:

- **Outer hitbox** (the larger, rotation-respecting yellow box visible in
  hitbox debug):
  - Triggers death on contact with a hazard (spike, sawblade, deadly trigger).
  - Used for top-contact / standing detection on solid blocks (you can
    "stand on" a block when only your outer hitbox is touching its top).
  - Tilts with the player's rotation.

- **Inner kill hitbox** (the smaller green box):
  - Triggers death on side-impact with a regular **solid (non-hazard)**
    block. This is the "you ran into a wall" check.
  - Does **not** trigger death from top-contact (so you can stand on
    blocks normally).

For a regular solid block, the engine's outcome depends on which hitbox
overlaps and how:

| Outer overlaps  | Inner overlaps  | Outcome                                     |
|-----------------|-----------------|---------------------------------------------|
| top edge        | no              | land/stand on top (no death)                |
| top edge        | side (any)      | death (you ran into the wall)               |
| side, falling   | no              | small clip-through allowed (rotation slack) |
| side, falling   | side            | death                                       |
| side, level     | side            | death                                       |

The engine's collision resolver runs side-impact-death **before** any
position-resolution pushback — so even if the engine *would* push you
down the side of a block, if your inner hitbox entered the side first,
you're dead before the pushback applies.

## Why it's non-deterministic (same block, sometimes survives, sometimes dies)

Reproducing the bug doesn't depend purely on the block — it depends on
the player's incoming state when the sim runs. Several plausible sources
of variability:

1. **Sim incoming-state drift**
   The bot scores many candidate plans per visual frame. Each plan starts
   from `copyAttributes(realPlayer)` plus our explicit-copy fields. If
   any rotation / sub-pixel-position / yVel / xVel field is even slightly
   off from reality (because `copyAttributes` skips a field, or because a
   prior sim's mutation wasn't fully restored), the inner-hitbox overlap
   test flips at the boundary case. Same block, different start state,
   different verdict.

2. **Plan-to-plan isolation gaps**
   If a sim run leaks state into the next sim run (a CCAction not
   stopped, a flag not restored), the second sim of the same block in
   the same visual frame may resolve differently from the first. The
   speed-portal leak (now fixed in 1.0.2) was an example of this kind of
   class — there could be other state leaks that affect collision
   resolution variability.

3. **Float precision near hitbox edges**
   Inner hitbox is small. At the moment of contact, an overlap by 0.001
   units one way is "inside" and the other way is "outside". Real player
   physics is single-precision, sim physics is single-precision — but
   small ordering differences (when does `update()` run vs.
   `checkCollisions()`) can produce different end-of-tick positions.

4. **Block-instance flags varying**
   Per the H-block doc: `m_isHeadHitter` (or analog) may be set on some
   blocks and not on visually-identical others, and the sim's collision
   path branches on that flag. If two adjacent blocks have different
   flags, sim succeeds on one and fails on the other — same visual, same
   approach.

## Hypotheses, ranked

### #1 — Sim's collision pass uses the wrong hitbox (outer where engine uses inner, or vice versa)
**Mechanism:** the sim calls `m_pl->checkCollisions(simPlayer, dt, false)`
— the same function the engine uses. So in principle the hitbox
selection should match. But if the *sim player* has a stale or wrong
hitbox-related field (rotation, scale, vehicle size, m_isOnGround,
m_isUpsideDown), the engine's hitbox calculation inside checkCollisions
returns a different shape than reality's, and the same overlap test
returns the opposite result.

**How to distinguish:** at the moment of mis-prediction, log
`sim->getObjectRect()` and `realPlayer->getObjectRect()` (outer) and
the inner kill rect (find the engine's accessor for inner hitbox) for
both. If they differ, hitbox-state is the issue. If they match,
hitbox-state is not the issue.

### #2 — Sim's incoming player state has a missed `copyAttributes` field
**Mechanism:** `copyAttributes` is the engine's own copy function, but
some fields are known not to be touched by it (we already explicitly
copy 25+ wave/slope fields per `runBranch`/`runPlan::initSim`). One of
the still-missed fields might be one the engine reads inside
`checkCollisions` to decide hitbox shape or kill-resolution path.

**How to distinguish:** when divergence is detected, dump every named
field on `PlayerObject` (sim vs. real). The first field where they
differ that's also referenced inside `checkCollisions` is the suspect.
The bindings list ~150 PlayerObject fields — most are easy to compare.

### #3 — Block-type misclassification per-instance (existing H-block doc)
**Mechanism:** `m_isHeadHitter` (or analog) is set on some block
instances and not on others. Sim's collision pass branches on this
flag. The "headhitter-style mis-survival" variant is consistent with
this hypothesis.

**How to distinguish:** at the moment of mis-prediction, log the block's
`m_objectType` and every per-instance flag. Compare to a same-type block
that the sim resolves correctly. If a flag differs, that's the property
the engine reads.

### #4 — Collision-resolution order mismatch (sim resolves position before checking death)
**Mechanism:** engine's `checkCollisions` does death-check first, then
position-resolution. If our sim's tick ordering inverts that (e.g., a
hook causes `update()` to run before `checkCollisions()` for the sim
player on a given tick), the sim's player gets pushed *out of* the kill
zone before the kill is checked, and survives a hit it should die on.

**How to distinguish:** the sim's tick body is in `Trajectory.cpp`
`runPlan` and `runBranch`. Both order checkCollisions BEFORE update —
which matches the engine. So if this is the issue, it's because the
engine's `checkCollisions` itself does both, and a single call's
internal order is what matters. Disassembly of `checkCollisions` may be
needed.

### #5 — Cull-window edge effects
**Mechanism:** sim's `collisionCheckObjects` filter
(`Hooks.cpp:127-191`) drops objects outside a 110-block window
(80 ahead + 30 behind). For wide objects whose origin sits behind the
back edge, we keep them via the right-edge test. But if a block is
borderline and we drop it on one tick and keep it on another, the sim
sees an obstacle one moment and doesn't the next.

**How to distinguish:** unlikely for blocks in the visible play area
(the player is much closer than 80 blocks away from anything they're
about to hit). Still worth ruling out by widening the cull window
temporarily and seeing if the bug rate changes.

## Status

**Diagnostic instrumentation added.** `Bot::recordRealDeath` (in
`src/Bot.cpp`, called from `BotPlayLayerHook::destroyPlayer` in
`src/BotHooks.cpp`) writes a tagged `# [FALSE-SURVIVAL P1] ...` line to
`divergence.log` whenever reality kills the real player on a frame the
bot's committed plan said the player would survive past. Lines tagged
`# [DEATH P1]` cover deaths that were also predicted by the plan —
useful as baseline reference rows for sim-vs-real comparison. Each row
captures: tick, plan idx, real position + outer rect, sim's predicted
position from `samples[idx+1]`, drift (d), real and predicted yVel,
rotation, scale, vehicle size, gravity mod, speed, all gamemode booleans,
upside-down, ground/slope/slide/ice flags, killer block id+type+rect,
and the local plan binary context.

To use: enable the bot, reproduce the bug, then `divergence.log` (at the
absolute path baked into `Bot.cpp` — the one we use for off-Geode-log
sessions) will have a tagged FALSE-SURVIVAL row. The full sim-side
state for the same tick is in the surrounding micro-drift rows the
detector already emits at 0.05-unit threshold.

What's still NOT captured (would be next round if the FALSE-SURVIVAL
row alone doesn't pin the cause):

- **Inner kill rect** — no public accessor for the engine's inner
  hitbox; the engine derives it inside `checkCollisions`. Outer rect is
  logged, which is enough for top-vs-side determination relative to the
  block's rect.
- **`checkCollisions` return value / per-side flags from the sim's run**
  — the sim's collision pass swallows the result inside
  `runPlan`/`runBranch`. Wiring it out would require an extra
  in-runPlan trace that's only kept for the winning candidate.

## Diagnostic plan

The path to a fix is "find the first sim-vs-real field that diverges at
the moment of mis-prediction." We already have a divergence detector
that logs sim/real position deltas when drift exceeds 0.5 units (1.0.0
changelog). Extend it to capture the full state needed to distinguish
the hypotheses above:

1. **Trigger condition:** when reality kills the player at a frame where
   the bot's committed plan has the player surviving past that frame —
   the simplest signal of a false-survival. This already happens per the
   image — the bot keeps holding its committed plan past the death
   point.

2. **Logged context** at the divergence frame:
   - Player real / sim positions, vels, rotation
   - Player real / sim outer + inner hitboxes (CCRect)
   - Block at impact: `m_objectType`, `m_isHeadHitter` (and any other
     classification flag worth checking from the bindings)
   - Player flags relevant to hitbox shape: `m_isMini`, `m_vehicleSize`,
     `m_isShip/m_isBall/m_isWave`/etc. (gamemode), `m_isUpsideDown`,
     `m_scale`
   - The collision result the sim got from `checkCollisions` (return
     value or per-side flags)

3. **Compare:** sim's logged state against real's logged state at the
   same frame. Walk the differences. If the player-state fields all
   match but the collision result differs, the issue is *inside*
   `checkCollisions` (or sub-functions). If a player-state field
   diverges, that's the missed copy.

4. **Instance comparison:** for the headhitter variant, log the same
   block's flags from BOTH a level where it's misclassified AND a level
   where it's classified correctly. The flag(s) that differ are the
   classification key.

## What to NOT do prematurely

- **Don't add explicit per-block-type overrides** in the sim's collision
  filtering. The engine is the source of truth; the sim should arrive at
  the correct result by feeding the engine the correct inputs. A
  hand-rolled classifier rots over time and won't generalize to new
  level mechanics.
- **Don't tighten the cull window** to "fix" non-determinism that's
  actually a state-leak issue. Smaller windows just hide the bug some of
  the time.
- **Don't add a "safety margin" to the kill check**. Same problem —
  hides the failure mode without fixing the cause.
- **Don't widen the divergence threshold** to suppress logs. We *want*
  the divergence detector to fire on these cases.

## References
- `src/Trajectory.cpp:421-502` — `runBranch` (per-frame visual prediction
  loop, has the full explicit-copy block)
- `src/Trajectory.cpp:504-649` — `runPlan` (bot scoring loop, same
  copy block in `initSim`)
- `src/Hooks.cpp:127-191` — `collisionCheckObjects` filter (the cull
  window)
- Bindings: `PlayerObject::getObjectRect` and any inner-rect accessor
  (`getInnerRect`?, search the bindings) — needed to log both hitboxes
- Bindings: `GameObject::m_objectType`, classification flags
- `docs/issue-h-block-misclassification.md` — narrower writeup of the
  headhitter-flag-mismatch variant
- `docs/issue-wave-ground-y-offset.md` — related class (sim incoming
  state slightly off from reality at ground contact)
