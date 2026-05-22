# Closed-Loop Bug Hunting

How Claude debugs Autosolver bugs end-to-end without you in the loop.
Reference this when starting a new session — the harness + telemetry +
build pipeline are reusable across any bug class.

## The loop

```
characterize → hypothesize → fix → build → test → measure → repeat
```

Each step has a concrete deliverable. The loop closes because the test
step produces machine-readable telemetry that informs the next characterize.

## What's wired

### 1. Test harness — `scripts/test-harness.sh`

Two modes:

- `./scripts/test-harness.sh [N]` — **launch mode** (default, plays for N seconds).
  Kills any running GD, launches fresh, force-fullscreens via `Cmd+Ctrl+F`,
  drives the menu via three calibrated mouse clicks + Space to open whichever
  level is in the "last played" slot on the main menu, captures one
  screenshot per second through the play window, extracts `[TEL]` lines from
  the live Geode log, then quits GD.
- `./scripts/test-harness.sh observe [N]` — **observe mode**. GD must already
  be running and on the level you want to test. The harness just captures
  screenshots + extracts `[TEL]` lines from the current log. Use this when
  the launch-mode click coordinates don't reach the level (custom levels,
  search results, etc.).

The level reached by launch mode is whatever the user last played and left
in the "last played" slot. To switch what launch mode tests, the user opens
the new level once, exits to main menu, then runs the harness — that level
becomes the new launch-mode target.

Outputs go to `/tmp/autosolver-test-YYYYMMDD-HHMMSS/`:
- `telemetry.log` — filtered `[TEL]` lines
- `geode-full.log` — full Geode log for grepping context
- `screenshot.png` — final reference shot
- `burst/NNN-HHMMSS.png` — one screenshot per second of the play window,
  timestamped so they correlate with telemetry timestamps

### 2. Mouse click via Swift — `scripts/click` (compiled from `click.swift`)

System Events `click at` uses macOS accessibility lookup and **does not work
for cocos2d games**. Click events have to go through `CGEventPost` against
the HID event tap. The `click` binary is a 50-line Swift program that does
exactly this. Rebuild with:

```
swiftc -O scripts/click.swift -o scripts/click
```

Calibrated for a 1500x1000 fullscreen GD window. Coordinates in
`scripts/test-harness.sh` (`COORD_*` variables) were recorded against the
user's setup; will need adjustment if click coords no longer land where
expected.

### 3. Mod telemetry — `src/Telemetry.cpp` / `src/Telemetry.hpp`

Structured log lines prefixed with `[TEL]` so the harness can grep them:

- `level_start id name attempts` — fires from `PlayLayer::setupHasCompleted`
- `level_reset attempt` — fires from `PlayLayer::resetLevel`
- `level_quit` — fires from `PlayLayer::onQuit`
- `real_death pos percent` — uses the LAST `real_pos` sample before the
  level resets, so it's robust against false `destroyPlayer` fires from
  anti-cheat bypass mods (Eclipse Menu intercepts the anticheat-spike kill,
  but the engine still calls `destroyPlayer` — a naive death log would
  capture that). Filtered to require the player to have moved past `x=5`.
- `real_pos pos yvel percent` — sampled per-physics-tick (240Hz),
  rate-limited inside the function to ~10/sec for log volume
- `win percent`
- `trigger_obj type who player_x trigger_x` and `trigger_act type who x_pos`
  — every `EffectGameObject::triggerObject`/`triggerActivated` call, with
  `who=sim|real` tag, rate-limited 1/30
- `divergence tick dist` — emitted from `Bot.cpp` whenever the bot's
  prediction drifts from reality by more than the configured threshold.
  Per-bot-instance rate-limited to 60-frame stride. **The single most
  diagnostic event** — paired with the contents of `divergence.log` at the
  project root (per-tick predicted vs actual position + every per-player
  state field), it tells you exactly when sim and real first disagreed and
  on which field.

### 4. Per-tick state log — `divergence.log` (project root)

Written by `Bot.cpp`'s divergence detector. Each time the bot finds a drift
> threshold, it dumps the prediction vs reality trace tick-by-tick for the
entire prediction window. Look for the FIRST tick with non-zero `d=` and
check which field diverged. Reading patterns:

- `pred=(X,Y) actual=(X,Y) d=(dx,dy)` — position prediction vs reality
- `yVel` (real) vs `simYVel` (predicted) — the most common divergence source
- `gnd=a/b/c/d` — `isOnGround / isOnGround2 / isOnGround3 / isOnGround4`
- `btnWant btnHeld jumpBuf gravPortal` — input + jump-buffer + portal state
- `gravity speed` — current gravity multiplier + speed modifier
- `plan=...[N]...` — slice of the bot's plan around the current tick;
  `[N]` is the active input at this tick

### 5. Backups — `backup-<descriptor>/`

