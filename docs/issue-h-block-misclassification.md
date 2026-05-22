# Issue: H-block misclassification across levels

## Symptom
Sim treats certain solid blocks as "headhitters" (H-blocks — pass-through-side,
bonk-on-top objects), so paths route THROUGH the block instead of dying on
contact.

## Evidence
- Level A (teal background, dark-blue gradient block): orange path passes
  THROUGH the block, bouncing off its top as if it's a headhitter. The player
  should clearly die on side-impact here.
- Level B (blue background, visually identical dark-blue gradient blocks): sim
  correctly classifies them as solid. Paths stop / route around.

Identical-looking objects (same gradient skin) are getting classified
differently across levels — so the misclassification is not purely a function
of the object's m_objectType / sprite.

## Likely root causes (to investigate)
- H-block detection uses a property that varies between identical-looking
  instances. Candidates:
  - GameObject flag (`m_isHeadHitter` or analog) set per-instance via
    object-edit settings, possibly editor-mode-only on level B.
  - Group/legacy block-ID classification path that varies by level.
  - `m_objectType` enum collision — some object types alias the same sprite
    skin between solid and headhitter variants.
- Collision-side detection error: sim correctly flags it as solid but its
  collision-side computation classifies the side-impact as a top-impact (would
  produce the "lands on top, keeps going" behavior visible in image #2).

## Where to look
- `src/Hooks.cpp::collisionCheckObjects` (filtering + delegation)
- Engine-side: GameObject hit-resolution path
  (`GJBaseGameLayer::collisionCheck*`, PlayerObject collision side
  computation)
- Bindings: `m_isHeadHitter` flag on GameObject (verify if real and
  per-instance), object-type enum scan for headhitter aliases of solid
  block types

## Diagnostic steps
1. Enable hitbox viz on a level where the bug reproduces; confirm the sim's
   hitbox AABB visually overlaps the block AABB at the moment the path
   continues.
2. Log `obj->m_objectType` and `obj->m_isHeadHitter` (if it exists) for the
   misclassified block at the moment of overlap to confirm which property is
   wrong.
3. Compare object instances between level A (broken) and level B (works) — if
   m_objectType matches but the per-instance flag differs, that's the answer.
