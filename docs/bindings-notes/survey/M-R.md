# Bindings Survey: M-R

Total files in range: 67. Reviewed: 67.

Note on line numbers: every binding header in `bindings/Geode/binding/` is a 9-line platform shim that includes `binding_arm/<File>.hpp` (ARM Mac) or `binding_intel/<File>.hpp` (Intel). All `binding_path:line` references below point at the ARM file (`bindings/Geode/binding_arm/<File>.hpp`); the Intel layout is field-equivalent.

---

## Critical files

### `PlayerObject.hpp` — **[HEAVY READ]**
ARM file: `bindings/Geode/binding_arm/PlayerObject.hpp` (2380 lines). Inherits `GameObject, AnimatedSpriteDelegate`. The fields block runs L2088-L2379 (exposed publicly, no protected/private split). Every field is up for grabs from `m_simP1`/`m_simP2`.

#### Physics state fields (problem #3 — divergence checklist)
- `bool m_wasTeleported` at L2089
- `bool m_fixGravityBug` at L2090
- `bool m_reverseSync` at L2091
- `double m_yVelocityBeforeSlope` at L2092
- `double m_dashX` at L2093, `double m_dashY` at L2094, `double m_dashAngle` at L2095, `double m_dashStartTime` at L2096
- `DashRingObject* m_dashRing` at L2097 — **[INTEREST C4/orb-false-hit]** owned-pointer to last activated dash orb
- `double m_slopeStartTime` at L2098
- `bool m_justPlacedStreak` at L2099
- collision logs L2100-L2108 (`m_lastCollisionBottom/Top/Left/Right`, `m_collisionLogTop/Bottom/Left/Right`)
- `GameObject* m_currentSlope2` at L2111, `m_preLastGroundObject` at L2112, `m_lastGroundObject` at L2117, `m_collidedObject` at L2116, `m_collidingWithLeft` at L2118, `m_collidingWithRight` at L2119
- `float m_slopeAngle` at L2113, `bool m_slopeSlidingMaybeRotated` at L2114
- `double m_groundYVelocity` at L2122, `double m_yVelocityRelated` at L2123
- `bool m_isCollidingWithSlope` at L2127, `m_collidingWithSlopeId` at L2134, `m_currentPotentialSlope` at L2131, `m_currentSlope` at L2132, `m_slopeFlipGravityRelated` at L2135, `m_slopeAngleRadians` at L2137
- `float m_rotationSpeed` at L2140, `m_rotateSpeed` at L2141, `m_isRotating` at L2142, `m_isBallRotating` at L2129, `m_isBallRotating2` at L2143
- `double m_speedMultiplier` at L2164, `m_yStart` at L2165, `m_gravity` at L2166
- `double m_gameModeChangedTime` at L2169
- `double m_lastJumpTime` at L2181, `m_lastFlipTime` at L2182, `m_lastSpiderFlipTime` at L2188
- `double m_accelerationOrSpeed` at L2193, `m_snapDistance` at L2194
- `GameObject* m_objectSnappedTo` at L2197 — **[INTEREST C6/staircase]** the object snap-jumped onto last frame; jump-on-land buffer reads this
- `double m_slopeRotation` at L2222, `m_currentSlopeYVelocity` at L2223
- `double m_blackOrbRelated` at L2225
- `bool m_isAccelerating` at L2228, `m_isCurrentSlopeTop` at L2229
- four collision-extent floats L2230-L2233 (`m_collidedTopMinY` / `BottomMaxY` / `LeftMaxX` / `RightMinX`)
- **`double m_yVelocity` at L2253** — primary vertical-velocity state, the one `getYVelocity()`/`setYVelocity()` touch
- `double m_fallSpeed` at L2254 — separate falling-speed cache; gravity portal mechanics may write here distinct from yVelocity
- `bool m_isOnSlope` at L2255, `m_wasOnSlope` at L2256, `float m_slopeVelocity` at L2257, `m_maybeUpsideDownSlope` at L2258
- **`bool m_isOnGround` at L2267** — primary on-ground bool; cross-ref Trajectory.cpp says `copyAttributes` does NOT copy this. **[INTEREST C3/divergence-fix]**
- `bool m_isOnGround2` at L2283, `m_isOnGround3` at L2178, `m_isOnGround4` at L2335 — duplicate ground caches; suspect at least one tracks delayed/post-collision state
- `bool m_isUpsideDown` at L2265, `m_isDead` at L2266, `m_isGoingLeft` at L2268, `m_isSideways` at L2269
- `int m_reverseRelated` at L2271, `m_maybeReverseSpeed` at L2272, `m_maybeReverseAcceleration` at L2273
- `float m_xVelocityRelated2` at L2274, `m_xVelocityRelated` at L2329, `m_yVelocityRelated3` at L2307
- `bool m_isDashing` at L2275, `int m_dashFireFrame` at L2276
- `int m_groundObjectMaterial` at L2277
- `float m_vehicleSize` at L2278, **`float m_playerSpeed` at L2279** — multiplier (1x, 2x, 3x, 4x speed portals)
- `cocos2d::CCPoint m_shipRotation` at L2280
- `cocos2d::CCPoint m_lastPortalPos` at L2281 — **[INTEREST C2/camera-leak]** portal position cache; could leak if sim crosses one
- `double m_lastLandTime` at L2284, `float m_platformerVelocityRelated` at L2285
- `bool m_maybeIsBoosted` at L2286, `m_decreaseBoostSlide` at L2288, `m_isLocked` at L2290, `m_controlsDisabled` at L2291
- `cocos2d::CCPoint m_lastGroundedPos` at L2292
- **`float m_gravityMod` at L2351** — gravity multiplier; cross-ref Trajectory.cpp `runPlan` initSim explicitly sets `sim->m_gravityMod = base->m_gravityMod` because copyAttributes misses it. **[INTEREST C3/divergence]**
- `int m_stateOnGround` at L2314, `unsigned char m_stateUnk` at L2315, `m_stateNoStickX/Y` at L2316/L2317, `m_stateUnk2` at L2318
- `int m_stateBoostX` at L2319, `m_stateBoostY` at L2320, `m_maybeStateForce2` at L2321, `m_stateScale` at L2322
- `double m_platformerXVelocity` at L2323
- `double m_scaleXRelated` at L2327, `m_scaleXRelatedTime` at L2287, `m_scaleXRelated2..5` at L2121,L2124,L2125,L2126
- `bool m_maybeHasStopped` at L2328, `m_maybeGoingCorrectSlopeDirection` at L2330, `m_isSliding` at L2331, `m_maybeSlopeForce` at L2332, `m_isOnIce` at L2333, `m_physDeltaRelated` at L2334
- `int m_maybeSlidingTime` at L2336, `double m_maybeSlidingStartTime` at L2337, `m_changedDirectionsTime` at L2338, `m_slopeEndTime` at L2339
- `bool m_isMoving` at L2340, `m_platformerMovingLeft/Right` at L2341/L2342, `m_isSlidingRight` at L2343
- `double m_maybeChangedDirectionAngle` at L2344
- `bool m_isPlatformer` at L2346
- `int m_stateNoAutoJump` at L2347, `m_stateDartSlide` at L2348, `m_stateHitHead` at L2349, `m_stateFlipGravity` at L2350
- `int m_stateForce` at L2352, `cocos2d::CCPoint m_stateForceVector` at L2353, `bool m_affectedByForces` at L2354
- `float m_somethingPlayerSpeedTime` at L2356, `m_playerSpeedAC` at L2357, `m_fixRobotJump` at L2358
- `float m_fallStartY` at L2373

