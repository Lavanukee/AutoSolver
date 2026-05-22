# GJGameState — field-by-field snapshot coverage audit

*GJGameState is `PlayLayer::m_gameState` — the per-level mutable state container holding camera, dual mode, gravity, level flip, time warp, portal markers, channel/tween/effect tables, and gameplay-area metadata. The sim mod's `LayerStateSnapshot` (src/Trajectory.cpp lines 34-227) uses a hybrid strategy: a POD-prefix memcpy captures every field up to (but not including) `m_spawnChannelRelated0`, the first non-trivial member; named fields after that boundary are captured explicitly. This audit verifies that every field is either covered by the memcpy or by an explicit capture, and flags every uncovered post-POD field as a leak candidate ranked by sim-impact. Field source: `bindings/bindings/Geode/binding_arm/GJGameState.hpp` (the generated header that the source actually compiles against — it includes `m_queuedTimeWarp` and `m_activatedObjectIDs`, which the older `.bro` at `_deps/bindings-src/bindings/2.208/GeometryDash.bro` lines 8789-8963 still labels as `m_unk18c` and `m_unkMapPairIntIntInt`).*

## Summary stats
- Total fields in GJGameState: 153
- Fields in POD prefix (covered by memcpy, before `m_spawnChannelRelated0`): 76
- The IS-boundary field (`m_spawnChannelRelated0` itself, a `gd::unordered_map`): not in the memcpy range, not explicitly captured — leak candidate
- Fields after POD prefix (post-`m_spawnChannelRelated0`): 76
  - Covered by explicit per-field capture in `LayerStateSnapshot`: 18
  - NOT covered (leak candidates): 58
    - High-impact: 6
    - Medium-impact: 10
    - Low-impact / write-once: 13
    - Inert (mutation suppressed by `TrajEffectHook` trigger gating): 29

## Fields table

Coverage values:
- `memcpy` — in POD prefix, restored by `std::memcpy(&gs, gameStatePodPrefix, kGameStatePodSize)`
- `explicit-capture` — captured by name in `LayerStateSnapshot::capture`
- `not-captured` — neither — leak candidate
- `intentionally-not-captured` — post-POD container that trigger-suppression keeps inert
- `unknown` — purpose unclear

