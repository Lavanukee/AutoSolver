# PlayerObject — sim-relevance audit

PlayerObject is the engine's per-player physics + visuals + input + state class. It owns the per-tick gameplay state (`m_yVelocity`, `m_isOnGround`, slope/collision pointers, ring-touch trackers, mode flags) that `PlayLayer::checkCollisions` and `PlayerObject::update` step through every frame. The mod (jedd.trajectory) creates TWO sim PlayerObjects (`m_simP1`, `m_simP2`) via `PlayerObject::create(1, 1, pl, pl, true)` in `src/Trajectory.cpp:236-243`; the real players are `pl->m_player1` and `pl->m_player2`. Each tick, the sim's `copyAttributes(real)` snapshots the engine-known prefix of fields from real onto sim, then the bot drives `checkCollisions(sim, dt, false)` + `sim->update(dt)` per simulated frame. Anything written to fields/objects shared with real (e.g. `m_touchingRings`, EnhancedGameObject activation flags, `m_speedObjects`) must be either own-allocated for sim or save/restored across the sim run, and anything that mutates persistent stats or fires audio/visual must be suppressed when `sim().isSimulating()` is true.

## Summary stats
- Total methods declared: 227 (1 ctor, 1 dtor, 1 static factory, 14 virtuals, 5 source-only, 206 with engine address)
- Total fields declared: 292 (291 `m_*` + 1 `unk_584` double)
- Methods classified as state-mutating / persistent-stat / audio (need isolation): 64 — of which 9 currently hooked
- Fields classified as player-physics-state (need real->sim copy): 86 — of which 31 currently explicit-copied (most additional are covered by engine's `copyAttributes`; 18 flagged as leak-candidate below)
- Fields classified as shared-container or real-player-pointer (need careful handling): 11

## Methods table

| Line | Name | Signature | Virtual? | Classification | Sim impact / notes |
|------|------|-----------|----------|----------------|-----|
| 14241 | PlayerObject | `PlayerObject()` | no | constructor | Engine ctor; sim creates via `create()` factory, not direct. |
| 14242 | ~PlayerObject | `~PlayerObject()` | (virtual implied) | destructor | Frees player resources; sim players live for whole PlayLayer lifetime, run once at PlayLayer cleanup. |
| 14244 | create | `static PlayerObject* create(int player, int ship, GJBaseGameLayer* gameLayer, cocos2d::CCLayer* layer, bool playLayer)` | no | constructor | Sim factory entry point: `createSimPlayer` in Trajectory.cpp:237 calls with (1,1,pl,pl,true). |
| 14246 | update | `virtual void update(float dt)` | yes | physics | Per-tick physics step; sim MUST call super each tick. Already hooked in TrajPlayerObjectHook:384 to capture frameDelta when not simulating. |
| 14247 | setScaleX | `virtual void setScaleX(float scale)` | yes | state-mutating-player | Writes m_scaleX; sim calling on itself is fine. |
| 14248 | setScaleY | `virtual void setScaleY(float scale)` | yes | state-mutating-player | Writes m_scaleY; sim calling on itself is fine. |
| 14249 | setScale | `virtual void setScale(float scale)` | yes | state-mutating-player | Convenience; same as above. |
| 14250 | setPosition | `virtual void setPosition(cocos2d::CCPoint const& position)` | yes | state-mutating-player | Sim uses this directly in runBranch/initSim to reset its own position from base. |
| 14251 | setVisible | `virtual void setVisible(bool visible)` | yes | visual | Sim sets itself invisible. Safe. |
| 14252 | setRotation | `virtual void setRotation(float rotation)` | yes | state-mutating-player | Sets player rotation; sim consults during render. |
| 14253 | setOpacity | `virtual void setOpacity(unsigned char opacity)` | yes | visual | Visual-only; sim calling on itself harmless. |
| 14254 | setColor | `virtual void setColor(cocos2d::ccColor3B const& color)` | yes | visual | Visual-only. |
| 14255 | setFlipX | `virtual void setFlipX(bool flipX)` | yes | state-mutating-player | Flips sprite; cheap state. |
| 14256 | setFlipY | `virtual void setFlipY(bool flipY)` | yes | state-mutating-player | Cube/mode flip; sim needs for accurate mode physics. |
| 14257 | resetObject | `virtual void resetObject()` | yes | state-mutating-player | Resets internal state; sim only on its own object. |
| 14258 | getRealPosition | `virtual cocos2d::CCPoint getRealPosition()` | yes | read-only | Getter. |
| 14259 | getOrientedBox | `virtual OBB2D* getOrientedBox()` | yes | read-only | Getter for collision OBB. |
| 14260 | getObjectRotation | `virtual float getObjectRotation()` | yes | read-only | Getter. |
| 14261 | animationFinished | `virtual void animationFinished(char const* key)` | yes | engine-internal | AnimatedSpriteDelegate callback; fires from animation system. |
| 14263 | activateStreak | `void activateStreak()` | no | visual | Re-enables m_regularTrail / wave trail; visual side effect — should be suppressed if sim triggers indirectly. **Leak candidate.** |
| 14264 | addAllParticles | `void addAllParticles()` | no | visual | Adds particle systems as children — leaks if sim triggers. **Leak candidate.** |
| 14265 | addToTouchedRings | `void addToTouchedRings(RingObject* object)` | no | state-mutating-player | Writes m_touchingRings CCArray and m_touchedRings set. Already protected: m_touchingRings is own-allocated for sim (adoptOwnRings, Trajectory.cpp:245); m_touchedRings is per-PlayerObject. |
| 14266 | addToYVelocity | `void addToYVelocity(double yVelocity, int type)` | no | physics | Writes m_yVelocity; sim physics needs this. |
| 14267 | animatePlatformerJump | `void animatePlatformerJump(float scale)` | no | visual | Visual scale tween. **Leak candidate** (CCAction). |
| 14268 | boostPlayer | `void boostPlayer(float yVelocity)` | no | physics | Pad-jump impulse; sim must call to predict pad bounces. |
| 14269 | bumpPlayer | `void bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject* object)` | no | physics | Bump physics; sim needs (with noEffects=true ideally). |
| 14270 | buttonDown | `bool buttonDown(PlayerButton button)` | no | read-only | Getter for m_holdingButtons. |
| 14271 | canStickToGround | `bool canStickToGround()` | no | read-only | Predicate. |
| 14272 | checkSnapJumpToObject | `void checkSnapJumpToObject(GameObject* object)` | no | state-mutating-player | Writes m_objectSnappedTo. |
| 14273 | collidedWithObject | `bool collidedWithObject(float dt, GameObject* object)` | no | physics | Collision resolve; sim path. |
| 14274 | collidedWithObject | `bool collidedWithObject(float dt, GameObject* object, cocos2d::CCRect rect, bool skipCheck)` | no | physics | Same. |
| 14275 | collidedWithObjectInternal | `bool collidedWithObjectInternal(float dt, GameObject* object, cocos2d::CCRect rect, bool skipCheck)` | no | physics | Engine collision impl. |
| 14276 | collidedWithSlope | `void collidedWithSlope(float dt, GameObject* object, bool skipPre)` | no | physics | Slope physics resolve. |
| 14277 | collidedWithSlopeInternal | `void collidedWithSlopeInternal(float dt, GameObject* object, bool forced)` | no | physics | Engine impl. |
| 14278 | convertToClosestRotation | `float convertToClosestRotation(float rotation)` | no | read-only | Pure math. |
| 14279 | copyAttributes | `void copyAttributes(PlayerObject* player)` | no | state-mutating-player | **CRITICAL**: this is the engine method used to shallow-copy base->sim each tick. Used in runBranch:543 and initSim:633. Coverage is opaque (no source); explicit-copy lists below patch known misses. |
| 14280 | createFadeOutDartStreak | `void createFadeOutDartStreak()` | no | visual | Adds streak node. **Leak candidate** if sim invokes. |
| 14281 | createRobot | `void createRobot(int frame)` | no | visual | Adds robot sprite as child. **Leak candidate**. |
| 14282 | createSpider | `void createSpider(int frame)` | no | visual | Adds spider sprite as child. **Leak candidate**. |
| 14283 | deactivateParticle | `void deactivateParticle()` | no | visual | Particle off — harmless. |
| 14284 | deactivateStreak | `void deactivateStreak(bool stop)` | no | visual | Trail off; cosmetic. |
| 14285 | destroyFromHitHead | `bool destroyFromHitHead()` | no | state-mutating-layer | Calls destroyPlayer; sim path is intercepted by PlayLayer::destroyPlayer hook. |
| 14286 | didHitHead | `void didHitHead()` | no | physics | Hit-head state set; sim physics path. |
| 14287 | disableCustomGlowColor | `void disableCustomGlowColor()` | no | visual | Visual-only. |
| 14288 | disablePlayerControls | `void disablePlayerControls()` | no | state-mutating-player | Sets m_controlsDisabled; sim shouldn't call on real. |
| 14289 | disableSwingFire | `void disableSwingFire()` | no | visual | Swing-fire sprite off; cosmetic. |
| 14290 | doReversePlayer | `void doReversePlayer(bool reverse)` | no | physics | Flips horizontal direction; sim needs for reverse-portals. |
| 14291 | enableCustomGlowColor | `void enableCustomGlowColor(cocos2d::ccColor3B const& color)` | no | visual | Visual-only. |
| 14292 | enablePlayerControls | `void enablePlayerControls()` | no | state-mutating-player | Clears m_controlsDisabled. |
| 14293 | exitPlatformerAnimateJump | `void exitPlatformerAnimateJump()` | no | visual | Cancels jump-anim CCAction. **Leak candidate**. |
| 14294 | fadeOutStreak2 | `void fadeOutStreak2(float duration)` | no | visual | Cosmetic streak fade. |
| 14295 | flashPlayer | `void flashPlayer(float flashDuration, float flashDelay, cocos2d::ccColor3B mainColor, cocos2d::ccColor3B secondColor)` | no | visual | Color flash; **already hooked** in TrajPlayerObjectHook:407 to suppress for sim. |
| 14296 | flipGravity | `void flipGravity(bool flip, bool noEffects)` | no | physics | Per-player gravity flip; the layer-level `GJBaseGameLayer::flipGravity` is already hooked at Hooks.cpp:193 to force noEffects=true. |
| 14297 | flipMod | `int flipMod()` | no | read-only | Getter. |
| 14298 | gameEventTriggered | `void gameEventTriggered(int gameEvent, int material)` | no | state-mutating-player | Engine event dispatch; sim's own may run. |
| 14299 | getActiveMode | `GameObjectType getActiveMode()` | no | read-only | Getter. |
| 14300 | getCurrentXVelocity | `double getCurrentXVelocity()` | no | read-only | Getter. |
| 14301 | getModifiedSlopeYVel | `float getModifiedSlopeYVel()` | no | read-only | Getter. |
| 14302 | getOldPosition | `float getOldPosition(float dt)` | no | read-only | Computed previous position. |
| 14303 | getSecondColor | `cocos2d::ccColor3B getSecondColor()` | no | read-only | Getter. |
| 14304 | getYVelocity | `double getYVelocity()` | no | read-only | Getter. |
| 14305 | gravityDown | `void gravityDown()` | no | physics | Inline gravity setter; sim physics. |
| 14306 | gravityUp | `void gravityUp()` | no | physics | Inline gravity setter; sim physics. |
| 14307 | handlePlayerCommand | `void handlePlayerCommand(int command)` | no | state-mutating-player | Player-command dispatch. |
| 14308 | handleRotatedCollisionInternal | `bool handleRotatedCollisionInternal(float dt, GameObject* object, cocos2d::CCRect rect, bool skipCheck, bool skipPre, bool slope)` | no | physics | Engine collision impl. |
| 14309 | handleRotatedObjectCollision | `bool handleRotatedObjectCollision(float dt, GameObject* object, cocos2d::CCRect rect, bool skipCheck)` | no | physics | Collision dispatch. |
| 14310 | handleRotatedSlopeCollision | `void handleRotatedSlopeCollision(float dt, GameObject* object, bool skipPre)` | no | physics | Slope-rot collision. |
| 14311 | hardFlipGravity | `void hardFlipGravity()` | no | physics | Flip without animation; sim friendly. |
| 14312 | hitGround | `void hitGround(GameObject* object, bool notFlipped)` | no | physics | Ground-land resolution + jump-buffer. |
| 14313 | hitGroundNoJump | `void hitGroundNoJump(GameObject* object, bool notFlipped)` | no | physics | Same as above sans buffered jump. |
| 14314 | incrementJumps | `void incrementJumps()` | no | persistent-stat | **Already hooked** at TrajPlayerObjectHook:389 to suppress for sim — increments PlayLayer/save stats. |
| 14315 | init | `bool init(int player, int ship, GJBaseGameLayer* gameLayer, cocos2d::CCLayer* layer, bool playLayer)` | no | constructor | Called from create(). |
| 14316 | isBoostValid | `bool isBoostValid(float yVelocity)` | no | read-only | Predicate. |
| 14317 | isFlying | `bool isFlying()` | no | read-only | Getter (ship/UFO/wave/swing). |
| 14318 | isInBasicMode | `bool isInBasicMode()` | no | read-only | Getter (cube). |
| 14319 | isInNormalMode | `bool isInNormalMode()` | no | read-only | Getter. |
| 14320 | isSafeFlip | `bool isSafeFlip(float flipTime)` | no | read-only | Time-window predicate. |
| 14321 | isSafeHeadTest | `bool isSafeHeadTest()` | no | read-only | Predicate. |
| 14322 | isSafeMode | `bool isSafeMode(float changeTime)` | no | read-only | Predicate. |
| 14323 | isSafeSpiderFlip | `bool isSafeSpiderFlip(float flipTime)` | no | read-only | Predicate. |
| 14324 | levelFlipFinished | `void levelFlipFinished()` | no | state-mutating-layer | Sets m_levelFlipping; layer state — already covered by LayerStateSnapshot. |
| 14325 | levelFlipping | `bool levelFlipping()` | no | read-only | Getter. |
| 14326 | levelWillFlip | `void levelWillFlip()` | no | state-mutating-layer | Sets layer flip flag; LayerStateSnapshot covers. |
| 14327 | limitDashRotation | `void limitDashRotation(float& rotation)` | no | physics | Inline math; sim path. |
| 14328 | loadFromCheckpoint | `void loadFromCheckpoint(PlayerCheckpoint* object)` | no | state-mutating-player | Sim must never call (would overwrite from a checkpoint). |
| 14329 | lockPlayer | `void lockPlayer()` | no | state-mutating-player | Sets m_isLocked; sim should not call on real. |
| 14330 | logValues | `void logValues()` | no | engine-internal | Debug log. |
| 14331 | modeDidChange | `void modeDidChange()` | no | state-mutating-player | Mode-change callback. |
| 14332 | performSlideCheck | `void performSlideCheck()` | no | physics | Inline slide test. |
| 14333 | placeStreakPoint | `void placeStreakPoint()` | no | visual | Adds segment to motion streak; **leak candidate** if sim hits it (wave trail leak risk). HardStreak::addPoint is already suppressed in Hooks.cpp:597. |
| 14334 | playBumpEffect | `void playBumpEffect(int objectType, GameObject* player)` | no | audio | **Already hooked** at TrajPlayerObjectHook:399 to suppress for sim. |
| 14335 | playBurstEffect | `void playBurstEffect()` | no | visual | Particle burst. **Leak candidate**. |
| 14336 | playCompleteEffect | `void playCompleteEffect(bool noEffects, bool instant)` | no | visual | Level-complete fanfare. **Leak candidate** (gated by playEndAnimationToPos hook upstream). |
| 14337 | playDeathEffect | `void playDeathEffect()` | no | visual | Death animation. **Leak candidate** if sim triggers — gated by destroyPlayer hook upstream. |
| 14338 | playDynamicSpiderRun | `void playDynamicSpiderRun()` | no | visual | Spider run anim. |
| 14339 | playerDestroyed | `void playerDestroyed(bool noEffects)` | no | state-mutating-player | Death state set; gated upstream. |
| 14340 | playerIsFalling | `bool playerIsFalling(float yVelocity)` | no | read-only | Predicate. |
| 14341 | playerIsFallingBugged | `bool playerIsFallingBugged()` | no | read-only | Predicate. |
| 14342 | playerIsMovingUp | `bool playerIsMovingUp()` | no | read-only | Predicate. |
| 14343 | playerTeleported | `void playerTeleported()` | no | state-mutating-player | Sets m_wasTeleported; sim's own may run. |
| 14344 | playingEndEffect | `void playingEndEffect()` | no | read-only | Getter/predicate. |
| 14345 | playSpawnEffect | `void playSpawnEffect()` | no | visual | Particle. **Leak candidate**. |
| 14346 | playSpiderDashEffect | `void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to)` | no | visual | **Already hooked** at TrajPlayerObjectHook:394 to suppress for sim. |
| 14347 | postCollision | `void postCollision(float dt)` | no | physics | Per-tick post-collision pass. |
| 14348 | preCollision | `void preCollision()` | no | physics | Per-tick pre-collision pass. |
| 14349 | preSlopeCollision | `bool preSlopeCollision(float dt, GameObject* object)` | no | physics | Slope pre-pass. |
| 14350 | propellPlayer | `void propellPlayer(float yVelocity, bool noEffects, int objectType)` | no | physics | Spring/orb propel; sim must call. |
| 14351 | pushButton | `bool pushButton(PlayerButton button)` | no | physics | Driven by sim (runBranch/runPlan call to inject inputs). |
| 14352 | pushDown | `void pushDown()` | no | physics | Inline. |
| 14353 | pushPlayer | `void pushPlayer(float yVelocity)` | no | physics | Inline. |
| 14354 | redirectDash | `void redirectDash(float rotation)` | no | physics | Dash-redirect orb. |
| 14355 | redirectPlayerForce | `void redirectPlayerForce(float rotation, float modifier, float minimum, float maximum)` | no | physics | Force redirect. |
| 14356 | releaseAllButtons | `void releaseAllButtons()` | no | state-mutating-player | Used internally; not called by sim directly. |
| 14357 | releaseButton | `bool releaseButton(PlayerButton button)` | no | physics | Driven by sim to inject button releases. |
| 14358 | removeAllParticles | `void removeAllParticles()` | no | visual | Cleanup; safe. |
| 14359 | removePendingCheckpoint | `void removePendingCheckpoint()` | no | state-mutating-player | Should never run for sim (checkpoint mutation). |
| 14360 | removePlacedCheckpoint | `void removePlacedCheckpoint()` | no | state-mutating-player | Same. |
| 14361 | resetAllParticles | `void resetAllParticles()` | no | visual | Reset particle systems. |
| 14362 | resetCollisionLog | `void resetCollisionLog(bool full)` | no | physics | **Called by sim** (runBranch:607, runPlan:727) at the start of each tick. |
| 14363 | resetCollisionValues | `void resetCollisionValues()` | no | physics | Clears collided* doubles. |
| 14364 | resetPlayerIcon | `void resetPlayerIcon()` | no | visual | Icon reset. |
| 14365 | resetStateVariables | `void resetStateVariables()` | no | state-mutating-player | Resets state on level reset; not sim-called. |
| 14366 | resetStreak | `void resetStreak()` | no | visual | Streak reset; not sim-called. |
| 14367 | resetTouchedRings | `void resetTouchedRings(bool removeAll)` | no | state-mutating-player | Clears m_touchedRings + m_touchingRings; sim's own state. |
| 14368 | reverseMod | `int reverseMod()` | no | read-only | Getter. |
| 14369 | reversePlayer | `void reversePlayer(EffectGameObject* object)` | no | physics | Reverse-portal handler. |
| 14370 | ringJump | `void ringJump(RingObject* object, bool skipCheck)` | no | physics | Orb-jump impulse; sim must call for orb-bouncing. |
| 14371 | rotateGameplay | `void rotateGameplay(int moveDirection, int groundDirection, bool editVelocity, float velocityModX, float velocityModY, bool overrideVelocity, bool dontSlide)` | no | state-mutating-layer | Sideways portal handler — rotates camera + velocity. State covered by LayerStateSnapshot. |
| 14372 | rotateGameplayObject | `void rotateGameplayObject(GameObject* object)` | no | state-mutating-layer | Per-object rotation for rotated-portal mode. |
| 14373 | rotateGameplayOnly | `void rotateGameplayOnly(bool sideways)` | no | state-mutating-layer | Same family. |
| 14374 | rotatePreSlopeObjects | `void rotatePreSlopeObjects()` | no | physics | Inline; sim path. |
| 14375 | runBallRotation | `void runBallRotation(float speed)` | no | visual | CCAction rotation; **leak candidate** if sim triggers ball rotation tween. |
| 14376 | runBallRotation2 | `void runBallRotation2()` | no | visual | Same. **Leak candidate**. |
| 14377 | runNormalRotation | `void runNormalRotation()` | no | visual | CCAction. **Leak candidate**. |
| 14378 | runNormalRotation | `void runNormalRotation(bool notNormalMode, float speed)` | no | visual | Overload. **Leak candidate**. |
| 14379 | runRotateAction | `void runRotateAction(bool ground, int type)` | no | visual | CCAction; **leak candidate**. |
| 14380 | saveToCheckpoint | `void saveToCheckpoint(PlayerCheckpoint* checkpoint)` | no | state-mutating-layer | Writes a checkpoint object; sim should never call. |
| 14381 | setSecondColor | `void setSecondColor(cocos2d::ccColor3B const& color)` | no | visual | Setter; sim's own. |
| 14382 | setupStreak | `void setupStreak()` | no | visual | Builds m_regularTrail; init-time. |
| 14383 | setYVelocity | `void setYVelocity(double velocity, int type)` | no | physics | Sim writes its own m_yVelocity. |
| 14384 | spawnCircle | `void spawnCircle()` | no | visual | Circle-emit. **Leak candidate**. |
| 14385 | spawnCircle2 | `void spawnCircle2()` | no | visual | **Leak candidate**. |
| 14386 | spawnDualCircle | `void spawnDualCircle()` | no | visual | **Leak candidate**. |
| 14387 | spawnFromPlayer | `void spawnFromPlayer(PlayerObject* player, bool flip)` | no | state-mutating-player | Dual-spawn helper; sim's own. |
| 14388 | spawnPortalCircle | `void spawnPortalCircle(cocos2d::ccColor3B color, float startRadius)` | no | visual | Portal circle visual. **Leak candidate**. |
| 14389 | spawnScaleCircle | `void spawnScaleCircle()` | no | visual | **Leak candidate**. |
| 14390 | specialGroundHit | `void specialGroundHit()` | no | physics | Inline. |
| 14391 | speedDown | `void speedDown()` | no | physics | Inline. |
| 14392 | speedUp | `void speedUp()` | no | physics | Inline. |
| 14393 | spiderTestJump | `void spiderTestJump(bool dynamic)` | no | physics | Spider raycast jump. |
| 14394 | spiderTestJumpInternal | `void spiderTestJumpInternal(bool dynamic)` | no | physics | Internal impl. |
| 14395 | spiderTestJumpX | `void spiderTestJumpX(bool dynamic)` | no | physics | Source-only. |
| 14396 | spiderTestJumpY | `void spiderTestJumpY(bool dynamic)` | no | physics | Source-only. |
| 14397 | startDashing | `void startDashing(DashRingObject* object)` | no | physics | Dash orb entry; sim must call to predict. |
| 14398 | stopBurstEffect | `void stopBurstEffect()` | no | visual | Stops burst particle. |
| 14399 | stopDashing | `void stopDashing()` | no | physics | Dash exit. |
| 14400 | stopParticles | `void stopParticles()` | no | visual | Particles off. |
| 14401 | stopPlatformerJumpAnimation | `void stopPlatformerJumpAnimation()` | no | visual | Cancel CCAction; cleanup. |
| 14402 | stopRotation | `void stopRotation(bool ground, int type)` | no | visual | CCAction stop. |
| 14403 | stopStreak2 | `void stopStreak2()` | no | visual | Stop wave streak. |
| 14404 | storeCollision | `void storeCollision(PlayerCollisionDirection direction, int id)` | no | state-mutating-player | Writes m_collisionLog* dictionaries. |
| 14405 | switchedDirTo | `bool switchedDirTo(PlayerButton button)` | no | read-only | Predicate. |
| 14406 | switchedToMode | `void switchedToMode(GameObjectType type)` | no | state-mutating-player | Mode-change side effects. |
| 14407 | testForMoving | `bool testForMoving(float dt, GameObject* object)` | no | read-only | Predicate. |
| 14408 | toggleBirdMode | `void toggleBirdMode(bool enable, bool noEffects)` | no | physics | Mode switch (UFO); sim must call. |
| 14409 | toggleDartMode | `void toggleDartMode(bool enable, bool noEffects)` | no | physics | Mode switch (wave). |
| 14410 | toggleFlyMode | `void toggleFlyMode(bool enable, bool noEffects)` | no | physics | Mode switch (ship). |
| 14411 | toggleGhostEffect | `void toggleGhostEffect(GhostType type)` | no | visual | Ghost trail. |
| 14412 | togglePlatformerMode | `void togglePlatformerMode(bool val)` | no | state-mutating-player | Sets m_isPlatformer. |
| 14413 | togglePlayerScale | `void togglePlayerScale(bool enable, bool noEffects)` | no | physics | Mini-mode scaling. |
| 14414 | toggleRobotMode | `void toggleRobotMode(bool enable, bool noEffects)` | no | physics | Mode switch. |
| 14415 | toggleRollMode | `void toggleRollMode(bool enable, bool noEffects)` | no | physics | Mode switch (ball). |
| 14416 | toggleSpiderMode | `void toggleSpiderMode(bool enable, bool noEffects)` | no | physics | Mode switch. |
| 14417 | toggleSwingMode | `void toggleSwingMode(bool enable, bool noEffects)` | no | physics | Mode switch. |
| 14418 | toggleVisibility | `void toggleVisibility(bool visible)` | no | visual | Visibility setter; sim sets itself invisible. |
| 14419 | touchedObject | `void touchedObject(GameObject* object)` | no | state-mutating-player | Generic touch handler. |
| 14420 | tryPlaceCheckpoint | `void tryPlaceCheckpoint()` | no | state-mutating-layer | Creates a checkpoint; sim must never call. **Leak candidate** (writes PlayLayer state). |
| 14421 | unrotateGameplayObject | `void unrotateGameplayObject(GameObject* object)` | no | state-mutating-layer | Reverse of rotateGameplayObject. |
| 14422 | unrotatePreSlopeObjects | `void unrotatePreSlopeObjects()` | no | physics | Inline. |
| 14423 | updateCheckpointMode | `void updateCheckpointMode(bool enable)` | no | state-mutating-player | Checkpoint-mode toggle. |
| 14424 | updateCheckpointTest | `void updateCheckpointTest()` | no | state-mutating-player | Per-tick checkpoint test. |
| 14425 | updateCollide | `void updateCollide(PlayerCollisionDirection direction, GameObject* object)` | no | physics | Collision-direction update. |
| 14426 | updateCollideBottom | `void updateCollideBottom(float y, GameObject* object)` | no | physics | Inline. |
| 14427 | updateCollideLeft | `void updateCollideLeft(float x, GameObject* object)` | no | physics | Sim path. |
| 14428 | updateCollideRight | `void updateCollideRight(float x, GameObject* object)` | no | physics | Sim path. |
| 14429 | updateCollideTop | `void updateCollideTop(float y, GameObject* object)` | no | physics | Inline. |
| 14430 | updateDashAnimation | `void updateDashAnimation()` | no | visual | Dash anim per-tick. |
| 14431 | updateDashArt | `void updateDashArt()` | no | visual | Dash sprite per-tick. |
| 14432 | updateEffects | `void updateEffects(float param)` | no | visual | Inline per-tick effects. |
| 14433 | updateGlowColor | `void updateGlowColor()` | no | visual | Glow color recompute. |
| 14434 | updateInternalActions | `void updateInternalActions(float dt)` | no | engine-internal | Internal action step. |
| 14435 | updateJump | `void updateJump(float dt)` | no | physics | Per-tick jump update; called from update(). |
| 14436 | updateJumpVariables | `void updateJumpVariables()` | no | physics | Inline. |
| 14437 | updateLastGroundObject | `void updateLastGroundObject(GameObject* object)` | no | state-mutating-player | Writes m_lastGroundObject. |
| 14438 | updateMove | `void updateMove(float dt)` | no | physics | Per-tick move; called from update(). |
| 14439 | updatePlayerArt | `void updatePlayerArt()` | no | visual | Art update. |
| 14440 | updatePlayerBirdFrame | `void updatePlayerBirdFrame(int frame)` | no | visual | Frame setter. |
| 14441 | updatePlayerDartFrame | `void updatePlayerDartFrame(int frame)` | no | visual | Frame setter. |
| 14442 | updatePlayerForce | `void updatePlayerForce(cocos2d::CCPoint velocity, bool additive)` | no | physics | Force update. |
| 14443 | updatePlayerFrame | `void updatePlayerFrame(int frame)` | no | visual | Frame setter. |
| 14444 | updatePlayerGlow | `void updatePlayerGlow()` | no | visual | Glow update. |
| 14445 | updatePlayerJetpackFrame | `void updatePlayerJetpackFrame(int frame)` | no | visual | Frame setter. |
| 14446 | updatePlayerRobotFrame | `void updatePlayerRobotFrame(int frame)` | no | visual | Frame setter. |
| 14447 | updatePlayerRollFrame | `void updatePlayerRollFrame(int frame)` | no | visual | Frame setter. |
| 14448 | updatePlayerScale | `void updatePlayerScale()` | no | visual | Scale recompute. |
| 14449 | updatePlayerShipFrame | `void updatePlayerShipFrame(int frame)` | no | visual | Frame setter. |
| 14450 | updatePlayerSpiderFrame | `void updatePlayerSpiderFrame(int frame)` | no | visual | Frame setter. |
| 14451 | updatePlayerSpriteExtra | `void updatePlayerSpriteExtra(gd::string frameName)` | no | visual | Extra sprite. |
| 14452 | updatePlayerSwingFrame | `void updatePlayerSwingFrame(int frame)` | no | visual | Frame setter. |
| 14453 | updateRobotAnimationSpeed | `void updateRobotAnimationSpeed()` | no | visual | Anim-speed setter. |
| 14454 | updateRotation | `void updateRotation(float dt)` | no | physics | Per-tick rotation. |
| 14455 | updateRotation | `void updateRotation(float dt, float rotation)` | no | physics | Overload. |
| 14456 | updateShipRotation | `void updateShipRotation(float dt)` | no | physics | Per-tick ship rotation. |
| 14457 | updateShipSpriteExtra | `void updateShipSpriteExtra(gd::string frameName)` | no | visual | Extra sprite. |
| 14458 | updateSlopeRotation | `void updateSlopeRotation(float dt)` | no | physics | Per-tick slope rotation. |
| 14459 | updateSlopeYVelocity | `void updateSlopeYVelocity(float yVelocity)` | no | physics | Inline; sim path. |
| 14460 | updateSpecial | `void updateSpecial(float dt)` | no | physics | Inline; sim path. |
| 14461 | updateStateVariables | `void updateStateVariables()` | no | state-mutating-player | Per-tick state update. |
| 14462 | updateStaticForce | `void updateStaticForce(float rotation, float staticForce, bool additive)` | no | physics | Inline. |
| 14463 | updateStreakBlend | `void updateStreakBlend(bool blend)` | no | visual | Streak blend setter. |
| 14464 | updateStreaks | `void updateStreaks(float dt)` | no | visual | Per-tick streak update. |
| 14465 | updateSwingFire | `void updateSwingFire()` | no | visual | Swing fire update. |
| 14466 | updateTimeMod | `void updateTimeMod(float speed, bool noEffects)` | no | physics | **Already hooked** at TrajPlayerObjectHook:421 to gate per-player speed-mod (sim-vs-real). |
| 14467 | usingWallLimitedMode | `bool usingWallLimitedMode()` | no | read-only | Predicate. |
| 14468 | yStartDown | `void yStartDown()` | no | physics | Inline. |
| 14469 | yStartUp | `void yStartUp()` | no | physics | Inline. |

## Fields table

| Line | Name | Type | Classification | Sim impact / notes |
|------|------|------|----------------|-----|
| 14471 | m_mainLayer | `cocos2d::CCNode*` | engine-internal | Parent ref for sprites; engine-managed. |
| 14472 | m_wasTeleported | `bool` | player-physics-state | Set by teleport portals; not in explicit-copy. **Leak candidate** if copyAttributes misses. |
| 14473 | m_fixGravityBug | `bool` | player-physics-state | Bugfix flag; persistent on real. **Leak candidate**. |
| 14474 | m_reverseSync | `bool` | player-physics-state | Reverse-portal sync flag. **Leak candidate**. |
| 14475 | m_yVelocityBeforeSlope | `double` | player-physics-state | Pre-slope velocity cache; not in explicit-copy. **Leak candidate** for slope re-entry mid-sim if uncopied. |
| 14476 | m_dashX | `double` | player-physics-state | **Explicitly cleared** for sim in clearSimRingState (Trajectory.cpp:469). |
| 14477 | m_dashY | `double` | player-physics-state | **Explicitly cleared** in clearSimRingState:470. |
| 14478 | m_dashAngle | `double` | player-physics-state | **Explicitly cleared** in clearSimRingState:471. |
| 14479 | m_dashStartTime | `double` | player-physics-state | **Explicitly cleared** in clearSimRingState:472. |
| 14480 | m_dashRing | `DashRingObject*` | real-player-pointer | **Explicitly cleared** in clearSimRingState:461 (sim shouldn't reference real dash ring). |
| 14481 | m_slopeStartTime | `double` | player-physics-state | Not explicit-copied. **Leak candidate** for slope re-entry. |
| 14482 | m_justPlacedStreak | `bool` | player-physics-state | Streak-place edge. **Leak candidate** (likely visual-only). |
| 14483 | m_maybeLastGroundObject | `cocos2d::CCNode*` | real-player-pointer | Aliases a real-level GameObject; copyAttributes likely covers (shallow ptr). |
| 14484 | m_collisionLogTop | `cocos2d::CCDictionary*` | shared-container | Per-player CCDictionary; engine likely owns per-sim. Used by storeCollision. Verify copyAttributes doesn't shallow-share — if it does, sim writes would mutate real's log. **Leak candidate.** |
| 14485 | m_collisionLogBottom | `cocos2d::CCDictionary*` | shared-container | Same. **Leak candidate.** |
| 14486 | m_collisionLogLeft | `cocos2d::CCDictionary*` | shared-container | Same. **Leak candidate.** |
| 14487 | m_collisionLogRight | `cocos2d::CCDictionary*` | shared-container | Same. **Leak candidate.** |
| 14488 | m_lastCollisionBottom | `int` | player-physics-state | Last-collision-ID by side; likely covered by copyAttributes. |
| 14489 | m_lastCollisionTop | `int` | player-physics-state | Same. |
| 14490 | m_lastCollisionLeft | `int` | player-physics-state | Same. |
| 14491 | m_lastCollisionRight | `int` | player-physics-state | Same. |
| 14492 | m_unk50C | `int` | unknown | Stubbed name; offset 0x50C. |
| 14493 | m_unk510 | `int` | unknown | Stubbed name; offset 0x510. |
| 14494 | m_currentSlope2 | `GameObject*` | real-player-pointer | **Explicitly copied** in runBranch:566 / initSim:651. |
| 14495 | m_preLastGroundObject | `GameObject*` | real-player-pointer | **Explicitly copied** runBranch:565 / initSim:650. |
| 14496 | m_slopeAngle | `float` | player-physics-state | **Explicitly copied** runBranch:557 / initSim:642. |
| 14497 | m_slopeSlidingMaybeRotated | `bool` | player-physics-state | **Explicitly copied** runBranch:558 / initSim:643. |
| 14498 | m_quickCheckpointMode | `bool` | player-physics-state | Quick-checkpoint flag. **Leak candidate**. |
| 14499 | m_collidedObject | `GameObject*` | real-player-pointer | **Explicitly copied** runBranch:567 / initSim:652. |
| 14500 | m_lastGroundObject | `GameObject*` | real-player-pointer | **Explicitly copied** runBranch:564 / initSim:649. |
| 14501 | m_collidingWithLeft | `GameObject*` | real-player-pointer | **Explicitly copied** runBranch:568 / initSim:653. |
| 14502 | m_collidingWithRight | `GameObject*` | real-player-pointer | **Explicitly copied** runBranch:569 / initSim:654. |
| 14503 | m_maybeSavedPlayerFrame | `int` | player-physics-state | Saved-frame buffer. **Leak candidate** if mode-relevant. |
| 14504 | m_scaleXRelated2 | `double` | unknown | Scale-related state. **Leak candidate** if scale-related to physics. |
| 14505 | m_groundYVelocity | `double` | player-physics-state | Ground-Y velocity. **Leak candidate** (slope/ground physics). |
| 14506 | m_yVelocityRelated | `double` | player-physics-state | yVelocity-related. **Leak candidate**. |
| 14507 | m_scaleXRelated3 | `double` | unknown | **Leak candidate** if scale-related. |
| 14508 | m_scaleXRelated4 | `double` | unknown | **Leak candidate**. |
| 14509 | m_scaleXRelated5 | `double` | unknown | **Leak candidate**. |
| 14510 | m_isCollidingWithSlope | `bool` | player-physics-state | Slope-collide flag. **Leak candidate**. |
| 14511 | m_dashFireSprite | `cocos2d::CCSprite*` | engine-internal | Sprite ptr; engine-managed. |
| 14512 | m_isBallRotating | `bool` | player-physics-state | Ball-mode rotation flag. **Leak candidate** for ball-mode sim. |
| 14513 | m_unk669 | `bool` | unknown | Offset 0x669. |
| 14514 | m_currentPotentialSlope | `GameObject*` | real-player-pointer | Slope candidate; **Leak candidate** if uncopied. |
| 14515 | m_currentSlope | `GameObject*` | real-player-pointer | Active slope; **Leak candidate** (note: only m_currentSlope2 is in explicit-copy). |
| 14516 | unk_584 | `double` | unknown | Stubbed offset 0x584. |
| 14517 | m_collidingWithSlopeId | `int` | player-physics-state | Slope ID. **Leak candidate**. |
| 14518 | m_slopeFlipGravityRelated | `bool` | player-physics-state | Slope/gravity flag. **Leak candidate**. |
| 14519 | m_particleSystems | `cocos2d::CCArray*` | engine-internal | Owned particle systems; engine-managed. |
| 14520 | m_slopeAngleRadians | `float` | player-physics-state | Slope angle radian cache. **Leak candidate** (companion to m_slopeAngle which IS copied). |
| 14521 | m_rotateObjectsRelated | `gd::unordered_map<int, GJPointDouble>` | shared-container | Per-player map. Verify copyAttributes shallow vs deep — shallow share would corrupt real. |
| 14522 | m_potentialSlopeMap | `gd::unordered_map<int, GameObject*>` | shared-container | Same; **Leak candidate**. |
| 14523 | m_rotationSpeed | `float` | player-physics-state | Visual rotation speed. |
| 14524 | m_rotateSpeed | `float` | player-physics-state | Same. |
| 14525 | m_isRotating | `bool` | player-physics-state | Rotation flag. |
| 14526 | m_isBallRotating2 | `bool` | player-physics-state | Ball rotation 2. |
| 14527 | m_hasGlow | `bool` | player-physics-state | Glow flag (visual). |
| 14528 | m_isHidden | `bool` | player-physics-state | Hidden flag. |
| 14529 | m_ghostType | `GhostType` | player-physics-state | Ghost-trail type. |
| 14530 | m_ghostTrail | `GhostTrailEffect*` | engine-internal | Trail ptr; engine-managed. |
| 14531 | m_iconSprite | `cocos2d::CCSprite*` | engine-internal | Icon. |
| 14532 | m_iconSpriteSecondary | `cocos2d::CCSprite*` | engine-internal | Icon. |
| 14533 | m_iconSpriteWhitener | `cocos2d::CCSprite*` | engine-internal | Icon. |
| 14534 | m_iconGlow | `cocos2d::CCSprite*` | engine-internal | Icon glow. |
| 14535 | m_vehicleSprite | `cocos2d::CCSprite*` | engine-internal | Vehicle. |
| 14536 | m_vehicleSpriteSecondary | `cocos2d::CCSprite*` | engine-internal | Vehicle. |
| 14537 | m_birdVehicle | `cocos2d::CCSprite*` | engine-internal | UFO vehicle. |
| 14538 | m_vehicleSpriteWhitener | `cocos2d::CCSprite*` | engine-internal | Vehicle. |
| 14539 | m_vehicleGlow | `cocos2d::CCSprite*` | engine-internal | Vehicle glow. |
| 14540 | m_swingFireMiddle | `PlayerFireBoostSprite*` | engine-internal | Swing fire. |
| 14541 | m_swingFireBottom | `PlayerFireBoostSprite*` | engine-internal | Swing fire. |
| 14542 | m_swingFireTop | `PlayerFireBoostSprite*` | engine-internal | Swing fire. |
| 14543 | m_dashSpritesContainer | `cocos2d::CCSprite*` | engine-internal | Dash sprite container. |
| 14544 | m_regularTrail | `cocos2d::CCMotionStreak*` | engine-internal | Motion streak — sim should not addPoint to this; HardStreak::addPoint is suppressed (Hooks.cpp:597) but CCMotionStreak is different. **Leak candidate** if engine pushes to it during sim update(). |
| 14545 | m_shipStreak | `cocos2d::CCMotionStreak*` | engine-internal | Ship streak. **Leak candidate**. |
| 14546 | m_waveTrail | `HardStreak*` | engine-internal | Wave trail; **partially protected** by HardStreak::addPoint hook (Hooks.cpp:597). |
| 14547 | m_speedMultiplier | `double` | player-physics-state | Speed-multiplier; not in explicit-copy. **Leak candidate** (note: m_playerSpeed IS handled via TrajLayerSpeedHook, but this is different). |
| 14548 | m_yStart | `double` | player-physics-state | Start-Y; not in explicit-copy. **Leak candidate**. |
| 14549 | m_gravity | `double` | player-physics-state | Gravity scalar (vs m_gravityMod which IS explicit-copied). **Leak candidate** if uncovered. |
| 14550 | m_trailingParticleLife | `float` | player-physics-state | Particle life setting. |
| 14551 | m_unk648 | `float` | unknown | Offset 0x648. |
| 14552 | m_gameModeChangedTime | `double` | player-physics-state | Time of last mode change. **Leak candidate** (mode-change timing affects physics). |
| 14553 | m_padRingRelated | `bool` | player-physics-state | **Explicitly cleared** in clearSimRingState:474. |
| 14554 | m_maybeReducedEffects | `bool` | player-physics-state | Reduced-effects flag. |
| 14555 | m_maybeIsFalling | `bool` | player-physics-state | Falling cache. **Leak candidate**. |
| 14556 | m_shouldTryPlacingCheckpoint | `bool` | player-physics-state | Checkpoint flag — sim should not write into real. **Leak candidate** (writes affect real's checkpoint). |
| 14557 | m_playEffects | `bool` | player-physics-state | Effects-enable flag. |
| 14558 | m_maybeCanRunIntoBlocks | `bool` | player-physics-state | Run-into-blocks flag. **Leak candidate**. |
| 14559 | m_hasGroundParticles | `bool` | player-physics-state | Particle flag. |
| 14560 | m_hasShipParticles | `bool` | player-physics-state | Particle flag. |
| 14561 | m_isOnGround3 | `bool` | player-physics-state | **Explicitly copied** runBranch:574 / initSim:659. |
| 14562 | m_checkpointTimeout | `bool` | player-physics-state | Checkpoint flag. |
| 14563 | m_lastCheckpointTime | `double` | player-physics-state | Checkpoint time. |
| 14564 | m_lastJumpTime | `double` | player-physics-state | Last-jump time; critical for jump-spam timing. **Leak candidate** if uncopied. |
| 14565 | m_lastFlipTime | `double` | player-physics-state | Last-flip time; gravity-spam timing. **Leak candidate** if uncopied. |
| 14566 | m_flashTime | `double` | player-physics-state | Flash time. |
| 14567 | m_flashDuration | `float` | player-physics-state | Flash dur. |
| 14568 | m_flashDelay | `float` | player-physics-state | Flash delay. |
| 14569 | m_flashMainColor | `cocos2d::ccColor3B` | player-physics-state | Flash color. |
| 14570 | m_flashSecondColor | `cocos2d::ccColor3B` | player-physics-state | Flash color. |
| 14571 | m_lastSpiderFlipTime | `double` | player-physics-state | Spider-flip time; spider-spam timing. **Leak candidate** if uncopied. |
| 14572 | m_unkBool5 | `bool` | unknown | Offset uncertain. |
| 14573 | m_maybeIsVehicleGlowing | `bool` | player-physics-state | Glow flag. |
| 14574 | m_switchWaveTrailColor | `bool` | player-physics-state | Trail-color flag. |
| 14575 | m_practiceDeathEffect | `bool` | player-physics-state | Practice-death flag. |
| 14576 | m_accelerationOrSpeed | `double` | player-physics-state | Accel/speed (platformer?). **Leak candidate** if uncopied. |
| 14577 | m_snapDistance | `double` | player-physics-state | Snap-distance. |
| 14578 | m_ringJumpRelated | `bool` | player-physics-state | **Explicitly cleared** in clearSimRingState:475. |
| 14579 | m_ringRelatedSet | `gd::unordered_set<int>` | shared-container | Per-player set; **explicitly cleared** in clearSimRingState:476. Verify copyAttributes doesn't shallow-share. |
| 14580 | m_objectSnappedTo | `GameObject*` | real-player-pointer | Snap-target. **Leak candidate** if uncopied. |
| 14581 | m_pendingCheckpoint | `CheckpointObject*` | real-player-pointer | Pending checkpoint. **Leak candidate** (sim should never have a real pending checkpoint). |
| 14582 | m_onFlyCheckpointTries | `int` | player-physics-state | Fly-checkpoint counter. |
| 14583 | m_robotSprite | `GJRobotSprite*` | engine-internal | Robot sprite. |
| 14584 | m_spiderSprite | `GJSpiderSprite*` | engine-internal | Spider sprite. |
| 14585 | m_maybeSpriteRelated | `bool` | player-physics-state | Sprite-related flag. |
| 14586 | m_playerGroundParticles | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14587 | m_trailingParticles | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14588 | m_shipClickParticles | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14589 | m_vehicleGroundParticles | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14590 | m_ufoClickParticles | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14591 | m_robotBurstParticles | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14592 | m_dashParticles | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14593 | m_swingBurstParticles1 | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14594 | m_swingBurstParticles2 | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14595 | m_useLandParticles0 | `bool` | player-physics-state | Land-particle flag. |
| 14596 | m_landParticles0 | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14597 | m_landParticles1 | `cocos2d::CCParticleSystemQuad*` | engine-internal | Particles. |
| 14598 | m_landParticlesAngle | `float` | player-physics-state | Particle angle. |
| 14599 | m_landParticleRelatedY | `float` | player-physics-state | Particle-Y. |
| 14600 | m_playerStreak | `int` | player-physics-state | Streak type. |
| 14601 | m_streakStrokeWidth | `float` | player-physics-state | Streak width. |
| 14602 | m_disableStreakTint | `bool` | player-physics-state | Streak-tint. |
| 14603 | m_alwaysShowStreak | `bool` | player-physics-state | Streak flag. |
| 14604 | m_shipStreakType | `ShipStreak` | player-physics-state | Streak type. |
| 14605 | m_slopeRotation | `double` | player-physics-state | Slope rotation. **Leak candidate** (slope physics). |
| 14606 | m_currentSlopeYVelocity | `double` | player-physics-state | Slope Y-vel. **Leak candidate**. |
| 14607 | m_unk3d0 | `double` | unknown | Offset 0x3d0. |
| 14608 | m_blackOrbRelated | `double` | player-physics-state | Black-orb state. **Leak candidate** if uncopied. |
| 14609 | m_unk3e0 | `bool` | unknown | Offset 0x3e0. |
| 14610 | m_unk3e1 | `bool` | unknown | Offset 0x3e1. |
| 14611 | m_isAccelerating | `bool` | player-physics-state | Accel flag. **Leak candidate**. |
| 14612 | m_isCurrentSlopeTop | `bool` | player-physics-state | Slope-top flag. **Leak candidate**. |
| 14613 | m_collidedTopMinY | `double` | player-physics-state | Collision Y. |
| 14614 | m_collidedBottomMaxY | `double` | player-physics-state | Collision Y. |
| 14615 | m_collidedLeftMaxX | `double` | player-physics-state | Collision X. |
| 14616 | m_collidedRightMinX | `double` | player-physics-state | Collision X. |
| 14617 | m_fadeOutStreak | `bool` | player-physics-state | Streak flag. |
| 14618 | m_canPlaceCheckpoint | `bool` | player-physics-state | Checkpoint flag. **Leak candidate** (sim writes affect real). |
| 14619 | m_originalMainColor | `cocos2d::ccColor3B` | player-physics-state | Original color. |
| 14620 | m_originalSecondColor | `cocos2d::ccColor3B` | player-physics-state | Original color. |
| 14621 | m_hasCustomGlowColor | `bool` | player-physics-state | Glow flag. |
| 14622 | m_glowColor | `cocos2d::ccColor3B` | player-physics-state | Glow color. |
| 14623 | m_maybeIsColliding | `bool` | player-physics-state | Collide flag. |
| 14624 | m_jumpBuffered | `bool` | player-physics-state | **Explicitly copied** runBranch:570 / initSim:655. |
| 14625 | m_stateRingJump | `bool` | player-physics-state | **Explicitly cleared** in clearSimRingState:477. |
| 14626 | m_wasJumpBuffered | `bool` | player-physics-state | **Explicitly copied** runBranch:571 / initSim:656. |
| 14627 | m_wasRobotJump | `bool` | player-physics-state | Robot-jump flag. **Leak candidate** (related to fix-robot-mid-air bookmarked bug). |
| 14628 | m_stateJumpBuffered | `unsigned char` | player-physics-state | **Explicitly copied** runBranch:572 / initSim:657. |
| 14629 | m_stateRingJump2 | `bool` | player-physics-state | **Explicitly cleared** in clearSimRingState:478. |
| 14630 | m_touchedRing | `bool` | player-physics-state | **Explicitly cleared** in clearSimRingState:479 and clearPerTickRingOverlap:507. |
| 14631 | m_touchedCustomRing | `bool` | player-physics-state | **Explicitly cleared** in clearSimRingState:480 and clearPerTickRingOverlap:508. |
| 14632 | m_touchedGravityPortal | `bool` | player-physics-state | Gravity-portal touch flag. **Leak candidate** if not reset between sim ticks. |
| 14633 | m_maybeTouchedBreakableBlock | `bool` | player-physics-state | Breakable-block flag. **Leak candidate**. |
| 14634 | m_jumpRelatedAC2 | `geode::SeedValueRSV` | player-physics-state | RSV jump-related. |
| 14635 | m_touchedPad | `bool` | player-physics-state | Pad-touch flag. **Leak candidate** if uncovered. |
| 14636 | m_yVelocity | `double` | player-physics-state | **CRITICAL** physics state — likely covered by copyAttributes (sim's update() reads it). Verify. |
| 14637 | m_fallSpeed | `double` | player-physics-state | Fall speed. **Leak candidate**. |
| 14638 | m_isOnSlope | `bool` | player-physics-state | **Explicitly copied** runBranch:578 / initSim:663. |
| 14639 | m_wasOnSlope | `bool` | player-physics-state | **Explicitly copied** runBranch:579 / initSim:664. |
| 14640 | m_slopeVelocity | `float` | player-physics-state | **Explicitly copied** runBranch:580 / initSim:665. |
| 14641 | m_maybeUpsideDownSlope | `bool` | player-physics-state | **Explicitly copied** runBranch:561 / initSim:646. |
| 14642 | m_isShip | `bool` | player-physics-state | Mode flag. Likely covered by copyAttributes (mode-set has dedicated paths). |
| 14643 | m_isBird | `bool` | player-physics-state | Mode flag. |
| 14644 | m_isBall | `bool` | player-physics-state | Mode flag. |
| 14645 | m_isDart | `bool` | player-physics-state | Mode flag (wave). |
| 14646 | m_isRobot | `bool` | player-physics-state | Mode flag. |
| 14647 | m_isSpider | `bool` | player-physics-state | Mode flag. |
| 14648 | m_isUpsideDown | `bool` | player-physics-state | Gravity flag. |
| 14649 | m_isDead | `bool` | player-physics-state | Dead flag. Sim tracks separately via m_simP1Dead/m_simP2Dead — but if copyAttributes sets sim's m_isDead from real, that could leak. **Leak candidate** for write-back direction. |
| 14650 | m_isOnGround | `bool` | player-physics-state | **Explicitly copied** runBranch:545 / initSim:635. |
| 14651 | m_isGoingLeft | `bool` | player-physics-state | Reverse flag. |
| 14652 | m_isSideways | `bool` | player-physics-state | Sideways-portal flag. **Leak candidate**. |
| 14653 | m_isSwing | `bool` | player-physics-state | Swing mode flag. |
| 14654 | m_reverseRelated | `int` | player-physics-state | Reverse state int. **Leak candidate**. |
| 14655 | m_maybeReverseSpeed | `double` | player-physics-state | Reverse speed. **Leak candidate**. |
| 14656 | m_maybeReverseAcceleration | `double` | player-physics-state | Reverse accel. **Leak candidate**. |
| 14657 | m_xVelocityRelated2 | `float` | player-physics-state | X-vel cache. **Leak candidate**. |
| 14658 | m_isDashing | `bool` | player-physics-state | **Explicitly cleared** in clearSimRingState:468. |
| 14659 | m_unk9e8 | `int` | unknown | Offset 0x9e8. |
| 14660 | m_groundObjectMaterial | `int` | player-physics-state | **Explicitly copied** runBranch:562 / initSim:647. |
| 14661 | m_vehicleSize | `float` | player-physics-state | **Explicitly copied** runBranch:581 / initSim:666. |
| 14662 | m_playerSpeed | `float` | player-physics-state | Speed; **handled separately** via TrajLayerSpeedHook (Hooks.cpp:444). |
| 14663 | m_shipRotation | `cocos2d::CCPoint` | player-physics-state | Ship rotation. **Leak candidate** for ship-mode sim. |
| 14664 | m_lastPortalPos | `cocos2d::CCPoint` | player-physics-state | Last-portal pos. **Leak candidate** (portal physics). |
| 14665 | m_unkUnused3 | `float` | unknown | Likely padding. |
| 14666 | m_isOnGround2 | `bool` | player-physics-state | **Explicitly copied** runBranch:573 / initSim:658. |
| 14667 | m_lastLandTime | `double` | player-physics-state | **Explicitly copied** runBranch:576 / initSim:661. |
| 14668 | m_platformerVelocityRelated | `float` | player-physics-state | Platformer-vel. |
| 14669 | m_maybeIsBoosted | `bool` | player-physics-state | Boosted flag. **Leak candidate**. |
| 14670 | m_scaleXRelatedTime | `double` | player-physics-state | Scale-X time. |
| 14671 | m_decreaseBoostSlide | `bool` | player-physics-state | Boost slide. |
| 14672 | m_unkA29 | `bool` | unknown | Offset 0xA29. |
| 14673 | m_isLocked | `bool` | player-physics-state | Locked flag. |
| 14674 | m_controlsDisabled | `bool` | player-physics-state | Controls flag. **Leak candidate** if uncopied (sim with controls disabled would refuse inputs). |
| 14675 | m_lastGroundedPos | `cocos2d::CCPoint` | player-physics-state | **Explicitly copied** runBranch:577 / initSim:662. |
| 14676 | m_touchingRings | `cocos2d::CCArray*` | shared-container | **Protected via adoptOwnRings** (Trajectory.cpp:245): sim gets its own retained CCArray, restored each runBranch via clearSimRingState:487. |
| 14677 | m_touchedRings | `gd::unordered_set<int>` | shared-container | Per-player set. **Explicitly cleared** in clearSimRingState:495. Verify copyAttributes doesn't shallow-share. |
| 14678 | m_lastActivatedPortal | `GameObject*` | real-player-pointer | Per-player last-portal. **Leak candidate** if uncopied — but layer-level m_lastActivatedPortal1/2 IS covered by LayerStateSnapshot. |
| 14679 | m_hasEverJumped | `bool` | persistent-stat | Jump-ever flag. **Leak candidate** — sim writing to real's would corrupt level-completion stats. Likely fine since sim is its own PlayerObject, but if copyAttributes mirrors and sim writes back via update path that path doesn't exist. |
| 14680 | m_hasEverHitRing | `bool` | persistent-stat | Same. |
| 14681 | m_playerColor1 | `cocos2d::ccColor3B` | player-physics-state | Color. |
| 14682 | m_playerColor2 | `cocos2d::ccColor3B` | player-physics-state | Color. |
| 14683 | m_position | `cocos2d::CCPoint` | player-physics-state | Position cache (vs CCNode's position). **Leak candidate** if drifts from setPosition. |
| 14684 | m_isSecondPlayer | `bool` | player-physics-state | P2 flag — sim is created with player=1 (Trajectory.cpp:237), so m_isSecondPlayer is false on both sims by default. **Leak candidate** if copyAttributes overwrites it from base, since the sim then runs P2 code paths. Check: does runBranch/initSim guard against this? Looking at code, no — sim's m_isSecondPlayer inherits whatever copyAttributes leaves it as. |
| 14685 | m_unkA99 | `bool` | unknown | Offset 0xA99. |
| 14686 | m_totalTime | `double` | persistent-stat | Total-time stat — would be a stat leak if sim writes back. **Leak candidate**. |
| 14687 | m_isBeingSpawnedByDualPortal | `bool` | player-physics-state | Dual-portal spawn flag. **Leak candidate**. |
| 14688 | m_audioScale | `float` | player-physics-state | Audio-scale. |
| 14689 | m_unkAngle1 | `float` | unknown | Angle. |
| 14690 | m_yVelocityRelated3 | `float` | player-physics-state | yVel-cache. **Leak candidate**. |
| 14691 | m_defaultMiniIcon | `bool` | player-physics-state | Mini-icon flag. |
| 14692 | m_swapColors | `bool` | player-physics-state | Color swap. |
| 14693 | m_switchDashFireColor | `bool` | player-physics-state | Dash fire color. |
| 14694 | m_followRelated | `int` | player-physics-state | Follow-related. |
| 14695 | m_playerFollowFloats | `gd::vector<float>` | shared-container | Per-player follow floats; copyAttributes may shallow-share. |
| 14696 | m_unk838 | `float` | unknown | Offset 0x838. |
| 14697 | m_stateOnGround | `int` | player-physics-state | **Explicitly copied** runBranch:563 / initSim:648. |
| 14698 | m_stateUnk | `unsigned char` | unknown | State byte. |
| 14699 | m_stateNoStickX | `unsigned char` | player-physics-state | No-stick-X. **Leak candidate**. |
| 14700 | m_stateNoStickY | `unsigned char` | player-physics-state | No-stick-Y. **Leak candidate**. |
| 14701 | m_stateUnk2 | `unsigned char` | unknown | State byte. |
| 14702 | m_stateBoostX | `int` | player-physics-state | Boost-X. **Leak candidate**. |
| 14703 | m_stateBoostY | `int` | player-physics-state | Boost-Y. **Leak candidate**. |
| 14704 | m_maybeStateForce2 | `int` | player-physics-state | State-force. |
| 14705 | m_stateScale | `int` | player-physics-state | State-scale. |
| 14706 | m_platformerXVelocity | `double` | player-physics-state | Platformer X-vel. **Leak candidate** for platformer-mode levels. |
| 14707 | m_holdingRight | `bool` | player-physics-state | Holding-right (platformer). |
| 14708 | m_holdingLeft | `bool` | player-physics-state | Holding-left. |
| 14709 | m_leftPressedFirst | `bool` | player-physics-state | Left-first order. |
| 14710 | m_scaleXRelated | `double` | unknown | Scale-X cache. **Leak candidate** if physics-relevant. |
| 14711 | m_maybeHasStopped | `bool` | player-physics-state | Stopped flag. |
| 14712 | m_xVelocityRelated | `float` | player-physics-state | X-vel cache. **Leak candidate**. |
| 14713 | m_maybeGoingCorrectSlopeDirection | `bool` | player-physics-state | **Explicitly copied** runBranch:560 / initSim:645. |
| 14714 | m_isSliding | `bool` | player-physics-state | **Explicitly copied** runBranch:555 / initSim:640. |
| 14715 | m_maybeSlopeForce | `double` | player-physics-state | **Explicitly copied** runBranch:556 / initSim:641. |
| 14716 | m_isOnIce | `bool` | player-physics-state | **Explicitly copied** runBranch:559 / initSim:644. |
| 14717 | m_physDeltaRelated | `double` | player-physics-state | Phys-delta. **Leak candidate**. |
| 14718 | m_isOnGround4 | `bool` | player-physics-state | **Explicitly copied** runBranch:575 / initSim:660. |
| 14719 | m_maybeSlidingTime | `int` | player-physics-state | Sliding-time. |
| 14720 | m_maybeSlidingStartTime | `double` | player-physics-state | Sliding-start. |
| 14721 | m_changedDirectionsTime | `double` | player-physics-state | Direction-change time. |
| 14722 | m_slopeEndTime | `double` | player-physics-state | Slope-end time. |
| 14723 | m_isMoving | `bool` | player-physics-state | Moving flag. |
| 14724 | m_platformerMovingLeft | `bool` | player-physics-state | Platformer-left. |
| 14725 | m_platformerMovingRight | `bool` | player-physics-state | Platformer-right. |
| 14726 | m_isSlidingRight | `bool` | player-physics-state | Slide-right. |
| 14727 | m_maybeChangedDirectionAngle | `double` | player-physics-state | Direction-change angle. |
| 14728 | m_unkUnused2 | `double` | unknown | Likely padding. |
| 14729 | m_isPlatformer | `bool` | player-physics-state | Platformer-mode flag. |
| 14730 | m_stateNoAutoJump | `int` | player-physics-state | No-auto-jump state. **Leak candidate**. |
| 14731 | m_stateDartSlide | `int` | player-physics-state | Dart-slide state. **Leak candidate** for wave-on-ground bug area. |
| 14732 | m_stateHitHead | `int` | player-physics-state | Hit-head state. |
| 14733 | m_stateFlipGravity | `int` | player-physics-state | Flip-gravity state. |
| 14734 | m_gravityMod | `float` | player-physics-state | **Explicitly copied** runBranch:544 / initSim:634. |
| 14735 | m_stateForce | `int` | player-physics-state | State-force. |
| 14736 | m_stateForceVector | `cocos2d::CCPoint` | player-physics-state | State-force vector. |
| 14737 | m_affectedByForces | `bool` | player-physics-state | Affected-by-forces. |
| 14738 | m_jumpPadRelated | `gd::map<int, bool>` | shared-container | Per-player pad-state map. **Explicitly cleared** in clearSimRingState:496. Verify copyAttributes doesn't shallow-share. |
| 14739 | m_somethingPlayerSpeedTime | `float` | player-physics-state | Speed-time. |
| 14740 | m_playerSpeedAC | `float` | player-physics-state | **Handled separately** via TrajLayerSpeedHook (Hooks.cpp:467). |
| 14741 | m_fixRobotJump | `bool` | player-physics-state | Robot-jump fix flag. **Leak candidate** (related to bookmarked robot mid-air bug). |
| 14742 | m_holdingButtons | `gd::map<int, bool>` | shared-container | Per-player button state — sim drives via pushButton/releaseButton. Verify copyAttributes doesn't shallow-share (would have sim share real's button state). |
| 14743 | m_inputsLocked | `bool` | player-physics-state | Input-lock. **Leak candidate** if uncopied. |
| 14744 | m_currentRobotAnimation | `gd::string` | player-physics-state | Robot anim name. |
| 14745 | m_gv0123 | `bool` | player-physics-state | Game-var flag. |
| 14746 | m_iconRequestID | `int` | engine-internal | Icon request ID. |
| 14747 | m_robotBatchNode | `cocos2d::CCSpriteBatchNode*` | engine-internal | Batch node. |
| 14748 | m_spiderBatchNode | `cocos2d::CCSpriteBatchNode*` | engine-internal | Batch node. |
| 14749 | m_unk958 | `cocos2d::CCArray*` | unknown | Unknown CCArray at 0x958. Likely shared-container if copyAttributes shallow-copies. |
| 14750 | m_robotFire | `PlayerFireBoostSprite*` | engine-internal | Robot fire sprite. |
| 14751 | m_unkUnused | `int` | unknown | Padding. |
| 14752 | m_gameLayer | `GJBaseGameLayer*` | real-player-pointer | Layer ptr — same on sim and real (the PlayLayer the sim was created against). Read-only use. |
| 14753 | m_parentLayer | `cocos2d::CCLayer*` | engine-internal | Parent layer. |
| 14754 | m_actionManager | `GJActionManager*` | engine-internal | Action manager. |
| 14755 | m_isOutOfBounds | `bool` | player-physics-state | OOB flag. **Leak candidate**. |
| 14756 | m_fallStartY | `float` | player-physics-state | Fall-start-Y. **Leak candidate**. |
| 14757 | m_disablePlayerSqueeze | `bool` | player-physics-state | Squeeze flag. |
| 14758 | m_robotAnimation1Enabled | `bool` | player-physics-state | Anim flag. |
| 14759 | m_robotAnimation2Enabled | `bool` | player-physics-state | Anim flag. |
| 14760 | m_spiderAnimationEnabled | `bool` | player-physics-state | Anim flag. |
| 14761 | m_ignoreDamage | `bool` | player-physics-state | Damage-ignore. **Leak candidate**. |
| 14762 | m_enable22Changes | `bool` | player-physics-state | 2.2 mode flag. |

## Cross-reference with current isolation

### Methods currently hooked for sim (PlayerObject-level)

| Method | Hook location | Behavior |
|--------|---------------|----------|
| `update(float dt)` | src/Hooks.cpp:384 (TrajPlayerObjectHook) | Always calls super; captures frameDelta only when not simulating. |
| `incrementJumps()` | src/Hooks.cpp:389 | Suppress for sim (prevents jump-count stat increment). |
| `playSpiderDashEffect(from, to)` | src/Hooks.cpp:394 | Suppress visual for sim. |
| `playBumpEffect(int, GameObject*)` | src/Hooks.cpp:399 | Suppress audio/visual for sim. |
| `flashPlayer(...)` | src/Hooks.cpp:407 | Suppress visual flash for sim. |
| `updateTimeMod(float, bool)` | src/Hooks.cpp:421 | Block non-sim-player writes; allow sim's own. |

Adjacent layer-level hooks that gate per-player paths (not PlayerObject methods but touch player state):
- `GJBaseGameLayer::flipGravity(player, ...)` — Hooks.cpp:193, force noEffects for sim player.
- `GJBaseGameLayer::updateTimeMod(...)` — Hooks.cpp:444 (TrajLayerSpeedHook), save/restore real speeds + propagate to sim.
- `EnhancedGameObject::activatedByPlayer(player)` — Hooks.cpp:557, spoof flags for sim.
- `EnhancedGameObject::hasBeenActivatedByPlayer(player)` — Hooks.cpp:582, combine sim-set with real-flag.
- `GJBaseGameLayer::playerTouchedRing(player, ring)` — Orbs.cpp:15, drop for non-sim during sim.

### Fields currently copied real -> sim (explicit copies in runBranch ~ Trajectory.cpp:538-590 and initSim ~Trajectory.cpp:631-670)

The following 31 fields are explicitly copied (or set) per sim-run by both runBranch and initSim:

1. `m_gravityMod` (via setter)
2. `m_isOnGround`
3. `m_isSliding`
4. `m_maybeSlopeForce`
5. `m_slopeAngle`
6. `m_slopeSlidingMaybeRotated`
7. `m_isOnIce`
8. `m_maybeGoingCorrectSlopeDirection`
9. `m_maybeUpsideDownSlope`
10. `m_groundObjectMaterial`
11. `m_stateOnGround`
12. `m_lastGroundObject`
13. `m_preLastGroundObject`
14. `m_currentSlope2`
15. `m_collidedObject`
16. `m_collidingWithLeft`
17. `m_collidingWithRight`
18. `m_jumpBuffered`
19. `m_wasJumpBuffered`
20. `m_stateJumpBuffered`
21. `m_isOnGround2`
22. `m_isOnGround3`
23. `m_isOnGround4`
24. `m_lastLandTime`
25. `m_lastGroundedPos`
26. `m_isOnSlope`
27. `m_wasOnSlope`
28. `m_slopeVelocity`
29. `m_vehicleSize`
30. (via setPosition) position
31. (via runBranch only: `setVisible(false)` — sim stays hidden, not copied from base)

Additional sim-only resets in clearSimRingState (Trajectory.cpp:459-497):
- `m_dashRing = nullptr`
- `m_isDashing = false`, `m_dashX = m_dashY = m_dashAngle = m_dashStartTime = 0.0`
- `m_padRingRelated = false`
- `m_ringJumpRelated = false`
- `m_ringRelatedSet.clear()`
- `m_stateRingJump = m_stateRingJump2 = false`
- `m_touchedRing = m_touchedCustomRing = false`
- `m_touchingRings = ownRings; ownRings->removeAllObjects()` (private array)
- `m_touchedRings.clear()`
- `m_jumpPadRelated.clear()`

Per-tick clear in clearPerTickRingOverlap (Trajectory.cpp:499-512):
- `m_touchedRing = false`
- `m_touchedCustomRing = false`
- `m_touchingRings->removeAllObjects()`

### Fields covered by `copyAttributes(base)`

The exact coverage of `copyAttributes` is opaque (engine binary; no source). Based on empirical evidence in the wave-ground-Y-offset hunt (the explicit-copy block in Trajectory.cpp was added BECAUSE these fields were observed missing from copyAttributes), the rule of thumb is:

- The 31 explicit-copy fields above are known/suspected to be MISSED by copyAttributes.
- Position (CCNode-level) is set explicitly via `setPosition` for the same belt-and-suspenders reason.
- All other player-physics-state fields are presumed COVERED by copyAttributes — but this is an unverified assumption.

Fields presumed covered (high confidence — common physics state):
- `m_yVelocity`, `m_isShip`/`m_isBird`/`m_isBall`/`m_isDart`/`m_isRobot`/`m_isSpider`/`m_isUpsideDown`/`m_isSwing` (mode flags), `m_playerSpeed`/`m_playerSpeedAC` (also separately handled by TrajLayerSpeedHook), `m_lastCollisionBottom/Top/Left/Right`, `m_collidedTopMinY/BottomMaxY/LeftMaxX/RightMinX`.

Fields presumed covered (lower confidence — speculative):
- The slope-related companions (`m_slopeAngleRadians`, `m_slopeRotation`, `m_currentSlopeYVelocity`, `m_groundYVelocity`, `m_yVelocityBeforeSlope`), all rotation/visual state, the `m_isDead` flag (sim death is shadowed by m_simP1Dead/m_simP2Dead, but the underlying field may flip), and all the platformer/reverse state fields (`m_platformerXVelocity`, `m_platformerMovingLeft/Right`, `m_reverseRelated`, `m_maybeReverseSpeed`, `m_maybeReverseAcceleration`).

### Leak candidates

Methods that are state-mutating-player / persistent-stat / audio / visual and NOT currently hooked, where engine code may invoke them on the sim during checkCollisions/update. For each, the symptom if sim runs/writes:

**Persistent stat / save-state writes**:
1. `tryPlaceCheckpoint()` (line 14420) — If sim runs this, it spawns a CheckpointObject in PlayLayer, leaking persistent practice-mode state.
2. `incrementJumps()` is already hooked but `m_hasEverJumped`/`m_hasEverHitRing` are still mutable from inside the engine update path — if copyAttributes shallow-mirrors these and sim writes them, the real player's run stats would falsely show jumps the bot took.
3. `m_totalTime` (field 14686) — If updated each tick on sim, sim's totalTime would count even though sim doesn't run on the timeline.

**CCAction / sprite leaks (visual side effects)**:
4. `runBallRotation` / `runBallRotation2` / `runNormalRotation` / `runRotateAction` — schedule CCActions on the sim PlayerObject. If they ever fire from inside `update()` for sim, the CCAction continues past sim end. Symptom: gradual sim rotation drift, or stuck-rotating sim.
5. `animatePlatformerJump` / `stopPlatformerJumpAnimation` / `exitPlatformerAnimateJump` — same CCAction concern.
6. `createRobot` / `createSpider` — would add sprite children to the sim if engine triggers a mode-build path mid-sim. Symptom: sim spawns its own robot/spider sprite that lingers visible after sim ends if `setVisible(false)` doesn't propagate.
7. `playBurstEffect`, `playSpawnEffect`, `spawnCircle*`, `spawnPortalCircle`, `spawnScaleCircle`, `playCompleteEffect`, `playDeathEffect`, `addAllParticles`, `placeStreakPoint`, `activateStreak`, `createFadeOutDartStreak` — particle/visual emission. Symptom: occasional visual artifacts at sim hitbox positions, particle leaks accumulating each plan.

**Shared CCDictionary / map / array — verify copyAttributes is deep**:
8. `m_collisionLogTop/Bottom/Left/Right` — if shallow-copied, sim's `storeCollision` writes into the real player's collision log dictionaries. Symptom: real's collision dedupe logic sees phantom collision IDs from sim runs and skips real collision events.
9. `m_rotateObjectsRelated` (gd::unordered_map) / `m_potentialSlopeMap` — same concern: sim writes during slope/rotated-object resolve would mutate real's map.
10. `m_ringRelatedSet` (gd::unordered_set) — `clearSimRingState` clears it on sim. If copyAttributes shallow-shares, the clear empties real's set too. (Likely safe — gd::unordered_set is typically value-copy on assignment.)
11. `m_touchedRings` (gd::unordered_set) — same; explicit clear.
12. `m_jumpPadRelated` (gd::map) — same.
13. `m_holdingButtons` (gd::map) — sim drives via pushButton; if shared, sim's input flips real's button state. Symptom: holding/releasing the bot's jump key would flip real's m_holdingButtons mid-frame.
14. `m_playerFollowFloats` (gd::vector) — same concern; unclear which engine path appends.
15. `m_unk958` (CCArray*) — same concern; purpose unknown.

**Per-player state that influences physics, not in explicit-copy and presumed-but-unverified covered by copyAttributes**:
16. `m_isSecondPlayer` — sim is `create(player=1, ...)` so this should be false on both sims. If copyAttributes mirrors it from base (so simP2 -> m_isSecondPlayer=true when copying from m_player2), the sim runs P2-specific code paths; that's wanted for accurate prediction. **Verify** by reading m_simP2's m_isSecondPlayer at runtime; the current code doesn't explicitly set it.
17. `m_lastJumpTime`, `m_lastFlipTime`, `m_lastSpiderFlipTime` — time-of-last-action timestamps. If uncopied, sim physics has stale timestamps; jump-spam / flip-spam timing predictions diverge from reality.
18. `m_lastActivatedPortal` (per-player) — last portal the player crossed. **Not** the layer-level m_lastActivatedPortal1/2 (which IS in LayerStateSnapshot). If uncopied, sim's per-player portal-throttle logic is wrong.
19. `m_isSideways`, `m_reverseRelated`, `m_maybeReverseSpeed`, `m_maybeReverseAcceleration`, `m_isGoingLeft` — reverse/sideways state. Uncopied: sim crossing a reverse portal predicts wrong direction.
20. `m_controlsDisabled`, `m_isLocked`, `m_inputsLocked` — input/control gates. Uncopied: sim with stale state ignores or accepts inputs reality wouldn't.
21. `m_speedMultiplier`, `m_gravity` — bulk physics scalars distinct from `m_playerSpeed`/`m_gravityMod`. Uncopied: divergence on any level using triggered speed/gravity modifiers.
22. `m_platformerXVelocity`, `m_platformerMovingLeft`, `m_platformerMovingRight`, `m_holdingLeft`, `m_holdingRight`, `m_leftPressedFirst` — platformer-mode horizontal state. Uncopied: platformer levels would diverge horizontally.
23. `m_wasRobotJump`, `m_fixRobotJump` — bookmarked-bug area. Likely fine since clearSimRingState resets ring-flagged jump state, but robot's mid-air fresh-jump bug may relate to these timestamps/flags being uncopied.
24. `m_isCurrentSlopeTop`, `m_collidingWithSlopeId`, `m_slopeFlipGravityRelated` — slope physics details adjacent to `m_isOnSlope`/`m_slopeAngle`. Uncopied: edge-case slope resolves differ.
25. `m_touchedGravityPortal`, `m_touchedPad`, `m_maybeTouchedBreakableBlock` — single-frame edge flags. Per-tick reset (like ring touches) likely needed if engine consults across ticks.

**Real-player-pointer fields whose stale value could mislead sim**:
26. `m_currentSlope`, `m_currentPotentialSlope`, `m_objectSnappedTo`, `m_pendingCheckpoint`, `m_maybeLastGroundObject` — if uncopied, sim references either nullptr (then drops slope/snap context) or — worse — the previous run's object pointer that may now be invalid.

## Caveats

- `copyAttributes` body is in the engine binary; we have only the symbol. The explicit-copy list in Trajectory.cpp was developed by empirical bisection of sim-vs-real divergences (see docs/issue-wave-ground-y-offset.md), not by reading copyAttributes' source. Some "presumed covered" claims above could be wrong.
- Several `m_*` fields are stubbed `m_unkXxxN` (`m_unk50C`, `m_unk510`, `m_unk669`, `m_unk648`, `m_unk3d0`, `m_unk3e0`, `m_unk3e1`, `m_unk9e8`, `m_unkA29`, `m_unkA99`, `m_unk838`, `m_unk958`, `m_unkAngle1`, `m_unkUnused`, `m_unkUnused2`, `m_unkUnused3`, `m_unkBool5`) — the name encodes only the byte-offset. Marked `unknown` here.
- One non-`m_` field exists: `unk_584` (line 14516, double, offset 0x584).
- Inline methods (`win inline`) have no engine address — they are likely defined directly in headers. Their bodies are still unavailable to us.
- "Likely covered by copyAttributes" is speculation based on common shallow-copy patterns; not verified.
- The reverse-direction concern (sim writes back to real via copyAttributes) is structurally impossible since `copyAttributes` is only invoked as `sim->copyAttributes(base)` (sim is the destination); but the shared-container concern is real because copyAttributes' shallow shallow-pointer copy would have sim's container pointer ALIAS base's. Any sim write through that pointer mutates base.