#### Gamemode-specific bools (every "is<Mode>" flag — **[INTEREST C3]** every one of these must be in copyAttributes for sim mode-switches to be correct)
- `bool m_isShip` at L2259
- `bool m_isBird` at L2260 (UFO)
- `bool m_isBall` at L2261
- `bool m_isDart` at L2262 (wave)
- `bool m_isRobot` at L2263
- `bool m_isSpider` at L2264
- `bool m_isSwing` at L2270
- `bool m_isUpsideDown` at L2265 (gravity)
- Toggling methods: `toggleBirdMode` L1499, `toggleDartMode` L1508, `toggleFlyMode` L1517 (ship), `togglePlatformerMode` L1535, `togglePlayerScale` L1544 (mini), `toggleRobotMode` L1553, `toggleRollMode` L1562 (ball), `toggleSpiderMode` L1571, `toggleSwingMode` L1580 — these toggles call `modeDidChange` (L810) and `switchedToMode` (L1481) which mutate sprite state and add child nodes. **[INTEREST C2/camera-leak]** if these run during sim, the parent layer gets sprite re-parents.
- `flipGravity(bool flip, bool noEffects)` at L495 — toggling produces a `m_gravityMod` flip plus particle effect. Already gated by sim hooks but worth noting the second arg is the noEffects toggle the bot can pass.

#### Ring/orb-related fields (problem #4 — orb-false-hit checklist)
- `cocos2d::CCArray* m_touchingRings` at L2293 — array of rings overlapping this player frame. Cross-ref `Trajectory.cpp clearSimRingState` L312-319 isolates this with `m_simP1OwnRings`/`m_simP2OwnRings` so the sim's removeAllObjects call never clobbers the real player's array. Already handled.
- `gd::unordered_set<int> m_touchedRings` at L2294 — IDs of rings activated this run. Cleared in `clearSimRingState` L320. Already handled.
- `gd::unordered_set<int> m_ringRelatedSet` at L2196 — appears to be a parallel ring tracker. Cleared in `clearSimRingState` L301. Already handled.
- `bool m_padRingRelated` at L2170 — pad/ring activation marker. Cleared L299.
- `bool m_ringJumpRelated` at L2195 — Cleared L300.
- `bool m_stateRingJump` at L2242, `m_stateRingJump2` at L2246 — Cleared L302/L303.
- `bool m_touchedRing` at L2247 — Cleared L304.
- `bool m_touchedCustomRing` at L2248 — Cleared L305.
- `bool m_touchedGravityPortal` at L2249 — **[INTEREST C2/C4 — NOT cleared in clearSimRingState]** if a gravity portal got crossed during a sim branch this stays true. Should be cleared analogously.
- `bool m_maybeTouchedBreakableBlock` at L2250 — **[INTEREST C4 — NOT cleared in clearSimRingState]** breakable block touch tracker; not a ring per se, but in the same activation-flag family.
- `geode::SeedValueRSV m_jumpRelatedAC2` at L2251 — opaque, but value may carry orb metadata. **[INTEREST C4 — NOT cleared in clearSimRingState]** worth a defensive clear.
- `bool m_touchedPad` at L2252 — **[INTEREST C4 — NOT cleared in clearSimRingState]** jump-pad touch. Pads are functionally separate from rings but the sim should reset them too for problem #4 false-pad-hits (analogous bug class).
- `gd::map<int, bool> m_jumpPadRelated` at L2355 — keyed by pad ID. Cleared in `clearSimRingState` L321.
- `bool m_hasEverHitRing` at L2297 — lifetime "have ever hit a ring" flag; **[NOT cleared]** but probably correct to leave alone since it's lifetime-scoped.
- `DashRingObject* m_dashRing` at L2097 — Cleared L286.
- `m_isDashing/m_dashX/Y/m_dashAngle/m_dashStartTime` at L2275, L2093-L2096 — Cleared L293-L297.
- `bool m_jumpBuffered` at L2241, `m_wasJumpBuffered` at L2243, `m_wasRobotJump` at L2244, `unsigned char m_stateJumpBuffered` at L2245 — **[INTEREST C6/staircase]** these are the jump-on-land buffer state. None are touched by `clearSimRingState`, but they should likely be re-derived from base via copyAttributes; if the engine's copyAttributes skips them (parallel to `m_isOnGround`), staircase failures could trace here. Recommend explicit copy in `initSim`.
- `bool m_hasEverJumped` at L2296 — lifetime flag.

Method-level ring/orb hooks worth knowing:
- `addToTouchedRings(RingObject*)` at L216 — appends to `m_touchingRings`.
- `resetTouchedRings(bool removeAll)` at L1134 — engine-side reset method; might be safer to call this than clearing fields manually.
- `ringJump(RingObject* object, bool skipCheck)` at L1161 — actually applies the ring-jump impulse.
- `startDashing(DashRingObject*)` at L1400, `stopDashing()` at L1418.