| Line | Name | Type | Position vs `m_spawnChannelRelated0` | Coverage | Sim impact / notes |
|---|---|---|---|---|---|
| 89 | m_cameraZoom | float | pre-POD-boundary | memcpy + explicit-capture | Camera zoom, restored both ways (explicit overwritten by memcpy at end of restore) |
| 90 | m_targetCameraZoom | float | pre-POD-boundary | memcpy + explicit-capture | Target zoom (lerp target) |
| 91 | m_cameraOffset | CCPoint | pre-POD-boundary | memcpy + explicit-capture | Static camera offset |
| 92-120 | m_unkPoint1..29 | CCPoint | pre-POD-boundary | memcpy | 29 unknown points, all POD numerics; covered by prefix memcpy |
| 121 | m_unkBool1 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 122 | m_unkInt1 | int | pre-POD-boundary | memcpy | Unknown int |
| 123 | m_unkBool2 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 124 | m_unkInt2 | int | pre-POD-boundary | memcpy | Unknown int |
| 125 | m_unkBool3 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 126 | m_unkPoint30 | CCPoint | pre-POD-boundary | memcpy | Unknown point |
| 127 | m_middleGroundOffsetY | float | pre-POD-boundary | memcpy + explicit-capture | Middleground vertical offset; explicit-capture also handled because middleground node position needs separate restore |
| 128 | m_unkInt3 | int | pre-POD-boundary | memcpy | Unknown int |
| 129 | m_unkInt4 | int | pre-POD-boundary | memcpy | Unknown int |
| 130 | m_unkBool4 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 131 | m_unkBool5 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 132 | m_unkFloat2 | float | pre-POD-boundary | memcpy | Unknown float |
| 133 | m_unkFloat3 | float | pre-POD-boundary | memcpy | Unknown float |
| 134 | m_unkInt5 | int | pre-POD-boundary | memcpy | Unknown int |
| 135 | m_unkInt6 | int | pre-POD-boundary | memcpy | Unknown int |
| 136 | m_unkInt7 | int | pre-POD-boundary | memcpy | Unknown int |
| 137 | m_unkInt8 | int | pre-POD-boundary | memcpy | Unknown int |
| 138 | m_unkInt9 | int | pre-POD-boundary | memcpy | Unknown int |
| 139 | m_unkInt10 | int | pre-POD-boundary | memcpy | Unknown int |
| 140 | m_unkInt11 | int | pre-POD-boundary | memcpy | Unknown int |
| 141 | m_unkFloat4 | float | pre-POD-boundary | memcpy | Unknown float |
| 142 | m_unkUint1 | float | pre-POD-boundary | memcpy | Unknown (typed float, named uint) |
| 143 | m_portalY | float | pre-POD-boundary | memcpy + explicit-capture | Last-portal Y position; used by gravity/wave physics |
| 144 | m_unkBool6 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 145 | m_gravityRelated | bool | pre-POD-boundary | memcpy + explicit-capture | Gravity flag set by yellow gravity portals |
| 146 | m_unkInt12 | int | pre-POD-boundary | memcpy | Unknown int |
| 147 | m_unkInt13 | float | pre-POD-boundary | memcpy | Mistyped int (header says float — see binding bug) |
| 148 | m_unkInt14 | int | pre-POD-boundary | memcpy | Unknown int |
| 149 | m_unkInt15 | int | pre-POD-boundary | memcpy | Unknown int |
| 150 | m_unkBool7 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 151 | m_unkBool8 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 152 | m_unkBool9 | bool | pre-POD-boundary | memcpy | Unknown flag |
| 153 | m_unkFloat5 | float | pre-POD-boundary | memcpy | Unknown float |
| 154 | m_unkFloat6 | float | pre-POD-boundary | memcpy | Unknown float |
| 155 | m_unkFloat7 | float | pre-POD-boundary | memcpy | Unknown float |
| 156 | m_unkFloat8 | float | pre-POD-boundary | memcpy | Unknown float |
| 157 | m_cameraAngle | float | pre-POD-boundary | memcpy + explicit-capture | Current camera rotation |
| 158 | m_targetCameraAngle | float | pre-POD-boundary | memcpy + explicit-capture | Target camera rotation |
| 159 | m_playerStreakBlend | bool | pre-POD-boundary | memcpy | Player streak blend flag |
| 160 | m_timeWarp | float | pre-POD-boundary | memcpy + explicit-capture | Time-warp factor (slow-mo / fast-fwd modes) |
| 161 | m_queuedTimeWarp | float | pre-POD-boundary | memcpy + explicit-capture | Queued time-warp pending application |
| 162 | m_timeWarpRelated | float | pre-POD-boundary | memcpy + explicit-capture | Auxiliary time-warp scalar |
| 163 | m_currentChannel | int | pre-POD-boundary | memcpy + explicit-capture | Current spawn-trigger channel |
| 164 | m_rotateChannel | int | pre-POD-boundary | memcpy + explicit-capture | Rotate-action channel |
| **165** | **m_spawnChannelRelated0** | gd::unordered_map<int,int> | **IS-boundary** | **not-captured** | **Map of per-channel ints; mutated by spawn triggers. Trigger suppression makes this inert in practice.** |
| 166 | m_spawnChannelRelated1 | gd::unordered_map<int,bool> | post-POD-boundary | not-captured | Per-channel bools; mutated by spawn triggers. Inert with trigger gating |
| 167 | m_totalTime | double | post-POD-boundary | not-captured | **HIGH: Game-wide elapsed time, advanced every tick by `GJBaseGameLayer::update`** — if sim ticks advance this, real game sees wrong elapsed-time |
| 168 | m_levelTime | double | post-POD-boundary | not-captured | **HIGH: Level elapsed time, advanced every tick** — same risk; affects synced effects, song-trigger timestamps |
| 169 | m_unkDouble3 | double | post-POD-boundary | not-captured | Unknown double; likely time-related (sits with m_totalTime/m_levelTime) |
| 170 | m_commandIndex | unsigned int | post-POD-boundary | not-captured | **HIGH: Cursor into the level's command queue.** If sim advances this past portals/triggers, real game skips the same commands at next tick |
| 171 | m_unkUint3 | float | post-POD-boundary | not-captured | Unknown numeric (typed float, named uint) |
| 172 | m_currentProgress | unsigned int | post-POD-boundary | not-captured | Level progress counter; potentially advanced during sim |
| 173 | m_unkUint4 | int | post-POD-boundary | not-captured | Unknown int (typed int, named uint) |
| 174 | m_unkUint5 | int | post-POD-boundary | not-captured | Unknown int |
| 175 | m_unkUint6 | int | post-POD-boundary | not-captured | Unknown int |
| 176 | m_unkUint7 | int | post-POD-boundary | not-captured | Unknown int |
| 177 | m_unkUint8 | int | post-POD-boundary | not-captured | Unknown int |
| 178 | m_lastActivatedPortal1 | GameObject* | post-POD-boundary | explicit-capture | P1's last-touched portal pointer |
| 179 | m_lastActivatedPortal2 | GameObject* | post-POD-boundary | explicit-capture | P2's last-touched portal pointer |
| 180 | m_cameraPosition | CCPoint | post-POD-boundary | explicit-capture | Current camera position |
| 181 | m_unkBool10 | bool | post-POD-boundary | not-captured | Unknown flag |
| 182 | m_levelFlipping | float | post-POD-boundary | explicit-capture | Level-flip portal interpolation [0..1] |
| 183 | m_unkBool11 | bool | post-POD-boundary | not-captured | Unknown flag (likely flip-related, sits next to m_levelFlipping) |
| 184 | m_unkBool12 | bool | post-POD-boundary | not-captured | Unknown flag |
| 185 | m_isDualMode | bool | post-POD-boundary | explicit-capture | Dual-player active |
| 186 | m_unkFloat9 | float | post-POD-boundary | not-captured | Unknown float |
| 187 | m_tweenActions | gd::unordered_map<int,GJValueTween> | post-POD-boundary | intentionally-not-captured | Tween action map; trigger suppression prevents sim from registering new tweens (see Trajectory.cpp doc comment) |
| 188 | m_cameraEdgeValue0 | int | post-POD-boundary | explicit-capture | Camera edge trigger value 0 |
| 189 | m_cameraEdgeValue1 | int | post-POD-boundary | explicit-capture | Camera edge trigger value 1 |
| 190 | m_cameraEdgeValue2 | int | post-POD-boundary | explicit-capture | Camera edge trigger value 2 |
| 191 | m_cameraEdgeValue3 | int | post-POD-boundary | explicit-capture | Camera edge trigger value 3 |
| 192 | m_gameObjectPhysics | gd::unordered_map<int,GameObjectPhysics> | post-POD-boundary | intentionally-not-captured | Per-object physics overrides; only mutated by AdvancedFollow + physics triggers which are suppressed for sim |
| 193 | m_unkVecFloat1 | gd::vector<float> | post-POD-boundary | not-captured | Unknown float vector; unclear who mutates |
| 194 | m_unkUint10 | float | post-POD-boundary | not-captured | Unknown numeric |
| 195 | m_unkUint11 | int | post-POD-boundary | not-captured | Unknown int |
| 196 | m_unkUint12 | int | post-POD-boundary | not-captured | Unknown int |
| 197 | m_cameraStepDiff | CCPoint | post-POD-boundary | explicit-capture | Per-step camera delta |
| 198 | m_unkFloat10 | float | post-POD-boundary | not-captured | Unknown float |
| 199 | m_timeModRelated | float | post-POD-boundary | explicit-capture | **Per-tick time-mod cache; read every tick by GJBaseGameLayer::update +0xe8. Documented leak in src/Trajectory.cpp.** |
| 200 | m_timeModRelated2 | bool | post-POD-boundary | explicit-capture | Companion to m_timeModRelated |
| 201 | m_activatedObjectIDs | gd::map<pair<int,int>,int> | post-POD-boundary | not-captured | **MEDIUM: Object-activation registry.** Sim activating objects may register entries the real game then sees as already-activated |
| 202 | m_unkUint13 | float | post-POD-boundary | not-captured | Unknown numeric |
| 203 | m_unkPoint32 | CCPoint | post-POD-boundary | not-captured | Unknown point |
| 204 | m_cameraPosition2 | CCPoint | post-POD-boundary | explicit-capture | Editor-mode camera position (mirror of m_cameraPosition) |
| 205 | m_unkBool20 | bool | post-POD-boundary | not-captured | Unknown flag |
| 206 | m_unkBool21 | bool | post-POD-boundary | not-captured | Unknown flag |
| 207 | m_unkBool22 | bool | post-POD-boundary | not-captured | Unknown flag |
| 208 | m_unkUint14 | float | post-POD-boundary | not-captured | Unknown numeric |
| 209 | m_unkBool26 | bool | post-POD-boundary | not-captured | Unknown flag |
| 210 | m_cameraShakeEnabled | bool | post-POD-boundary | explicit-capture | Shake-trigger enabled |
| 211 | m_cameraShakeFactor | float | post-POD-boundary | explicit-capture | Shake intensity |
| 212 | m_unkUint15 | float | post-POD-boundary | not-captured | Unknown numeric |
| 213 | m_unkUint16 | float | post-POD-boundary | not-captured | Unknown numeric |
| 214 | m_unkUint64_1 | double | post-POD-boundary | not-captured | Unknown double; possibly a time/tick counter |
| 215 | m_unkPoint34 | CCPoint | post-POD-boundary | not-captured | Unknown point |
| 216 | m_dualRelated | unsigned int | post-POD-boundary | explicit-capture | Dual-player auxiliary state |
| 217 | m_stateObjects | gd::unordered_map<int,EnhancedGameObject*> | post-POD-boundary | intentionally-not-captured | State-object registry; trigger-driven |
| 218 | m_unkMapPairGJGameEventIntVectorEventTriggerInstance | gd::map<pair<GJGameEvent,int>,vector<EventTriggerInstance>> | post-POD-boundary | intentionally-not-captured | Event-trigger instance vectors; trigger-suppressed |
| 219 | m_unkMapPairGJGameEventIntInt | gd::map<pair<GJGameEvent,int>,int> | post-POD-boundary | intentionally-not-captured | Event-trigger int map; trigger-suppressed |
| 220 | m_enterEffectInstanceVectors | gd::unordered_map<int,vector<EnterEffectInstance>> | post-POD-boundary | intentionally-not-captured | Area-effect enter vectors; trigger-suppressed |
| 221 | m_exitEffectInstanceVectors | gd::unordered_map<int,vector<EnterEffectInstance>> | post-POD-boundary | intentionally-not-captured | Area-effect exit vectors; trigger-suppressed |
| 222 | m_enterChannelMap | gd::vector<int> | post-POD-boundary | intentionally-not-captured | Enter-effect channel map; trigger-suppressed |
| 223 | m_exitChannelMap | gd::vector<int> | post-POD-boundary | intentionally-not-captured | Exit-effect channel map; trigger-suppressed |
| 224 | m_moveEffectInstances | gd::vector<EnterEffectInstance> | post-POD-boundary | intentionally-not-captured | Move-effect instances; trigger-suppressed |
| 225 | m_rotateEffectInstances | gd::vector<EnterEffectInstance> | post-POD-boundary | intentionally-not-captured | Rotate-effect instances; trigger-suppressed |
| 226 | m_scaleEffectInstances | gd::vector<EnterEffectInstance> | post-POD-boundary | intentionally-not-captured | Scale-effect instances; trigger-suppressed |
| 227 | m_fadeEffectInstances | gd::vector<EnterEffectInstance> | post-POD-boundary | intentionally-not-captured | Fade-effect instances; trigger-suppressed |
| 228 | m_tintEffectInstances | gd::vector<EnterEffectInstance> | post-POD-boundary | intentionally-not-captured | Tint-effect instances; trigger-suppressed |
| 229 | m_unsortedAreaEffects | gd::unordered_set<int> | post-POD-boundary | intentionally-not-captured | Unsorted area effects; trigger-suppressed |
| 230 | m_unkBool27 | bool | post-POD-boundary | not-captured | Unknown flag |
| 231 | m_advanceFollowInstances | gd::vector<AdvancedFollowInstance> | post-POD-boundary | intentionally-not-captured | Advance-follow instances; trigger-suppressed |
| 232 | m_dynamicMoveActions | gd::vector<DynamicObjectAction> | post-POD-boundary | intentionally-not-captured | Dynamic-move actions; trigger-suppressed |
| 233 | m_dynamicRotateActions | gd::vector<DynamicObjectAction> | post-POD-boundary | intentionally-not-captured | Dynamic-rotate actions; trigger-suppressed |
| 234 | m_unkBool28 | bool | post-POD-boundary | not-captured | Unknown flag |
| 235 | m_unkBool29 | bool | post-POD-boundary | not-captured | Unknown flag |
| 236 | m_unkUint17 | float | post-POD-boundary | not-captured | Unknown numeric |
| 237 | m_unkUMap8 | gd::unordered_map<int,vector<int>> | post-POD-boundary | not-captured | Unknown map; likely trigger-driven |
| 238 | m_proximityVolumeRelated | gd::map<pair<int,int>,SFXTriggerInstance> | post-POD-boundary | intentionally-not-captured | SFX proximity volume; trigger-suppressed |
| 239 | m_songChannelStates | gd::unordered_map<int,SongChannelState> | post-POD-boundary | intentionally-not-captured | Song-channel states; trigger-suppressed |
| 240 | m_songTriggerStateVectors | gd::unordered_map<int,vector<SongTriggerState>> | post-POD-boundary | intentionally-not-captured | Song-trigger vectors; trigger-suppressed |
| 241 | m_sfxTriggerStates | gd::vector<SFXTriggerState> | post-POD-boundary | intentionally-not-captured | SFX-trigger states; trigger-suppressed |
| 242 | m_unkBool30 | bool | post-POD-boundary | not-captured | Unknown flag |
| 243 | m_background | int | post-POD-boundary | not-captured | LOW: Background ID; set once at level load |
| 244 | m_ground | int | post-POD-boundary | not-captured | LOW: Ground ID; set once at level load |
| 245 | m_middleground | int | post-POD-boundary | not-captured | LOW: Middleground ID; set once at level load (note: collision with the `PlayLayer::m_middleground` GJMGLayer pointer — these are different fields on different classes) |
| 246 | m_unkBool31 | bool | post-POD-boundary | not-captured | Unknown flag |
| 247 | m_points | int | post-POD-boundary | not-captured | LOW: Total points / score; not advanced by physics ticks |
| 248 | m_unkBool32 | bool | post-POD-boundary | not-captured | Unknown flag |
| 249 | m_pauseCounter | unsigned int | post-POD-boundary | not-captured | LOW: Pause counter; not advanced by sim ticks unless pause-trigger fires |
| 250 | m_pauseBufferTimer | unsigned int | post-POD-boundary | not-captured | LOW: Pause-buffer timer; same as above |

