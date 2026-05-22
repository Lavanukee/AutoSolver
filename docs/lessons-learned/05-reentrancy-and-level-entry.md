# Reentrancy and level entry

## The reentrancy loop

Without guards: `runSearch → runPlan → checkCollisions → engine updateCamera → runSearch → ...`. Each level deepens the call stack. A 2026-05-03 crash dump showed ~2780 frames deep on level entry. Stack overflow.

The cycle exists because:
- The sim shares the engine's `PlayLayer::checkCollisions`. The engine's collision pass for any tick fires trigger objects, camera-effect objects, and portals as the sim crosses them.
- Some of those activations call back into `updateCamera` (e.g. camera-tween triggers). Our `BotBGLHook::updateCamera` is the runSearch trigger.
- Without a guard, the inner updateCamera starts a new search inside the outer search.

## The three sentinels

| Sentinel | Owns what | Set true during |
|----------|-----------|-----------------|
| `m_simulating` (Trajectory) | Whether *any* sim is in progress (runPlan or runBranch) | `runPlan` body, `runBranch` body |
| `m_inSimulate` (Trajectory) | Whether `simulate()` itself is on the stack | `simulate()` body only |
| `m_inSearch` (Bot) | Whether `runSearch()` itself is on the stack | `runSearch()` body only |

`m_simulating` gates `runPlan` (so a runPlan inside an outer sim no-ops). `m_inSimulate` and `m_inSearch` are independent hard guards at the entry of their owning functions — even if `m_simulating` somehow gets out of sync, the function-level sentinels prevent recursion.

Each hook checks the relevant sentinel before triggering work. The most important is `BotBGLHook::updateCamera`: gated on `!sim().isSimulating()` so the inner-updateCamera-during-sim path early-returns.

## Level entry: `m_levelReady`

`PlayLayer::setupHasCompleted()` runs *during* `PlayLayer::init`, which itself processes the level's setup objects and *triggers an initial `updateCamera` tick* before init returns. Our `BotPlayLayerHook::setupHasCompleted` had to choose ordering carefully:

```cpp
void setupHasCompleted() {
    bot_().onPlayLayerInit(this);    // 1. fresh bot state for the new layer
    PlayLayer::setupHasCompleted();  // 2. engine setup (fires inner updateCamera)
    bot_().setLevelReady(true);      // 3. allow runSearch from the NEXT updateCamera
}
```

Without the levelReady gate, the engine's setup-tick `updateCamera` would call `runSearch` against a half-wired `PlayLayer` (object lists not finalized, player position uninitialized), with reproducible crashes from null/dangling pointers. The Trajectory simulator has its own `m_levelReady` for the same reason at the same point.

Reset/quit clear `m_levelReady` so the next level entry re-gates correctly.

## macOS heap reuse

Pointer equality between `PlayLayer*` instances is unreliable across level resets — macOS frequently lands the new instance at the same address as the freed prior one. The `bot.playLayer() == this` checks in hooks are a *necessary but not sufficient* guard against running search against a dead layer; the `onPlayLayerInit/Reset/Quit` calls have to forcibly reset `m_pl` to `nullptr` on quit so the address-equality guard isn't a false-positive against a freed layer.

## References

- `src/Bot.hpp::m_inSearch`, `m_levelReady`
- `src/Trajectory.hpp::m_inSimulate`, `m_levelReady`
- `src/BotHooks.cpp::setupHasCompleted` (init order)
- `src/Hooks.cpp::updateCamera` (runSearch reentrancy guard)