#### Button state (input mirroring — problem #3)
- `bool pushButton(PlayerButton button)` at L990 — used heavily by sim.
- `bool releaseButton(PlayerButton button)` at L1044 — used heavily by sim.
- `void releaseAllButtons()` at L1035 — useful between sim branches.
- `bool buttonDown(PlayerButton button)` at L261 — query.
- `bool switchedDirTo(PlayerButton button)` at L1472 — direction-change query (platformer).
- `gd::map<int, bool> m_holdingButtons` at L2359 — actual button-held state map (key is PlayerButton enum int). **[INTEREST C3/divergence]** if the layer's "real player held button" isn't mirrored to sim before runPlan, the very first physics tick on the sim sees the wrong input; this is the layer-side button-state mirror Trajectory.cpp suspects.
- `bool m_inputsLocked` at L2360
- `bool m_holdingRight` at L2324, `m_holdingLeft` at L2325, `m_leftPressedFirst` at L2326 — platformer.
- `bool m_isLocked` at L2290, `m_controlsDisabled` at L2291 — gates inputs.

#### copyAttributes
- `void copyAttributes(PlayerObject* player)` at L342. Implementation is opaque (game-side, not in headers). Confirmed by Trajectory.cpp src note that it copies *most* fields but specifically misses `m_gravityMod` (manually copied L399) and `m_isOnGround` (manually copied L400). Given the field count (290+), there are likely OTHER misses. Based on `clearSimRingState` patching everything ring-shaped after a copyAttributes, the assumption seems to be "copyAttributes copies them, but stale ring data leaks" — meaning copyAttributes DOES copy `m_touchingRings`, `m_touchedRings`, `m_ringRelatedSet`, `m_padRingRelated`, `m_ringJumpRelated`, `m_stateRingJump/2`, `m_touchedRing`, `m_touchedCustomRing`, `m_jumpPadRelated` but NOT in a useful way for sim purposes. The empirical pattern is "copyAttributes catches almost everything; for fields where the sim wants a clean slate (rings) we override; for fields where the engine forgets (gravityMod/isOnGround) we override; investigate other engine-forgets candidates: `m_jumpBuffered` family (L2241-L2245), `m_holdingButtons` (L2359), `m_isOnGround2/3/4` (L2283/L2178/L2335), `m_lastPortalPos` (L2281), `m_touchedGravityPortal` (L2249)."

#### flashPlayer
- `void flashPlayer(float flashDuration, float flashDelay, ccColor3B mainColor, ccColor3B secondColor)` at L486 — **[INTEREST C1/portal-particles, C2/camera-leak]** Used internally by mode-switch toggles (`toggleBirdMode`, `toggleDartMode`, etc.) to play the white-flash overlay on the screen. Stores into `m_flashTime` (L2183), `m_flashDuration` (L2184), `m_flashDelay` (L2185), `m_flashMainColor` (L2186), `m_flashSecondColor` (L2187). When sim crosses a portal these fields will mutate and the engine then renders the flash on the next visit; sim mode-switches need to either gate calls into this or restore the fields post-sim via the `LayerStateSnapshot` pattern already used for the layer.
- Particle/effect-related methods to consider gating during sim: `playSpawnEffect()` L936, `spawnPortalCircle(ccColor3B, float)` L1323, `spawnDualCircle()` L1305, `spawnScaleCircle()` L1332, `spawnCircle()` L1287, `playBumpEffect(...)` L837, `playBurstEffect()` L846, `playDeathEffect()` L864, `addAllParticles()` L207, `removeAllParticles()` L1053, `resetAllParticles()` L1080, `stopParticles()` L1427, `deactivateParticle()` L378, `activateStreak()` L198, `placeStreakPoint()` L828.

#### Particle pointer fields (problem #1 — portal particles still firing)
- `cocos2d::CCArray* m_particleSystems` at L2136 — ALL particle systems on the player; iterating and pausing this list during sim-mode would gate problem #1 globally.
- `m_playerGroundParticles` (L2203), `m_trailingParticles` (L2204), `m_shipClickParticles` (L2205), `m_vehicleGroundParticles` (L2206), `m_ufoClickParticles` (L2207), `m_robotBurstParticles` (L2208), `m_dashParticles` (L2209), `m_swingBurstParticles1/2` (L2210/L2211), `m_landParticles0/1` (L2213/L2214) — individual `CCParticleSystemQuad*` slots.
- `bool m_useLandParticles0` at L2212, `float m_landParticlesAngle` at L2215, `m_landParticleRelatedY` at L2216
- `cocos2d::CCMotionStreak* m_regularTrail` at L2161, `m_shipStreak` at L2162, `HardStreak* m_waveTrail` at L2163

#### Other useful methods
- `static PlayerObject* create(int player, int ship, GJBaseGameLayer* gameLayer, cocos2d::CCLayer* layer, bool playLayer)` at L45 — note final `bool playLayer` param; passing `false` may suppress some attachment.
- `init(int player, int ship, GJBaseGameLayer*, cocos2d::CCLayer*, bool playLayer)` at L666.
- `virtual void update(float dt)` at L54 — primary per-frame physics step.
- `void resetObject()` at L153 — full reset.
- `void resetCollisionLog(bool full)` at L1089 — used by sim every frame (Trajectory.cpp L375/L441/L446).
- `void resetCollisionValues()` at L1098 — second variant.
- `void preCollision()` at L963, `void postCollision(float dt, bool betweenSteps)` at L954 — wrapping pair around collision. Sim does NOT currently call these between collision and update; if the engine relies on preCollision to set up state for update, the sim may be skipping it.
- `bool collidedWithObject(float dt, GameObject* object)` at L288, plus rect/skip variants L297/L306. Used by `m_pl->checkCollisions` indirectly.
- `void saveToCheckpoint(PlayerCheckpoint*)` at L1251, `loadFromCheckpoint(PlayerCheckpoint*)` at L783 — **[INTEREST C3/divergence]** PlayerCheckpoint (see below) explicitly enumerates the fields the engine considers "the physics state" for save/restore. This is a better catalog than copyAttributes for "what does the bot need to mirror?" — see PlayerCheckpoint.hpp section.
- `void resetStateVariables()` at L1116, `updateStateVariables()` at L1976 — opaque but suggests there's a maintained "state" subset. Not currently called by sim.
- `disablePlayerControls()` L423, `enablePlayerControls()` L459, `lockPlayer()` L792 — sim could lock the real player's input, but more likely we just gate via simulator state.

---

### `PlayLayer.hpp` — already extensively hooked
ARM file: `bindings/Geode/binding_arm/PlayLayer.hpp` (1302 lines). Inherits `GJBaseGameLayer, CCCircleWaveDelegate, CurrencyRewardDelegate, DialogDelegate`.