## Cross-reference with snapshot code

`LayerStateSnapshot::capture` and `restore` exist in `src/Trajectory.cpp`. Explicit field captures are at:

| Field | capture() line | restore() line | Coverage |
|---|---|---|---|
| m_cameraZoom | 120 | 167 | explicit + memcpy |
| m_targetCameraZoom | 121 | 168 | explicit + memcpy |
| m_cameraOffset | 122 | 169 | explicit + memcpy |
| m_cameraPosition | 123 | 170 | explicit, post-POD only |
| m_cameraPosition2 | 124 | 171 | explicit, post-POD only |
| m_cameraAngle | 125 | 172 | explicit + memcpy |
| m_targetCameraAngle | 126 | 173 | explicit + memcpy |
| m_cameraEdgeValue0..3 | 127-130 | 174-177 | explicit, post-POD only |
| m_cameraShakeEnabled | 131 | 178 | explicit, post-POD only |
| m_cameraShakeFactor | 132 | 179 | explicit, post-POD only |
| m_cameraStepDiff | 133 | 180 | explicit, post-POD only |
| m_isDualMode | 134 | 181 | explicit, post-POD only |
| m_dualRelated | 135 | 182 | explicit, post-POD only |
| m_levelFlipping | 136 | 183 | explicit, post-POD only |
| m_gravityRelated | 137 | 184 | explicit + memcpy |
| m_portalY | 138 | 185 | explicit + memcpy |
| m_lastActivatedPortal1 | 139 | 186 | explicit, post-POD only |
| m_lastActivatedPortal2 | 140 | 187 | explicit, post-POD only |
| m_timeWarp | 141 | 188 | explicit + memcpy |
| m_queuedTimeWarp | 142 | 189 | explicit + memcpy |
| m_timeWarpRelated | 143 | 190 | explicit + memcpy |
| m_currentChannel | 144 | 191 | explicit + memcpy |
| m_rotateChannel | 145 | 192 | explicit + memcpy |
| (m_speedObjects — on PlayLayer, not GJGameState) | 146-152 | 193-198 | explicit, separate object |
| (m_groundLayer position — on PlayLayer) | 153-158 | 205-216 | explicit, separate object |
| m_middleGroundOffsetY | 159 | 217 | explicit + memcpy |
| m_timeModRelated | 160 | 218 | explicit, post-POD only |
| m_timeModRelated2 | 161 | 219 | explicit, post-POD only |
| (POD-prefix memcpy) | 162 | 225 | bulk |

