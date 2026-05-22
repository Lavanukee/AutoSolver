# Known issues

Index of every open or in-progress issue at HEAD (1.0.2). Each entry links
to a detailed writeup with hypotheses, diagnostic steps, and what NOT to do.

Status meanings:
- **open** — known symptom, root cause not yet diagnosed
- **in-progress** — investigation under way, hypotheses or diagnostics in code
- **deferred** — root cause not found after iteration; on hold per user

---

## 1. Speed-mod portal: sim-side propagation gap

**Symptom.** When the sim's predicted path crosses a speed-mod portal, the
sim player's `m_playerSpeed` does NOT update to the new portal value. The
sim continues at the pre-portal speed for the rest of the plan, so the
predicted path diverges from reality at the portal boundary (the bot
under-/over-shoots obstacles past the portal).

**Why it's hard.** Five propagation routes have been attempted and all
failed: (1) layer-level mirror via `TrajLayerSpeedHook::updateTimeMod`
(the layer hook is never reached with `players=true` during sim portal
crossings); (2) EffectGameObject capture-and-copy of real-player speed
delta (engine's super never writes the sim's `m_playerSpeed` directly,
and the per-player virtual gate blocks the engine's call to real, so
there's no delta); (3) direct `PlayerObject::updateTimeMod(newSpeed,
noEffects=true)` on each sim (clamps yVelocity and locks vertical motion
— sim trajectory goes flat horizontal at portal-entry height); (4) super
+ post-super direct write of `m_playerSpeed`/`m_playerSpeedAC` (super
has undocumented mutations that ALSO lock y-motion separately from
updateTimeMod); (5) skip-super + direct field write only (works
mechanically but flagged by user as a monkey patch — root cause of the
y-motion lock not understood).

**Status.** **deferred.** Per user instruction, no further iteration on
patch-style fixes until the engine's actual data flow for portal-driven
speed updates is mapped (which super-path mutates which sim field, on
which schedule).

**Doc.** [`issue-speed-portal-leak.md`](issue-speed-portal-leak.md)

---

## 2. Speed-mod portal: sim → real leak (residual)

**Symptom.** Per 1.0.2's own known-issues note, the SPEED CHANGE itself
still leaks to the real player when the sim crosses a speed portal —
visible as the real player accelerating/decelerating BEFORE actually
reaching the portal. 1.0.2 added `m_timeModRelated`/`m_timeModRelated2`
to `LayerStateSnapshot` (which fixed the layer-cache leak path), but a
second leak path through `m_playerSpeed` itself remained.

**Likely root cause (per 1.0.2 note).** The engine writes
`m_playerSpeed` through a path that bypasses the `PlayerObject::updateTimeMod`
virtual gate, so the per-player virtual override doesn't catch every
write. Investigation pending.

**Status.** **open.** Tightly coupled to issue #1 — the same root-cause
analysis that explains where speed-mod writes originate will inform both
the leak fix AND the sim-propagation fix.

**Doc.** [`issue-speed-portal-leak.md`](issue-speed-portal-leak.md)
(historical context); changelog 1.0.2 known issues.

---

## 3. Sim mis-survives solid-block side-impacts (false-survival)

**Symptom.** The sim sometimes predicts the player survives a collision
with a regular solid block when, in reality, the same input plan kills
the player at the moment of contact. Two visible variants: (a)
headhitter-style — sim treats the block as if it were a pass-through-side
headhitter (path enters from the side and continues out the other side
or rides along the bottom); (b) side-hit "forced downward" — sim treats
side-impact as a glancing blow that pushes the player past the block.
The bot trusts the prediction, commits the plan, and the player dies on
a path the sim believed was safe.

**Why it's non-deterministic.** Same block can survive in one plan and
die in another within the same visual frame — the sim's incoming state
(rotation, sub-pixel position, yVel, vehicleSize) at the moment of
contact decides which hitbox the engine's collision pass uses (outer
yellow vs inner green kill rect), and tiny state-copy gaps flip the
overlap test at the boundary case.

**Status.** **in-progress.** Diagnostic instrumentation in place:
`Bot::recordRealDeath` writes a tagged `# [FALSE-SURVIVAL P1] ...` line
to `divergence.log` whenever reality kills the real player on a frame
the bot's committed plan said the player would survive past. Surrounding
micro-drift rows from the 0.05-unit detector capture sim's full state
for the same tick. Awaiting a clean repro log to pin the diverging
field.

**Doc.** [`issue-block-collision-misclassification.md`](issue-block-collision-misclassification.md)