Setup/lifecycle:
- ctor at L32, dtor at L41
- `static PlayLayer* create(GJGameLevel* level, bool useReplay, bool dontCreateObjects)` at L50 — note `dontCreateObjects` flag (false in normal play).
- `static PlayLayer* get()` at L59 — Geode addition, singleton accessor.
- `bool init(GJGameLevel*, bool useReplay, bool dontCreateObjects)` at L617
- `virtual void onEnterTransitionDidFinish()` at L77 — first frame after scene push lands here.
- `virtual void onExit()` at L86
- `virtual void postUpdate(float dt)` at L95 — already a hook target for the bot.
- `void setupHasCompleted()` at L905
- `void prepareCreateObjectsFromSetup(gd::string& levelString)` at L761, `processCreateObjectsFromSetup()` at L779, `createObjectsFromSetupFinished()` at L500
- `void startGame()` at L1013, `startGameDelayed()` at L1022, `startMusic()` at L1031, `prepareMusic(bool dontWait)` at L770
- `void resetLevel()` at L365, `resetLevelFromStart()` at L833, `delayedResetLevel()` at L518, `fullReset()` at L527, `delayedFullReset()` at L509
- `void onQuit()` at L698 — currently gated by `TrajectorySimulator::onPlayLayerQuit`.
- `void pauseGame(bool unfocused)` at L725, `void resume()` at L842, `void resumeAndRestart(bool fromStart)` at L851

Virtual overrides worth knowing:
- `virtual void destroyPlayer(PlayerObject* player, GameObject* object)` at L194 — kill hook. **[INTEREST C3]** sim must NOT trigger this for the real player, and the existing `isSimPlayer` guard pattern needs to wrap it.
- `virtual void checkpointActivated(CheckpointGameObject*)` at L284
- `virtual void updateVisibility(float dt)` at L140
- `virtual void updateVerifyDamage()` at L122, `m_damageVerified` at L1208 / `m_isIgnoreDamageEnabled` at L1220 / `toggleIgnoreDamage(bool)` at L1112 — **[INTEREST C3]** if the bot temporarily toggles ignoreDamage to study trajectories without dying, this is the entry point. Currently NOT used by Trajectory.cpp.
- `virtual void postUpdate(float dt)` at L95
- `virtual void playGravityEffect(bool flip)` at L266 — **[INTEREST C2/camera-leak]** spawns a camera-side gravity effect; if a sim crosses a gravity portal this fires unless gated.
- `virtual void manualUpdateObjectColors(GameObject*)` at L275
- `virtual void updateColor(...)` at L158
- `virtual void updateAttemptTime(float)` at L131 — called every frame; sim may want to gate to prevent attemptTime drift.
- `virtual void updateTimeWarp(float)` at L257
- `virtual void resetSPTriggered()` at L248
- `virtual void flipArt(bool)` at L293

Snapshot-relevant methods:
- `void takeStateSnapshot()` at L1076, `void compareStateSnapshot()` at L482, `void checkSnapshot()` at L311 — built-in snapshot infrastructure! **[INTEREST C2/camera-leak]** these may be the engine's own state-capture mechanism, possibly more thorough than the bot's `LayerStateSnapshot`. Worth investigating whether bot can piggy-back.
- `void saveActiveSaveObjects(gd::vector<SavedActiveObjectState>&, gd::vector<SavedSpecialObjectState>&)` at L860, `loadActiveSaveObjects(...)` at L644 — engine's own save/load for "active" objects (move triggers in flight, pulse triggers, etc.).
- `void saveDynamicSaveObjects(gd::vector<SavedObjectStateRef>&)` at L869, `loadDynamicSaveObjects(...)` at L662 — and for dynamic objects.
- `void scanActiveSaveObjects()` at L878, `scanDynamicSaveObjects()` at L887
- `gd::vector<GameObject*> m_dynamicSaveObjects` at L1213, `m_activeSaveObjects1/2` at L1214/L1215, `gd::vector<SavedObjectStateRef> m_dynamicSaveObjects2` at L1216

Performance/spatial query candidates (problem #5 — spatial cull):
- `void addObject(GameObject*)` at L410
- `void removeAllObjects()` at L806 — full nuke.
- `cocos2d::CCArray* m_circleWaveArray` at L1239, `m_collectibles` at L1240, `m_speedObjects` at L1232, `m_checkpointArray` at L1231, `m_coinArray` at L1210
- `float m_maxObjectX` at L1243 — total level extent.
- `cocos2d::CCPoint m_endPosition` at L1300
- `cocos2d::CCPoint posForTime(float)` at L239, `float timeForPos(...)` at L230 — useful for `getCurrentPercent` math.
- The level's spatial index lives on the parent class `GJBaseGameLayer` (out of this file's scope) — `m_sectionObjects` / similar spatial buckets are there.

Other useful fields:
- `cocos2d::CCLabelBMFont* m_statusLabel` at L1221, `m_attemptLabel` at L1244, `m_percentageLabel` at L1245, `m_infoLabel` at L1289
- `bool m_isPaused` at L1287
- `int m_jumps` at L1260, `bool m_hasJumped` at L1261, `int m_uncommittedJumps` at L1262
- `bool m_inResetDelay` at L1265
- `int m_lastAttemptPercent` at L1266
- `double m_attemptTime` at L1278, `m_bestAttemptTime` at L1279, `m_pauseTime` at L1280, `m_currentTime` at L1281, `m_pauseDelta` at L1282
- `CheckpointObject* m_currentCheckpoint` at L1230, `m_activatedCheckpoint` at L1298
- `EndTriggerGameObject* m_platformerEndTrigger` at L1301
- `cocos2d::CCArray* m_gravityEffects` at L1255 — **[INTEREST C2/camera-leak]** gravityEffect node array; if sim crosses a portal one gets pushed in here without gating.
- `int m_totalGravityEffects` at L1252, `m_activeGravityEffects` at L1253, `m_gravityEffectIndex` at L1254
- `bool m_disableGravityEffect` at L1288 — **[INTEREST C2]** could be temporarily set true during sim.
- `bool m_glitterEnabled` at L1284, `m_bgEffectDisabled` at L1285 — likewise.

No `shouldStartPaused` was found in this file.

---

### `RingObject.hpp`
ARM file: `bindings/Geode/binding_arm/RingObject.hpp` (136 lines). Inherits `EffectGameObject` — NOT `EnhancedGameObject`. **[INTEREST C4/orb-false-hit — IMPORTANT]**