The explicit-capture list correctly covers 11 pre-POD-boundary fields redundantly (memcpy would also handle them) and 18 post-POD-boundary fields (which memcpy can't reach). The redundancy is harmless because the memcpy at the end of `restore()` overrides the per-field writes for pre-POD fields with identical values.

## Leak candidates section

Every post-POD field not explicitly captured. Ranked by sim-impact:

### High-impact leak candidates (read every tick, sim-mutable, untracked)

1. **m_totalTime (line 167, double)** — Per `GJBaseGameLayer::update`, this advances each tick. Sim running physics-step calls within `runBranch` likely advance this. Real game next tick reads a stale-skewed total time, which feeds song-trigger and event-trigger timestamps. **Mitigation: explicit capture/restore.**
2. **m_levelTime (line 168, double)** — Same risk as `m_totalTime`. Used by SFX/song triggers, area effects, advanced-follow timing. **Mitigation: explicit capture/restore.**
3. **m_commandIndex (line 170, unsigned int)** — Cursor into the level's per-tick command queue. If sim advances this past commands, real game skips those commands. **Mitigation: explicit capture/restore.** Strong candidate to investigate as the cause of the user's bookmarked speed-portal sim-side leak (project_bookmarked_bugs.md).
4. **m_currentProgress (line 172, unsigned int)** — Level progress counter; if it tracks player advancement (per object cross), sim advancing past objects mutates it. **Mitigation: explicit capture/restore.**
5. **m_unkUint3 (line 171, typed float)** — Sits between `m_commandIndex` and `m_currentProgress`; likely time/progress related given neighbors. Unknown semantics but very high suspicion of being read each tick. **Mitigation: explicit capture/restore as `float`.**
6. **m_unkUint64_1 (line 214, double)** — Double-sized field with no other doubles nearby; likely a high-precision time/tick counter. **Mitigation: explicit capture/restore as `double`.**

### Medium-impact leak candidates (numeric/flag, mutation source unclear)

7. **m_unkBool10 (line 181)** — Sits between `m_cameraPosition` and `m_levelFlipping`; could be camera or flip state.
8. **m_unkBool11, m_unkBool12 (lines 183-184)** — Sit between `m_levelFlipping` and `m_isDualMode`; likely flip/dual related.
9. **m_unkFloat9 (line 186)** — Sits right after `m_isDualMode`; possibly a dual-player parameter.
10. **m_unkUint4..8 (lines 173-177, all int)** — Five consecutive unknown ints; possibly indices into command lists or trigger queues.
11. **m_unkBool20..22, m_unkBool26..29 (lines 205-207, 209, 230, 234, 235)** — Various flags near `m_cameraShakeEnabled` and area-effect tables.
12. **m_unkUint10..17 (lines 194-196, 202, 208, 212, 213, 236)** — Eight unknown numerics scattered through the post-POD region.
13. **m_unkFloat10 (line 198)** — Sits between `m_cameraStepDiff` and `m_timeModRelated`; possibly camera-step related.
14. **m_unkPoint32, m_unkPoint34 (lines 203, 215)** — Two unknown points; could be auxiliary camera/portal positions.
15. **m_activatedObjectIDs (line 201, gd::map<pair<int,int>,int>)** — Object-activation registry. If sim activating-and-not-restoring entries lets the real game skip activation. Inert only if trigger-suppression handles this path.
16. **m_unkVecFloat1 (line 193, gd::vector<float>)** — Unknown float vector; unclear mutator.

### Low-impact leak candidates (write-once during setup)

17. **m_background (line 243, int)** — Background ID, set at level load. No sim mutation expected.
18. **m_ground (line 244, int)** — Ground ID, same.
19. **m_middleground (line 245, int)** — Middleground ID, same. (Different field from `PlayLayer::m_middleground` GJMGLayer* — that one is on a different class.)
20. **m_points (line 247, int)** — Score counter; sim doesn't award points.
21. **m_pauseCounter (line 249)** / **m_pauseBufferTimer (line 250)** — Pause state; sim doesn't pause.
22. **m_unkBool27, m_unkBool28, m_unkBool29, m_unkBool30, m_unkBool31, m_unkBool32 (lines 230, 234, 235, 242, 246, 248)** — Flags near setup-related fields; assumed write-once. Promote to medium if any sim run shows divergence.
23. **m_unkDouble3 (line 169)** — Sits with `m_totalTime`/`m_levelTime`; could be third-time accumulator, but only High-impact if read each tick.

### Inert (mutation suppressed by `TrajEffectHook` trigger gating, per src/Trajectory.cpp doc comment)

The following 29 fields are containers that the engine's trigger-processing routines mutate. The mod's `TrajEffectHook` (and related hooks) suppress trigger firing during sim, so sim never writes to these in practice. Listed for completeness; not flagged as leaks:

- m_spawnChannelRelated0 (line 165, **IS-boundary**)
- m_spawnChannelRelated1 (line 166)
- m_tweenActions (line 187)
- m_gameObjectPhysics (line 192)
- m_stateObjects (line 217)
- m_unkMapPairGJGameEventIntVectorEventTriggerInstance (line 218)
- m_unkMapPairGJGameEventIntInt (line 219)
- m_enterEffectInstanceVectors (line 220)
- m_exitEffectInstanceVectors (line 221)
- m_enterChannelMap (line 222)
- m_exitChannelMap (line 223)
- m_moveEffectInstances (line 224)
- m_rotateEffectInstances (line 225)
- m_scaleEffectInstances (line 226)
- m_fadeEffectInstances (line 227)
- m_tintEffectInstances (line 228)
- m_unsortedAreaEffects (line 229)
- m_advanceFollowInstances (line 231)
- m_dynamicMoveActions (line 232)
- m_dynamicRotateActions (line 233)
- m_proximityVolumeRelated (line 238)
- m_songChannelStates (line 239)
- m_songTriggerStateVectors (line 240)
- m_sfxTriggerStates (line 241)
- m_unkUMap8 (line 237) — likely trigger-driven, no other known mutators

Note: the "inert" classification is **only as good as the trigger-suppression coverage**. If any code path bypasses `TrajEffectHook` during a sim run (e.g., a portal mutates m_tweenActions directly), the container will leak. The user's bookmarked speed-portal sim-side bug (project_bookmarked_bugs.md) is consistent with such a bypass.

## How to extend isolation if a leak is real

For each high/medium leak candidate, the same three-step pattern applies. **Example for `m_totalTime`:**

1. Add field to the `LayerStateSnapshot` struct (near the `timeModRelated` block, lines 97-98):

       double totalTime{0.0};

2. Add to `capture()` (just before line 162's memcpy):

       totalTime = gs.m_totalTime;

3. Add to `restore()` (just before line 220's memcpy):

       gs.m_totalTime = totalTime;

Repeat for `m_levelTime`, `m_unkDouble3`, `m_commandIndex`, `m_currentProgress`, `m_unkUint3`, `m_unkUint64_1` (all POD-safe scalars — simple `=` works).

### Non-trivially-copyable fields requiring deep-copy

The following candidates are containers whose `=` operator only does pointer-copy semantics for the gd:: variants, and would require a deep-copy strategy if isolation is needed:

- **m_activatedObjectIDs** (`gd::map<pair<int,int>,int>`) — gd::map is std::map-like; assignment is deep, so `gs.m_activatedObjectIDs = activatedObjectIDsCopy;` would work, **but** capturing requires `activatedObjectIDsCopy = gs.m_activatedObjectIDs;` which allocates. Safe to do at runBranch scope; not safe to do every frame at high rate without profiling.
- **m_unkVecFloat1** (`gd::vector<float>`) — gd::vector is std::vector-like; deep-copy via `=` is fine for a `vector<float>` (POD payload).
- **m_unkUMap8** (`gd::unordered_map<int, gd::vector<int>>`) — deep-copy via `=` works (POD payloads), but allocates.

For the rest (the inert list), do NOT add explicit captures unless you also remove the trigger-suppression, because the engine's per-trigger logic assumes container invariants (e.g., a song-trigger registered in `m_songTriggerStateVectors` matches a song trigger having fired with side effects elsewhere). Snapshotting one without snapshotting the other would desync the engine.

### Recommended next debugging step

Add capture/restore for the **six High-impact fields** as a single batch and re-run the speed-portal sim-side and robot-mid-air-fresh-jump test cases (per project_bookmarked_bugs.md). If the speed-portal regression resolves, the leak was in `m_totalTime` / `m_levelTime` / `m_commandIndex` family. If not, escalate Medium-impact unknowns (especially `m_unkUint4..8`) into a similar batch.
