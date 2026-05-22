# Frame independence

## The problem

Original input injection used `GJBaseGameLayer::handleButton(down, button, isPlayer1)` from a per-visual-frame hook. This routes the button through `m_queuedButtons`, which the engine drains *at the start of each physics tick* (240 Hz) before `checkCollisions`. A `handleButton` call made from inside the same tick — whether before or after super — misses that drain and lands one tick late.

Symptom: at speedhack 1 the bot played fine, but at lower visual frame rates (laggy levels) sim's "release at frame N" became real's "release at frame N+1, 2, ..." with the lag, and the bot's chosen plan stopped matching reality. The Bot.hpp commit memory has a 2026-05-03 entry showing a stack overflow in this regime.

## What we ended up doing

Moved injection to `BotPlayerObjectHook::update`, PRE-super, calling `pushButton`/`releaseButton` directly on `m_player1` (not via `handleButton`). Both functions mutate `PlayerObject::m_holdingButtons` in place — the same field `checkCollisions` reads on the same tick. `m_frame` is incremented post-super so it always names the tick that just ran.

This matches the simulator's `runPlan` ordering exactly: `setButton(plan[i]) → checkCollisions(sim) → sim->update(dt)`. Sim and reality run the same sequence on the same tick, so divergence reduces to state-copy completeness (the D2 audit), not timing.

## Why not the queue

The engine's queue has its own bookkeeping (`m_queuedRecordedButtons`, `m_queuedReplayButtons`) that we'd have to mirror cleanly to avoid corrupting real-game replay/record state. `pushButton`/`releaseButton` skip all of that and just touch the player's per-button hold state. Sim already uses `setButton` (its private equivalent of these). Symmetry over machinery.

## What this opens up

Once injection is timing-correct, "the bot dies on a stair" stops being a timing argument and becomes a state-copy argument. See [05](05-reentrancy-and-level-entry.md) for the parallel reentrancy story; the staircase root-cause hypothesis (m_jumpBuffered not in copyAttributes) lives in `bindings-notes/README.md` finding 2.

## References

- `src/BotHooks.cpp:98-120` (BotPlayerObjectHook::update)
- `src/Bot.cpp:advanceFrame` (post-tick counter increment + divergence detector)
- Bindings: `PlayerObject::pushButton`/`releaseButton`, `m_holdingButtons`
- Bindings: `GJBaseGameLayer::m_queuedButtons` (G-L survey L4278)
