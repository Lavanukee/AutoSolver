# Sim Reference — index

Comprehensive sim/real-discrepancy reference for the `jedd.trajectory` Geometry Dash mod. Generated 2026-05-10 from an exhaustive audit of `_deps/bindings-src/bindings/2.208/GeometryDash.bro` (~1MB of binding declarations), the FMOD and Cocos2d binding files, and the current source in `src/`.

Use this directory as the canonical reference when:
- Debugging sim-vs-real divergence (which fields aren't copied? which methods aren't hooked?)
- Adding new isolation toggles (each audit doc has discriminator + code-sketch sections)
- Considering loosening isolation (each leak candidate lists the symptom-if-loosened)
- Onboarding new hook code (audit docs label each method's classification)

---

## Files

| File | Scope | Lines | Methods audited | Fields audited |
|---|---|---|---|---|
| [01-playerobject.md](01-playerobject.md) | `PlayerObject` exhaustive | 674 | 226 | 292 |
| [02-playlayer.md](02-playlayer.md) | `PlayLayer` (PlayLayer-direct, not inherited) | 314 | 130 | 99 |
| [03-gjbasegamelayer.md](03-gjbasegamelayer.md) | `GJBaseGameLayer` exhaustive | 778 | 426 | 287 |
| [04-gjgamestate.md](04-gjgamestate.md) | `GJGameState` (the snapshot target) | 299 | n/a | 153 |
| [05-playercheckpoint.md](05-playercheckpoint.md) | `PlayerCheckpoint` (engine's player-state struct) | 364 | n/a | 186 |
| [06-gameobject-base.md](06-gameobject-base.md) | `GameObject` + `EnhancedGameObject` + `EffectGameObject` | 828 | 204 | 296 |
| [07-trigger-subclasses.md](07-trigger-subclasses.md) | 42 trigger / specialized GameObject subclasses | 591 | per-subclass | per-subclass |
| [08-audio-fmod.md](08-audio-fmod.md) | `FMODAudioEngine` + every GD audio path | 459 | 143 | n/a |
| [09-cocos2d-tweening.md](09-cocos2d-tweening.md) | `CCAction` surface — every method that spawns a persistent tween | 496 | 71 | n/a |
| [10-current-isolation-map.md](10-current-isolation-map.md) | Cross-reference of every hook we have today | (synthesis) | n/a | n/a |
| [11-leak-candidates.md](11-leak-candidates.md) | Ranked list of every unisolated state surface | (synthesis) | n/a | n/a |

**Total raw audit content**: ~4800 lines across 9 source-audit files. ~1300 methods + ~1300 fields classified.

---

## Conventions used in audit tables

### Method classification labels

| Label | Meaning |
|---|---|
| `physics` / `physics-required` | Sim must call super; skipping breaks sim physics |
| `physics-conditional` | Sim must call super for sim player; real-player branch must suppress |
| `visual` | Visual-only side effect; safe to suppress for sim |
| `audio` | Audio side effect; suppress for sim |
| `state-mutating-self` / `state-mutating-player` | Writes own fields — fine for sim's own player, bad if writes to real |
| `state-mutating-layer` | Writes layer/game state; must be captured by snapshot or save-restored |
| `state-mutating-game` | Writes persistent game/level state — must suppress for sim |
| `persistent-stat` | Increments stat or saves to player progress |
| `trigger-dispatch` | Part of trigger fire path |
| `coin-collect` | Pickup/coin collection — must not fire for sim |
| `read-only` | Getter, no concern |
| `engine-internal` | Not a hook surface; engine-managed |
| `lifecycle` | Init/reset/quit; mostly hooked already |
| `editor-only` | Only fires in `LevelEditorLayer` context |
| `constructor` / `destructor` | C++ lifecycle |
| `unknown` | Name doesn't make purpose clear |

### Field classification labels

| Label | Meaning |
|---|---|
| `player-physics-state` | Needs to be copied real→sim at run start |
| `layer-game-state` | Should be in `LayerStateSnapshot` |
| `real-player-pointer` | Pointer aliasing real player state; sim must not overwrite |
| `shared-container` | CCArray/vector/map; sim writes would mutate real's view |
| `trigger-state` | Owned by trigger system (m_effectManager, m_groupNodes, etc.) |
| `level-data` | Immutable during play (no concern) |
| `ui-element` | Visual sub-node |
| `section-index` | Engine-recomputed per tick (no isolation needed) |
| `static-trigger-config` | Trigger's configured behavior; engine reads, doesn't write |
| `engine-internal` | Engine-managed |
| `unknown` | Name unclear |

### Coverage labels (GJGameState / PlayerCheckpoint)

| Label | Meaning |
|---|---|
| `memcpy` | Captured by POD-prefix memcpy in LayerStateSnapshot |
| `explicit-capture` | Captured by name in LayerStateSnapshot::capture |
| `not-captured` | Leak candidate |
| `intentionally-not-captured` | Post-POD container; trigger suppression prevents mutation |
| `via-checkpoint` | Covered by `PlayerObject::loadFromCheckpoint(PlayerCheckpoint*)` |

### Severity tags (leak-candidates.md)

| Tag | Meaning |
|---|---|
| `critical` | Production-blocking — real-game state corruption visible to user |
| `high` | Predicted-trajectory divergence; bot picks wrong path |
| `medium` | Edge-case divergence; only fires under specific level structure |
| `low` | Theoretical leak, no observed symptom |

---

## How to use this reference

### "I'm debugging a sim-physics divergence — which field might be uncopied?"
Start with [05-playercheckpoint.md](05-playercheckpoint.md) → cross-reference section. Lists fields in our explicit-copy list and the residual fields PlayerObject has but PlayerCheckpoint does not. Also see [01-playerobject.md](01-playerobject.md) leak-candidates section for `m_currentSlope`, `m_lastJumpTime`, etc.

### "Sim ran and now real game is glitched (camera moved, color changed, etc.)"
[04-gjgamestate.md](04-gjgamestate.md) lists every GJGameState field and whether it's in the snapshot. [03-gjbasegamelayer.md](03-gjbasegamelayer.md) lists layer-direct fields with the same coverage analysis.

### "I want to suppress audio that fires during sim probes"
[08-audio-fmod.md](08-audio-fmod.md) — recommendation is engine-level FMOD hook, code sketches included.

### "Visual leak that persists past sim end (tween still running)"
[09-cocos2d-tweening.md](09-cocos2d-tweening.md) — 15 currently-unhooked CCAction leak candidates, ranked.

### "I want to add per-family trigger control (let move triggers fire but not color)"
[07-trigger-subclasses.md](07-trigger-subclasses.md) — every trigger subclass with recommended treatment (suppress / save-restore / run-for-sim-only) + a cheap discriminator cheat sheet (`m_speedModType != 0`, `m_animationID != 0`, etc.) so per-family branches don't need `dynamic_cast`.

### "I'm reviewing what's already isolated"
[10-current-isolation-map.md](10-current-isolation-map.md) — every existing hook with file:line, what it does, which audit row(s) it covers.

### "What's the biggest unfixed risk right now?"
[11-leak-candidates.md](11-leak-candidates.md) — top-N ranked. As of 2026-05-10 the high-impact open items are: GJGameState post-POD scalars (`m_totalTime`/`m_levelTime`/`m_commandIndex`), `m_effectManager` on GJBaseGameLayer (entire trigger-state container), `m_collisionLog{Top,Bottom,Left,Right}` on PlayerObject (could phantom-poison real's collision dedupe), audio surface entirely unguarded except via trigger gate.

---

## Caveats & known gotchas

1. **Bindings file vs generated header drift.** The `.bro` source the audits read may lag the generated C++ headers we actually compile against (`build/_deps/bindings-src/bindings/include/Geode/...`). The GJGameState audit found at least one new field (`m_unkUint8`) and two renamed fields (`m_queuedTimeWarp`, `m_activatedObjectIDs`) that exist in the generated header but not the 2.208 `.bro`. If you're verifying against `src/Trajectory.cpp` compiles, trust the generated header. The audit docs note this where it matters.

2. **`copyAttributes` coverage is opaque.** We don't know exactly which fields the engine's own `PlayerObject::copyAttributes` carries. The audits assume our explicit copy list is for fields known to be missed by it (empirically determined). Any field NOT in our explicit list AND NOT in `PlayerCheckpoint` is in a gray zone — it may or may not be copied. Cross-reference [05-playercheckpoint.md](05-playercheckpoint.md) for the residual list.

3. **Method addresses ≠ source.** The bindings give us signatures + engine addresses but no method bodies. Classifications are best-effort inference from name + neighborhood. Treat `unknown` rows as "needs further investigation," not "safe to ignore."

4. **Inheritance not duplicated.** PlayLayer's audit ([02-playlayer.md](02-playlayer.md)) only lists PlayLayer-direct methods/fields. Inherited members are in [03-gjbasegamelayer.md](03-gjbasegamelayer.md). When investigating a method on PlayLayer that isn't in 02, check 03.

5. **Trigger subclass methods.** [07-trigger-subclasses.md](07-trigger-subclasses.md) covers per-subclass unique overrides. Members inherited from `EffectGameObject` / `EnhancedGameObject` / `GameObject` are in [06-gameobject-base.md](06-gameobject-base.md).

---

## Maintenance

When you add a hook, update [10-current-isolation-map.md](10-current-isolation-map.md). When you find a new leak in practice, add to [11-leak-candidates.md](11-leak-candidates.md). The per-class audit docs (01-09) are mostly static unless the bindings version changes.

If the bindings version updates (Geode 2.209+), the audits should be regenerated — the agent prompts that produced them are recorded in the session transcript at `/Users/jedd/.claude/projects/-Users-jedd-Desktop-GeometryDash-Autosolver/784aa673-c3d0-4c6a-8631-30bc7e3bd81a.jsonl`.
