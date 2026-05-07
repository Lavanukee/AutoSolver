# 1.0.0

First public release.

**Trajectory visualization**
- Per-frame hold/release prediction (green = hold, red = release) starting from the real player's current state.
- Configurable horizon, with optional accounting for pads, orbs, and portals.

**Autosolver bot**
- Searches user-authored input plans every visual frame; commits the plan that survives longest under simulated physics.
- Frame-independent input timing: injection happens PRE-super at the 240 Hz physics tick via `pushButton`/`releaseButton` directly on the player, bypassing the engine's button queue. Sim's `runPlan` ordering and reality's tick ordering match, so divergence reduces to state-copy completeness — not timing skew.
- Sim state isolation:
    - `LayerStateSnapshot` captures and restores the engine-mutated POD subset of `m_gameState` per `runPlan` and per `runBranch`.
    - Each sim player owns a private `m_touchingRings` `CCArray` so the sim's per-branch clear never touches the real player's array.
- Portal/effect split: physics state changes (gamemode, gravity, dual-mode, time-warp) apply to sim; particles, camera tweens, lightning flashes, screen-shake, and player flash are suppressed via direct hooks on the engine's particle/camera methods.
- Reentrancy: triple-sentinel guards (`m_simulating`, `m_inSearch`, `m_inSimulate`) plus `m_levelReady` gating prevent the search → runPlan → engine → search recursion that overflowed the stack on level entry.
- Divergence detector: logs sim-vs-reality position deltas with full physics state when drift exceeds 0.5 units, so divergence sources are debuggable from logs.
- Configurable search interval: drop search from 60 Hz to 30/15/7.5 Hz on heavy levels; input injection stays at 240 Hz.
- Spatial cull on sim collision pass: 80 blocks ahead, 30 behind. Wide objects with origin behind the window are kept via right-edge test.
- Diagnostic logging at orb activation: flags far-from-orb activations (sim-to-orb dist > 30 units) to localize false-hit sources.
- Dual-mode mirror: in single-player levels with a dual portal active, P2's input is mirrored from P1 each tick. 2P-mode (level-authored two-player) levels are excluded — independent P1/P2 plans for those is not yet supported.
- Optional debug visuals (dots / lines) showing input transitions on the chosen path.
- Toggleable via mod settings, F1, or the bottom-right pause-menu button.
