# Issue: Wave-mode ground-Y offset between sim and reality

## Symptom
When the wave player is sliding on a surface (level boundary / gameplay-area
floor / D-block top), the sim's predicted Y is consistently a few pixels
LOWER than the real player's actual Y. Visible as:

- Action-marker dots (orange/green squares marking input-flip frames in the
  bot path) appear slightly embedded in the ground at sliding contact points,
  while the real player's hitbox sits cleanly on top of the surface.
- "Doing nothing" (no input) wave: orange line sits a little into the ground
  vs. the real player floating just above.
- Same dots positioned correctly when the wave is mid-air (e.g., between
  surfaces) — only the on-ground sliding case shows the offset.

## Where it shows up
- Level boundaries when gameplay-area-defined floor is the contact surface.
- D-blocks (dual-mode-active blocks the wave is allowed to slide on).
- Possibly correlates with the "alternating path inputs" pattern the bot
  emits to maintain altitude in wave — if Y is slightly off, the
  hold/release rhythm produces a slightly different trajectory that
  integrates drift.

## Why this is odd
Cube physics is perfectly precise (round-1 button-order fix verified). So the
basic state-copy + checkCollisions order is right. The wave-only nature of
this offset suggests a wave-specific field that copyAttributes skips OR a
hitbox/size mismatch that only matters when the player rests on a surface.

## Likely root causes (to investigate)
1. **m_vehicleSize not synced**: Wave has a non-cube vehicle size. If
   copyAttributes leaves the sim at a stale size from a prior candidate's
   gamemode (or default), the sim's hitbox-bottom Y differs from reality's
   by a consistent offset. PlayerObject.hpp:2278.
2. **m_isSliding / m_maybeSlopeForce / m_slopeAngle state**: Wave on a flat
   surface may set these wave-specific sliding flags; if not copied, sim
   resolves ground contact differently. PlayerObject.hpp:2331-2337.
3. **m_groundObjectMaterial**: Surface-material modifier (ice etc.) that
   could affect rest-Y if not synced. PlayerObject.hpp:2277.
4. **m_lastGroundedPos / m_lastGroundObject**: Wave's grip uses the last
   ground position as a Y reference; sim may inherit a stale value.
   PlayerObject.hpp:2117, 2292. (Currently copied for cube/ship via the
   round-2 fix; verify it's actually picked up for wave too.)
5. **m_stateOnGround int**: Categorized ground state. PlayerObject.hpp:2314.
6. **CCNode m_obPosition vs m_position**: PlayerObject has both setPosition
   (CCNode-virtual, line 90) and a `cocos2d::CCPoint m_position` field
   (line 2300). copyAttributes' coverage of these is unverified. If sim
   doesn't pick up the real CCNode position, every tick starts from the
   stale {0, 105} default plus drift.

## Diagnostic steps
1. Add a per-tick log in advanceFrame's divergence path that captures
   `base->m_vehicleSize` vs sim's, `base->m_isSliding` vs sim's,
   `m_groundObjectMaterial`, `m_stateOnGround` — at the moment the wave is
   sliding on ground.
2. Confirm whether copyAttributes copies CCNode position (compare
   sim->getPosition() to base->getPosition() right after copyAttributes
   returns, before any explicit-state-copy lines run).
3. If the offset is constant across ticks, it's almost certainly hitbox /
   size related — m_vehicleSize is the prime suspect.

## Fix approach (once diagnosed)
Same pattern as the round-2 jump-buffer fix and the (reverted) round-3
yVel-fields fix: add explicit copies in `initSim` and `runBranch` for
whichever wave-relevant field copyAttributes is missing. Bonus: also add
explicit `sim->setPosition(base->getPosition())` to guarantee CCNode
position is synced.