The `TrajRingHook` in `src/Hooks.cpp` already gates `triggerActivated` (L97), `powerOnObject(int)` (L115), and `spawnCircle()` (L133). Crucially, `RingObject` does NOT have an `activatedByPlayer(PlayerObject*)` virtual override here — that lives in `EnhancedGameObject` (parent of `ParticleGameObject`, but RingObject's parent chain is `EffectGameObject -> GameObject` not via `EnhancedGameObject`). This means the bot's `TrajEnhancedHook::activatedByPlayer` in src/Hooks.cpp L260-L267 does NOT cover RingObject — confirmed by the architecture: `playerTouchedRing` on `GJBaseGameLayer` is the entry point for ring activation (already gated in src/Orbs.cpp `TrajOrbsLayerHook`), and `triggerActivated` is the secondary path (gated in `TrajRingHook`).

So there is NO bypass for RingObject through `activatedByPlayer` — the current hooks should be complete. The remaining concern for problem #4 is:
- `m_claimTouch` at L134 and `m_isSpawnOnly` at L135 — only two RingObject-specific fields.
- `static RingObject* create(char const* frame)` at L34
- `virtual void resetObject()` at L61 — may be called by `m_pl->resetLevel`; check whether sim activations leave a ring in a half-activated state where `resetObject` is NOT called and the ring "remembers" being activated by the sim. The activation tracking is in `EnhancedGameObject::m_activatedByPlayer1/m_activatedByPlayer2` (one bit each per player) and `m_isMultiActivate`. Confirmed sim's `markActivated`/`hasBeenActivated` set in `TrajEnhancedHook::hasBeenActivatedByPlayer` (src/Hooks.cpp L277-L288) reads these directly.
- `void spawnCircle()` at L133 — public, currently gated.

**Recommendation for C4:** check whether `playerTouchedRing` on the layer is *actually* the only entry point for the engine's ring-activation pipeline, by hooking `addToTouchedRings` (PlayerObject L216) and adding a defensive guard there too. Currently `Trajectory.cpp clearSimRingState` resets the ring tracking arrays after sim, but if `playerTouchedRing` is bypassed in some collision path (e.g., `collidedWithObjectInternal` directly mutates `m_touchingRings`), the sim could "false-activate" a ring it didn't touch, then on cleanup the real player thinks it touched.

---

## Particle files

### `ParticleGameObject.hpp`
ARM: `binding_arm/ParticleGameObject.hpp` (350 lines). Inherits `EnhancedGameObject` — so the `TrajEnhancedHook::activatedByPlayer`/`hasBeenActivatedByPlayer` already cover it. **[INTEREST C1/portal-particles]**
- `virtual void deactivateObject(bool deactivate)` at L142 — engine-side pause toggle for a single particle object.
- `virtual void claimParticle()` at L169, `virtual void unclaimParticle()` at L178 — pooling: the engine claims/releases CCParticleSystemQuads from a pool. **[INTEREST C1]** if sim activates a ParticleGameObject, it claims a particle from the global pool and the pool is one fewer for the real game until unclaimed. Even if the visible result is invisible, the resource leak compounds.
- `virtual void particleWasActivated()` at L187 — single-shot callback after activation.
- `virtual void blendModeChanged()` at L205, `applyParticleSettings(CCParticleSystemQuad*)` at L268
- `void createAndAddCustomParticle()` at L277, `void createParticlePreviewArt()` at L286
- `void updateParticle()` at L304, `updateParticleAngle(float, CCParticleSystemQuad*)` at L313
- Fields: `gd::string m_particleData` L341, `bool m_updatedParticleData` L342, `cocos2d::ParticleStruct m_particleStruct` L343, `bool m_hasUniformObjectColor` L344, `int m_popupPage` L345, `bool m_shouldQuickStart` L346, `float m_respawnResult` L347, `bool m_startingRespawn` L348, `bool m_notPreviewing` L349

There is NO global "particle gate" toggle on this class itself. The closest thing is `m_disableGravityEffect`/etc. on PlayLayer for specific FX; for portal particles specifically, gating happens in `PlayerObject::spawnPortalCircle` (L1323) and the per-object `claimParticle`/`activateObject` paths — both of which the existing `TrajEnhancedHook::activatedByPlayer` should catch (since ParticleGameObject inherits EnhancedGameObject).

**Recommendation for C1:** if portal particles are still firing, the activation path may be `PlayerObject::spawnPortalCircle` directly (called from `toggleFlyMode`/`toggleDartMode`/etc. mode switches when crossing a portal, NOT via `EnhancedGameObject::activatedByPlayer`). Adding a `$modify(TrajPlayerHook, PlayerObject)` that gates `spawnPortalCircle` (L1323), `spawnCircle` (L1287), `spawnDualCircle` (L1305), `spawnScaleCircle` (L1332), `spawnFromPlayer` (L1314), and `playSpawnEffect` (L936) when `sim().isSimulating()` would cover the player-side portal particles. The triggers themselves (in EnhancedGameObject) are already gated.

### `ParticlePreviewLayer.hpp`
ARM: `binding_arm/ParticlePreviewLayer.hpp` (83 lines). Editor/preview UI; `cocos2d::CCParticleSystemQuad* m_particleSystem` at L81. UI skip (only used in particle editor popup, not gameplay).

---

## All other files (one-line each unless noted)

- `MapPackCell.hpp` — UI/menu skip.
- `MapSelectLayer.hpp` — UI/menu skip.
- `MenuGameLayer.hpp` — Background scene shown behind main menu (animated player icon bouncing on ground). Has a `PlayerObject* m_playerObject` at L158 and `tryJump`/`updateColor`. Worth knowing exists — `PlayerObject::isVanillaPlayer()` (L2061) explicitly returns false for this — but irrelevant to bot.
- `MenuLayer.hpp` — UI/menu skip (main menu).
- `MessageListDelegate.hpp` — UI/menu skip.
- `MessagesProfilePage.hpp` — UI/menu skip.
- `MoreOptionsLayer.hpp` — UI/menu skip.
- `MoreSearchLayer.hpp` — UI/menu skip.
- `MoreVideoOptionsLayer.hpp` — UI/menu skip.
- `MPLobbyLayer.hpp` — UI/menu skip (multiplayer).
- `MultilineBitmapFont.hpp` — Font wrapper class; pure CCNode utility. Skip.
- `MultiplayerLayer.hpp` — UI/menu skip.
- `MultiTriggerPopup.hpp` — UI/menu skip (editor).
- `MusicArtistObject.hpp` — Audio asset metadata. Skip.
- `MusicBrowser.hpp` — UI/menu skip.
- `MusicBrowserDelegate.hpp` — UI/menu skip.
- `MusicDelegateHandler.hpp` — UI/menu skip.
- `MusicDownloadDelegate.hpp` — UI/menu skip.
- `MusicDownloadManager.hpp` — Audio asset manager. Skip.
- `MusicSearchResult.hpp` — UI/menu skip.
- `NCSInfoLayer.hpp` — UI/menu skip.
- `NewgroundsInfoLayer.hpp` — UI/menu skip.
- `NodePoint.hpp` — Editor utility (point in 2D space). Skip.
- `NumberInputDelegate.hpp` — UI/menu skip.
- `NumberInputLayer.hpp` — UI/menu skip.
- `OBB2D.hpp` — Oriented bounding box class (`bindings/Geode/binding_arm/OBB2D.hpp`, 93 lines). **[INTEREST C5/spatial-cull]** `static OBB2D* create(CCPoint center, float width, float height, float rotationAngle)` at L24, `cocos2d::CCRect getBoundingRect()` at L51, `bool overlaps(OBB2D* other)` at L78, `bool overlaps1Way(OBB2D* other)` at L87, `m_corners` (L88), `m_positions` (L89), `m_edges` (L90). PlayerObject::getOrientedBox() (L171) returns one of these; every rotated GameObject has one. For spatial cull, OBB2D::getBoundingRect gives you an axis-aligned bounding box cheap to test against a query rect.
- `ObjectControlGameObject.hpp` — `EffectGameObject` subclass (62 lines), no extra fields. Likely an "object control" trigger (stop jump/move/rotation/slide on group). Skip unless sim crosses one.
- `ObjectManager.hpp` — Static singleton (`instance()` at L24) holding object definitions and animation dicts. `m_objectDefinitions` L115, `m_loadedAnimations` L116. Used during level setup; no per-frame relevance. Skip.
- `ObjectToolbox.hpp` — Static singleton (`sharedState()` L24) mapping object key int → frame name string. `m_allKeys` (`gd::map<int, gd::string>`) at L70. Editor-side, no per-frame use. Skip.
- `OnlineListDelegate.hpp` — UI/menu skip.
- `OpacityEffectAction.hpp` — Single-tick opacity tween action (`bindings/Geode/binding_arm/OpacityEffectAction.hpp`, 36 lines). `void step(float delta)` at L23, fields `m_duration`/`m_fromValue`/`m_toValue`/`m_currentValue`/`m_targetGroupID`/`m_triggerUniqueID`/`m_controlID` etc. **[INTEREST C2/camera-leak]** these are in-flight effect actions on PlayLayer; if sim crosses an opacity trigger and snapshots the layer, the snapshot captures an OpacityEffectAction whose state then mutates during sim. The PlayLayer save/load infrastructure (saveDynamicSaveObjects) likely covers these.
- `OptionsCell.hpp` — UI/menu skip.
- `OptionsLayer.hpp` — UI/menu skip.
- `OptionsObject.hpp` — UI/menu skip.
- `OptionsObjectDelegate.hpp` — UI/menu skip.
- `OptionsScrollLayer.hpp` — UI/menu skip.
- `ParentalOptionsLayer.hpp` — UI/menu skip.
- `ParticleGameObject.hpp` — see Particle files section above.
- `ParticlePreviewLayer.hpp` — see above.
- `PauseLayer.hpp` — Mostly UI (`bindings/Geode/binding_arm/PauseLayer.hpp`, 262 lines). UI skip with one note: `static PauseLayer* create(bool unfocused)` at L35 takes `unfocused` (whether the pause was due to window-blur). `bool m_unfocused` at L260, `bool m_tryingQuit` at L261. **[INTEREST]** sim may want to detect when PauseLayer is active and freeze itself (currently the bot probably already does this via PlayLayer::pauseGame hook).
- `PlatformDownloadDelegate.hpp` — Cross-platform IO delegate (33 lines). Skip.
- `PlatformToolbox.hpp` — Static utility class (`bindings/Geode/binding_arm/PlatformToolbox.hpp`, 455 lines). Methods like `getDeviceRefreshRate()` L68, `getDisplaySize()` L77, file IO, clipboard. **[INTEREST]** `getDeviceRefreshRate` could be called by the bot to set `m_frameDt`; otherwise general utility. No per-frame relevance.
- `PlayerButtonCommand.hpp` — Tiny POD struct (20 lines): `PlayerButton m_button`, `bool m_isPush`, `bool m_isPlayer2`, `int m_step`, `double m_timestamp`. Used for replay/recording. Could be useful as a serialization format if the bot wants to log decisions. Skip for current problems.
- `PlayerCheckpoint.hpp` — **[HIGH-VALUE CROSS-REF]** `bindings/Geode/binding_arm/PlayerCheckpoint.hpp` (229 lines). This is what `PlayerObject::saveToCheckpoint`/`loadFromCheckpoint` write to. **It is the engine's authoritative answer to "what fields ARE the physics state?"** Compare to PlayerObject:
  - Includes: `m_position` (L44), `m_lastPosition` (L45), `m_yVelocityUnrounded` (L46), all gamemode bools (L47-L55), `m_isOnGround` (L56), `m_ghostType` (L57), `m_isMini` (L58), `m_playerSpeed` (L59), `m_isHidden` (L60), `m_isGoingLeft` (L61), `m_maybeReverseSpeed` (L62), `m_jumpBuffered` (L63), `m_isDashing` (L64), all dash state (L65-L70 incl. `m_dashRing`), `m_platformerCheckpoint` (L71), `m_lastFlipTime` (L72), `m_gravityMod` (L73), `m_objectSnappedTo` (L74), `m_snapDistance` (L75), `m_accelerationOrSpeed` (L76), `m_decreaseBoostSlide` (L77), `m_followRelated` (L78), `m_playerFollowFloats` (L79), `m_isOnSlope`/`m_wasOnSlope`/`m_slopeVelocity` (L81-L83), `m_rotation` (L84), `m_yVelocityBeforeSlope` (L88), all collision-log indices (L91-L96), all slope state (L97-L120), `m_speedMultiplier` (L125), `m_yStart` (L126), `m_gravity` (L127), `m_padRingRelated` (L130), `m_ringJumpRelated` (L137), `m_ringRelatedSet` (L138), `m_landParticlesAngle` (L140), `m_blackOrbRelated` (L145), `m_collidedTopMinY` etc. (L150-L153), `m_wasJumpBuffered`/`m_wasRobotJump`/`m_stateJumpBuffered`/`m_stateRingJump2` (L154-L157), `m_touchedRing`/`m_touchedCustomRing`/`m_touchedGravityPortal`/`m_maybeTouchedBreakableBlock`/`m_touchedPad` (L158-L162), `m_yVelocity`/`m_fallSpeed` (L163/L164), `m_lastPortalPos` (L170), `m_isOnGround2` (L172), `m_lastLandTime` (L173), `m_platformerVelocityRelated` (L174), `m_maybeIsBoosted` (L175), `m_isLocked`/`m_controlsDisabled` (L177/L178), `m_lastGroundedPos` (L179), `m_touchingRings` (L180), `m_touchedRings` (L181), `m_lastActivatedPortal` (L182), `m_totalTime` (L183), `m_yVelocityRelated3` (L184), all `m_state*` (L188-L196 incl. `m_stateBoostX/Y`, `m_stateScale`, `m_maybeStateForce2`), `m_platformerXVelocity` (L197), `m_holdingRight`/`m_holdingLeft`/`m_leftPressedFirst` (L198-L200), `m_isPlatformer`-related state but NOT `m_isPlatformer` itself, `m_stateNoAutoJump`/`m_stateDartSlide`/`m_stateHitHead`/`m_stateFlipGravity`/`m_stateForce`/`m_stateForceVector`/`m_affectedByForces` (L220-L226), `m_jumpPadRelated` (L227), `m_fallStartY` (L228).
  - **Notably ABSENT from PlayerCheckpoint** (compared to PlayerObject): `m_holdingButtons` (so the engine treats button-state as ephemeral and re-derives — this maps to problem #3), `m_stateRingJump` (the FIRST one, only L157's `m_stateRingJump2` is saved — odd asymmetry), `m_isOnGround3`/`m_isOnGround4` (only `m_isOnGround` and `m_isOnGround2` are saved — implies the others are derivable), `m_isOnIce`/`m_physDeltaRelated`/`m_isMoving`/`m_platformerMovingLeft/Right`/`m_isSlidingRight`/`m_maybeChangedDirectionAngle`/`m_maybeSlidingTime`/`m_maybeSlidingStartTime`/`m_changedDirectionsTime`/`m_slopeEndTime`, `m_inputsLocked`. **The bot should treat PlayerCheckpoint's field set as the floor for "what to copy via copyAttributes + manual fixup."** Field-by-field cross-compare against the existing initSim logic in Trajectory.cpp could uncover the missing cases driving problem #3.
- `PlayerControlGameObject.hpp` — Effect trigger to disable jump/move/rotation/slide on a group (66 lines). Fields: `m_stopJump`/`m_stopMove`/`m_stopRotation`/`m_stopSlide` at L62-L65. Triggers via `EffectGameObject::triggerObject`. **[INTEREST C3]** if a level uses these and sim doesn't honor them, sim diverges.
- `PlayerFireBoostSprite.hpp` — Visual fire sprite for swing/robot (62 lines). Pure CCSprite wrapper, no physics relevance. Skip.
- `PlayerObject.hpp` — see HEAVY READ section above.
- `PlayLayer.hpp` — see Critical files section above.
- `PointNode.hpp` — Tiny CCObject-wrapping-a-CCPoint (44 lines). Editor utility. Skip.
- `PriceLabel.hpp` — UI/menu skip.
- `ProfilePage.hpp` — UI/menu skip.
- `PromoInterstitial.hpp` — Ad/promo popup. Skip.
- `PulseEffectAction.hpp` — In-flight pulse-color tween action (`bindings/Geode/binding_arm/PulseEffectAction.hpp`, 59 lines). Fields: `m_fadeInTime`/`m_holdTime`/`m_fadeOutTime`/`m_currentValue`/`m_color`/`m_pulseEffectType`/`m_hsv`/`m_colorIndex`/`m_mainOnly`/`m_detailOnly`/`m_isDynamicHsv`/`m_triggerUniqueID`/`m_controlID`/`m_startTime`/`m_disabled`. Same considerations as OpacityEffectAction — captured by save/load infrastructure but worth knowing exists for snapshot work. Skip.
- `PurchaseItemPopup.hpp` — UI/menu skip.
- `RandTriggerGameObject.hpp` — Random-trigger (extends ChanceTriggerGameObject, 89 lines). `getRandomGroupID()` at L79 picks one of N children based on chance weights. **[INTEREST C3/divergence]** if sim execution rolls a different random outcome than reality, divergence. Likely the engine seeds its RNG from the level seed; check that `triggerObject` (L52, marked Rebinded) reads from the same RNG state and that snapshot/restore preserves it. The `m_levelSettings.m_seed` lives on GJBaseGameLayer (out of range).
- `RateDemonLayer.hpp` — UI/menu skip.
- `RateLevelDelegate.hpp` — UI/menu skip.
- `RateLevelLayer.hpp` — UI/menu skip.
- `RateStarsLayer.hpp` — UI/menu skip.
- `RecordButtonCommand.hpp` — POD struct for replay recording (20 lines): `m_button`, `m_isPush`, `m_isPlayer2`, `m_step`, `m_unk00c`. Skip.
- `RecordCheckpoint.hpp` — POD for replay-recording checkpoint state (24 lines): `m_index`/`m_step`/`m_seed`/`m_attempts`/`m_ticks`/`m_time`/`m_clicks`/`m_points`/`m_inputs`. **[INTEREST]** `m_seed` and `m_inputs` (gd::string of recorded inputs) suggest the bot could reuse this format for serializing decisions. Skip for current problems.
- `RetryLevelLayer.hpp` — UI/menu skip.
- `RewardedVideoDelegate.hpp` — Ad/monetization skip.
- `RewardsPage.hpp` — UI/menu skip.
- `RewardUnlockLayer.hpp` — UI/menu skip.
- `RingObject.hpp` — see Critical files section above.
- `RotateGameplayGameObject.hpp` — `EffectGameObject` subclass (91 lines) for "Rotate Gameplay" trigger (rotates the level world 90/180/270 deg). `m_moveDirection`/`m_groundDirection`/`m_editVelocity`/`m_overrideVelocity`/`m_velocityModX`/`m_velocityModY`/`m_changeChannel`/`m_channelOnly`/`m_targetChannelID`/`m_instantOffset`/`m_dontSlide` at L80-L90. Method `updateGameplayRotation()` at L79. **[INTEREST C2/camera-leak, C3/divergence]** A "Rotate Gameplay" trigger crossed during sim WILL rotate the world. PlayerObject has `rotateGameplay(...)` at L1170 and `unrotateGameplayObject(GameObject*)` at L1616, suggesting per-object rotation tracking. **The bot's `LayerStateSnapshot` should capture and restore the rotation state**, which lives somewhere on GJBaseGameLayer (probably `m_gameState`). Without it, a sim that crosses a rotate-gameplay trigger leaves the real game in a rotated state.

---

## Summary of findings in this range

1. **Problem #3 (divergence) — highest-leverage finding:** `PlayerCheckpoint.hpp` enumerates the ENGINE'S OWN definition of "what is the physics state" — see L44-L228. Cross-compare this against the bot's current `initSim` (Trajectory.cpp L396-L405) to find missing fields. **Strong candidates not currently copied: `m_jumpBuffered` family (PlayerObject L2241-L2245), `m_holdingButtons` (L2359), `m_lastPortalPos` (L2281), `m_touchedGravityPortal` (L2249), `m_objectSnappedTo` (L2197), `m_lastGroundedPos` (L2292), `m_isOnSlope`/`m_wasOnSlope`/`m_slopeVelocity`/`m_currentSlope`/`m_yVelocityBeforeSlope`, `m_jumpPadRelated` (L2355).**

2. **Problem #4 (orb-false-hit) — `clearSimRingState` is incomplete:** Three ring-family fields are NOT cleared by `clearSimRingState` (Trajectory.cpp L284-L322): `m_touchedGravityPortal` (PlayerObject.hpp L2249), `m_maybeTouchedBreakableBlock` (L2250), `m_touchedPad` (L2252). Add these to `clearSimRingState`.

3. **Problem #4 — RingObject NOT covered by `TrajEnhancedHook`:** Confirmed RingObject inherits `EffectGameObject`, not `EnhancedGameObject`. The current `TrajRingHook` (src/Hooks.cpp L22-37) gates `triggerActivated`/`powerOnObject`/`spawnCircle` and `TrajOrbsLayerHook` gates `playerTouchedRing`. There is NO `activatedByPlayer` bypass on RingObject. Coverage is complete via these specific hooks. Investigate `addToTouchedRings` (PlayerObject.hpp L216) as a defensive third gate if false-hits persist.

4. **Problem #1 (portal particles) — patch the player-side spawn methods:** Particles still firing during sim are likely from `PlayerObject::spawnPortalCircle` (L1323), `spawnDualCircle` (L1305), `spawnScaleCircle` (L1332), `spawnFromPlayer` (L1314), and the ground/jump particle paths called from `update()` and `toggle*Mode`. Add a `$modify(TrajPlayerObjectHook, PlayerObject)` that gates these when `sim().isSimulating()`. Note `flashPlayer` (L486) is the white-flash overlay used by mode-switch toggles — it writes `m_flashTime`/`m_flashDuration`/`m_flashDelay`/`m_flashMainColor`/`m_flashSecondColor` (L2183-L2187) which then trigger render in `update()`; gate at the call site for cleanest fix.

5. **Problem #2 (camera leak) — gravity effects + rotate-gameplay:** PlayLayer has `m_gravityEffects` (L1255), `m_totalGravityEffects`/`m_activeGravityEffects` (L1252/L1253), and `bool m_disableGravityEffect` (L1288). Setting `m_disableGravityEffect = true` for sim duration is a one-line fix. Additionally `bool m_glitterEnabled` (L1284) and `m_bgEffectDisabled` (L1285) are toggles for layer FX. **The biggest unaddressed leak is the "Rotate Gameplay" trigger** — `RotateGameplayGameObject` rotates the entire world via PlayerObject's `rotateGameplay()` (L1170), and the rotation state persists on GJBaseGameLayer. Bot's `LayerStateSnapshot` must capture/restore world rotation.

6. **Problem #5 (spatial cull) — `OBB2D::getBoundingRect` (line 51) is the building block.** Every rotated GameObject has a `getOrientedBox()` returning `OBB2D*` (PlayerObject L171 confirms). For collision pre-cull, query the OBB2D's bounding rect against the sim's current/predicted position rect. The level-wide spatial index is on `GJBaseGameLayer` (out of M-R range, in G-files), but PlayLayer has helpful caches: `m_circleWaveArray` L1239, `m_collectibles` L1240, `m_speedObjects` L1232, `m_checkpointArray` L1231, `float m_maxObjectX` L1243.

7. **Problem #6 (staircase) — buffered-jump fields aren't copied:** `m_jumpBuffered` (L2241), `m_wasJumpBuffered` (L2243), `m_wasRobotJump` (L2244), `unsigned char m_stateJumpBuffered` (L2245). **PlayerCheckpoint saves these (L63, L154, L155, L156)** confirming they are part of the physics state. If `copyAttributes` is missing them (parallel to its known miss of `m_isOnGround` and `m_gravityMod`), staircase failures trace here. Add explicit copies in `initSim` lambda (Trajectory.cpp L396).

8. **Engine snapshot infrastructure exists:** `PlayLayer::takeStateSnapshot()` (L1076), `compareStateSnapshot()` (L482), `checkSnapshot()` (L311), plus `saveActiveSaveObjects`/`saveDynamicSaveObjects`/`scanActiveSaveObjects`/`scanDynamicSaveObjects`. Worth investigating if these can be invoked by the bot to capture/restore in lieu of (or alongside) the manually-built `LayerStateSnapshot`. Could also be relevant for problem #2 (camera leak) since a built-in mechanism likely captures more state than the bot's hand-rolled snapshot.

9. **Confirmed copyAttributes coverage gaps:** Trajectory.cpp explicitly fixes `m_gravityMod` and `m_isOnGround` after `copyAttributes`. Based on PlayerCheckpoint as ground truth, additional likely-missed fields: `m_holdingButtons` (the engine's PlayerCheckpoint doesn't save it, suggesting it's volatile and copyAttributes might similarly skip), and the entire `m_jumpBuffered` family. A test: in initSim, log `base->m_jumpBuffered` and `sim->m_jumpBuffered` after copyAttributes; if they diverge, copyAttributes is missing them and you've found the root cause of staircase failures.

10. **PlayerObject Geode-additions worth a try:** `bool isVanillaPlayer()` (L2061), `bool isPlayer1()` (L2074), `bool isPlayer2()` (L2087). The "vanilla player" check explicitly returns false for PlayerObjects in MenuGameLayer — useful as a defense if a hook fires while in the main menu. Existing `isSimPlayer()` guard pattern probably already covers it, but worth knowing.