`cp -R src/ backup-<descriptor>/` at any verified-working milestone. The
project has accumulated quite a few; `backup-checkpoint-sync-working/` is
the latest known-good baseline as of the PlayerCheckpoint sync landing.

## The loop in practice

### Step 1: Characterize

Run the harness, read telemetry. The signal you're looking for is **a
divergence event followed shortly by a `real_death` event**. That's a bug:
sim predicted survival, real died. The divergence tick + position tell you
WHERE the bug is in the level; the divergence.log tick-by-tick tells you
WHICH field diverged first.

If `real_death` fires WITHOUT a preceding divergence, the bot's plan was
actually bad (a known limitation — greedy local search can pick a path that
softlocks). Don't try to fix this with state-sync work; it's an algorithm
issue.

### Step 2: Hypothesize

From the divergence's first-differing field, hypothesize what state isn't
being copied/synced. Examples we've seen:

- `simYVel` constant for many ticks → sim's player has button-held state
  that real doesn't share, or vice versa
- Y divergence with X aligned → platform/obstacle at a different position
  in sim vs real (move trigger gap), OR sim/real disagree about whether
  player is on ground
- Sudden divergence at one tick → portal/mode transition not replicated
- Slowly-accumulating tiny divergence → physics integration step difference
  (usually a precision / order-of-operations issue)

### Step 3: Fix

Targeted change in `src/`. Build with `geode build` from project root.
Always test with the harness immediately after.

### Step 4: Build

```
geode build
```

The Geode CLI handles compile + .geode packaging + install. The mod
auto-installs to GD's mods directory. **You MUST close GD and relaunch
for the new build to load** — Geode doesn't hot-reload. The harness's
launch mode does this automatically; observe mode does NOT (you have to
quit/relaunch manually).

### Step 5: Test

```
./scripts/test-harness.sh 30
```

Default is launch mode, 30s play. Read the telemetry:

```
LATEST=$(ls -dt /tmp/autosolver-test-* | head -1)
grep -E "real_death|divergence|level_start" "$LATEST/telemetry.log"
```

### Step 6: Measure

Compare to baseline. The metrics that matter:

- **Furthest reached** — read `real_pos` lines, find max x. This is the
  closest thing to "how good is the bot."
- **Death positions + percent** — clusters indicate consistent bug spots.
- **Divergence count + max magnitude** — fewer + smaller is better.

If the change moved the needle (further reach, fewer deaths, smaller
divergence) → commit. If not → revert from backup, try a different
hypothesis.

### Step 7: Repeat

The next death position is the next bug. Go back to step 1.

## Picking a level to test on

The launch-mode click sequence opens "last played." To target a specific
level for closed-loop debugging, the user opens that level once in GD,
plays a few frames, exits to menu. Now launch-mode reaches that level.

Different levels surface different bug classes:
- Ship/wave/UFO sections → mode-physics divergences
- Trigger-heavy sections → effect manager state issues
- Slope-heavy sections → slope/collision state issues
- Ball mode → ball-specific physics (e.g. surface-snap quirks)

## What this loop CANNOT fix

- **Algorithm-level local minima.** If the bot's plan is short-sighted and
  walks into a softlock, that's a search-strategy bug, not a sim-fidelity
  bug. No amount of state sync fixes it. Telemetry will show a `real_death`
  without a preceding divergence — that's the signature.
- **Bugs that require actual gameplay interpretation.** If a level has a
  "click here to unlock" prompt mid-level, the bot has no way to handle
  that and telemetry won't catch it cleanly.

## Reference points

- Last known-good baseline: `backup-checkpoint-sync-working/` (after the
  PlayerCheckpoint state sync landed)
- Commit: `e463bbe PlayerCheckpoint state sync: bit-exact sim/real player state`
- Working test: Platinum Adventure 2 reached 15.34% with the harness +
  PlayerCheckpoint sync. Death at x=4315 is the next bug to characterize.

## Common pitfalls

- **Stale build.** `geode build` sometimes appears to succeed without
  actually rebuilding (caching quirk). If divergence numbers are
  byte-identical between two runs after a code change, force-touch:
  `touch src/Trajectory.cpp && geode build`. Verify the dylib's mtime
  changed: `ls -la build/jedd.trajectory.dylib`.
- **Anti-cheat false deaths.** Eclipse Menu intercepts the anticheat-spike
  kill but the engine still fires `destroyPlayer` with the anticheat-spike
  pointer as cause. The `real_death` telemetry uses level_reset as the
  death signal precisely to avoid this — but if you see `cause_obj_type=2
  anticheat=1` in the older `death` event format (pre-real_death), that's
  almost certainly NOT a real death.
- **Forgetting to relaunch GD.** Observe mode reads from a live session;
  changes to the mod don't take effect until GD restarts.
- **The harness times out at 25s default.** Many bugs only manifest after
  several attempt cycles. Use `./scripts/test-harness.sh 60` or higher when
  the bot dies early and you want to watch multiple respawn attempts.