---

## 4. H-block misclassification across levels

**Symptom.** Identical-looking solid blocks (same gradient skin) are
classified differently across levels — sim treats them as headhitters
in level A but as solid in level B, or vice versa. Narrower than #3 (a
specific mechanism) but the visible behavior is the headhitter-variant
of #3's symptom.

**Likely root cause.** A per-instance flag (`m_isHeadHitter` or
analog) set via object-edit settings, possibly editor-mode-only on one
of the two levels. Because the misclassification is per-instance, sim's
collision branch lands on a different path than reality's for the same
visual sprite.

**Status.** **open.** Diagnosis path: log
`obj->m_objectType` and `obj->m_isHeadHitter` at the moment of overlap
on both a level where it reproduces and a level where it doesn't, then
compare the per-instance flags.

**Doc.** [`issue-h-block-misclassification.md`](issue-h-block-misclassification.md)

---

## 5. Wave-mode ground-Y offset (sim sits below reality when sliding)

**Symptom.** When the wave player is sliding on a surface (level
boundary / gameplay-area floor / D-block top), the sim's predicted Y
sits a few pixels LOWER than the real player's actual Y. Action-marker
dots appear embedded in the ground at sliding contact points; the
"doing nothing" wave's orange line sits in the ground vs. the real
player floating just above. Mid-air wave is fine — only the on-ground
sliding case shows the offset.

**Likely root causes (in priority order).** (1) `m_vehicleSize` mismatch
between sim and real (wave's non-cube hitbox bottom changes the rest-Y
by a constant offset); (2) wave-specific sliding flags not synced
(`m_isSliding`, `m_maybeSlopeForce`, `m_slopeAngle`); (3) ground-material
modifier (`m_groundObjectMaterial`); (4) `m_lastGroundedPos` /
`m_lastGroundObject`; (5) `m_stateOnGround`; (6) CCNode `m_obPosition`
vs `m_position` divergence post-`copyAttributes`. Note: 1.0.1 already
added explicit copies for several of these — verification of current
coverage is the next step.

**Status.** **in-progress.** Diagnosis path: per-tick log of the
candidate fields when the wave is sliding on ground, comparing sim's
value vs. reality's at the same tick.

**Doc.** [`issue-wave-ground-y-offset.md`](issue-wave-ground-y-offset.md)

---

## 6. Ball portal treated as wall on attempt 1 (self-heals attempt 2)

**Symptom.** On the FIRST attempt of a level containing a ball portal,
the bot's predicted (orange) path enters the portal region and behaves
as if the portal were a solid obstacle: path heads down through the
floor at the portal's X coordinate, then oscillates along the bottom of
the playable area for the rest of the prediction horizon. On attempt 2
of the same level (post-death/reset), the sim correctly transitions
into ball mode at the portal — predicted path shows the bouncing arcs
of ball-mode physics. No code change between attempts; the sim just
starts working.

**Strongest hypothesis.** Lazy sub-state initialization on the sim
`PlayerObject`. `createSimPlayer` builds the sim via
`PlayerObject::create(1, 1, pl, pl, true)` — pure cube defaults. The
engine's per-mode child sprites/state are constructed lazily on first
mode-toggle. When the sim crosses a ball portal mid-sim on attempt 1,
the engine's super invokes `toggleBallMode`, but a required child node
isn't present yet, so the toggle leaves the sim in a half-cube/
half-ball state. After attempt 1, the sub-state persists on the sim
PlayerObject (which is NOT recreated by `onPlayLayerReset`), so attempt
2 finds it ready.

**Status.** **open.** Cheap diagnostic: in `createSimPlayer`,
force-toggle every gamemode on/off after the create call to warm the
sub-state. If attempt 1 then matches attempt 2, hypothesis confirmed.

**Doc.** [`issue-ball-portal-first-attempt-wall.md`](issue-ball-portal-first-attempt-wall.md)

---

## Cross-cutting notes

- Issues #3 and #4 are likely the same underlying class — #4 is one of
  several specific mechanisms that produce #3's symptom. Fix to #4 may
  reduce but not eliminate #3 cases; #3's diagnostic instrumentation
  will catch both.
- Issues #1 and #2 share root-cause investigation; resolving the
  "where does the engine actually write `m_playerSpeed` for portal
  crossings" question informs both.
- Issues #5 and #6 are unrelated to each other and to the speed-portal
  cluster — they're independent state-completeness gaps in the sim
  setup path.
