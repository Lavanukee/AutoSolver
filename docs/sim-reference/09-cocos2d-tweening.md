# CCAction tweening — persistent-side-effect audit
*A state-value snapshot can't roll back a running `CCAction`. Cocos2d-x actions are CCObjects that retain a target node and step their own state via `CCActionManager::update(dt)`. Once attached via `CCNode::runAction(...)`, an action lives on the manager's internal hash table indexed by target node — it will keep stepping every engine tick until `isDone()` returns true or someone calls `stopAllActions()`/`stopAction(...)`/`stopActionByTag(...)` on that target. `LayerStateSnapshot` only restores scalar fields (m_cameraPosition, m_levelFlipping, etc.); the action queue is invisible to it. So if sim's super call spawns a tween, the tween will keep applying its delta on the REAL scene-graph node on the very next engine tick after sim ends, undoing the snapshot restore frame-by-frame. The only durable defenses are (a) suppress the action-spawning method at source so the `CCAction*` is never created in the first place, or (b) call `stopAllActions()` on the affected node at the sim boundary. This file enumerates every method that COULD fire a `CCAction` during sim's super call paths so we know what to hook.*

## CCAction hierarchy (Cocos2d.bro)

The full hierarchy as exposed in `Cocos2d.bro`. Indentation = inheritance. All ultimately derive from `cocos2d::CCAction : cocos2d::CCObject`.

```
CCAction (Cocos2d.bro:132)
├── CCFiniteTimeAction (Cocos2d.bro:1466)
│   ├── CCActionInstant (Cocos2d.bro:190)
│   │   ├── CCCallFunc (Cocos2d.bro:563)
│   │   ├── CCFlipX (Cocos2d.bro:1476) / CCFlipY (Cocos2d.bro:1497)
│   │   ├── CCHide (Cocos2d.bro:1642) / CCShow (Cocos2d.bro:3845)
│   │   ├── CCPlace (Cocos2d.bro:3316)
│   │   ├── CCRemoveSelf (Cocos2d.bro:3469)
│   │   ├── CCReuseGrid (Cocos2d.bro:3570) / CCStopGrid (Cocos2d.bro:4208)
│   │   └── CCToggleVisibility (Cocos2d.bro:4677)
│   └── CCActionInterval (Cocos2d.bro:202)
│       ├── CCMoveBy (Cocos2d.bro:2511) -> CCMoveTo (Cocos2d.bro:2525)
│       ├── CCRotateBy (Cocos2d.bro:3606) / CCRotateTo (Cocos2d.bro:3622)
│       ├── CCScaleTo (Cocos2d.bro:3664) -> CCScaleBy (Cocos2d.bro:3652)
│       ├── CCSkewTo (Cocos2d.bro:3884) -> CCSkewBy (Cocos2d.bro:3873)
│       ├── CCFadeIn (Cocos2d.bro:1356) / CCFadeOut (Cocos2d.bro:1367) / CCFadeTo (Cocos2d.bro:1412)
│       ├── CCTintTo (Cocos2d.bro:4540) / CCTintBy (Cocos2d.bro:4528)
│       ├── CCBlink (Cocos2d.bro:528)
│       ├── CCJumpBy (Cocos2d.bro:1711) / CCJumpTo (Cocos2d.bro:1735)
│       ├── CCBezierBy (Cocos2d.bro:502)
│       ├── CCCardinalSplineTo (Cocos2d.bro:644)
│       ├── CCAnimate (Cocos2d.bro:263)
│       ├── CCDelayTime (Cocos2d.bro:790)
│       ├── CCSequence (Cocos2d.bro:3756) / CCSpawn (Cocos2d.bro:3897)
│       ├── CCRepeat (Cocos2d.bro:3529) / CCRepeatForever (Cocos2d.bro:3550) / CCReverseTime (Cocos2d.bro:3579)
│       ├── CCTargetedAction (Cocos2d.bro:4237)
│       ├── CCProgressTo (Cocos2d.bro:3458) / CCProgressFromTo (Cocos2d.bro:3408)
│       ├── CCGridAction (Cocos2d.bro:1610) (and many grid subclasses)
│       ├── CCActionEase (Cocos2d.bro:172)
│       │   ├── CCEaseIn / CCEaseOut / CCEaseInOut (Cocos2d.bro:1182-1212, derived from CCEaseRateAction)
│       │   ├── CCEaseSineIn/InOut/Out (Cocos2d.bro:1230-1262)
│       │   ├── CCEaseExponentialIn/InOut/Out (Cocos2d.bro:1149-1181)
│       │   ├── CCEaseElasticIn/InOut/Out (Cocos2d.bro:1113-1147)
│       │   ├── CCEaseBackIn/InOut/Out (Cocos2d.bro:1022-1054)
│       │   └── CCEaseBounceIn/InOut/Out (Cocos2d.bro:1067-1098)
│       ├── CCActionCamera (Cocos2d.bro:163) -> CCOrbitCamera (Cocos2d.bro:2835)
│       └── CCAccelAmplitude / CCAccelDeccelAmplitude / CCDeccelAmplitude (Cocos2d.bro:104-128, 776)
├── CCFollow (Cocos2d.bro:1516)
└── CCSpeed (Cocos2d.bro:3918)
```

`CCActionTween` (Cocos2d.bro:248) — separate, used by Cocos2d-x as a generic "tween any float property on a node by name" helper. Same persistence rules.

## CCAction subclasses that matter for sim leakage

These are the action types most likely to be spawned by GD code paths sim crosses:

- **CCMoveTo / CCMoveBy** — translates the target node's position over time. Used by camera tweens (`cameraMoveX/Y`, `moveCameraToPos`, `updateStaticCameraPos*`), ground-bar slides (`animateInGroundNew`, `animateInDualGroundNew`, `animateOutGroundNew`), portal-Y tween (`animatePortalY`).
- **CCRotateTo / CCRotateBy** — angle tween. Used for screen rotation (`updateScreenRotation`), ball-mode roll (`runBallRotation`), normal-mode rotation (`runNormalRotation`), object rotate triggers.
- **CCFadeIn / CCFadeOut / CCFadeTo** — opacity tween. Used for flash effects, fade-out streaks, gravity-effect ribbon fade, object opacity triggers, end-animation fades.
- **CCScaleTo / CCScaleBy** — scale tween. Used by spawn-circle/portal-circle visuals, scale-jump bumps (`animatePlatformerJump`), shine/burst effects.
- **CCTintTo / CCTintBy** — color tween. Used by color-flash effects on the player (`flashPlayer`), object selection tints.
- **CCSequence / CCSpawn** — composites of the above. Most "drop-and-fade", "flash + scale", and "rotate-then-callback" routines wrap their tweens in a CCSequence (e.g. `GameToolbox::getDropActionWDelay` at GeometryDash.bro:6954, `getDropActionWEnd` at 6955).
- **CCRepeat / CCRepeatForever** — wraps an action to loop. Catastrophic if leaked into the real game, because nothing else cancels it: only `stopAction(...)` or `stopAllActions()` will end it.
- **CCActionEase\*** (all easing variants) — wraps an inner CCActionInterval and applies an easing function. Used wherever any GD method takes `easingType` / `easingRate` parameters (see signatures with `int easingType, float easingRate` in GeometryDash.bro).
- **CCCallFunc** — fires a selector on completion. Often appended to a sequence as the "cleanup" callback; e.g. `LoadingCircleSprite::fadeInCircle` (GeometryDash.bro:12753). If sim leaks one of these, the callback fires on the real layer after sim ends.
- **CCBlink** — blinks (visibility toggle). Used by some powerup/coin pickup animations.
- **CCAnimate** — frame-cycle animation; used by sprite-based animated objects (e.g. coin spin, animated decorations).

## Cocos2d-side `runAction` mechanics

Method signatures in Cocos2d.bro:

- `CCNode::runAction(CCAction*)` — Cocos2d.bro:2665. Internally calls `CCActionManager::addAction(action, this, !running)`. The action is retained by the manager and indexed by target pointer in a hash table.
- `CCNode::stopAllActions()` — Cocos2d.bro:2682. Calls `CCActionManager::removeAllActionsFromTarget(this)`.
- `CCNode::stopAction(CCAction*)` — Cocos2d.bro:2680. Removes a specific action.
- `CCNode::stopActionByTag(int)` — Cocos2d.bro:2681. Removes the first action with matching tag.
- `CCNode::getActionByTag(int)` — Cocos2d.bro:2658. Look up.
- `CCNode::numberOfRunningActions()` — Cocos2d.bro:2661. Sanity check.
- `CCActionManager::update(dt)` — Cocos2d.bro:229. The engine pump. Walks every (target, action-list) pair, calls `action->step(dt)`, removes done actions.

The manager is held by `CCScheduler`/`CCDirector` and runs once per visit (post-`update(dt)`, pre-render). So every `CCAction` attached to a live scene-graph node ticks once per real-engine frame, regardless of which logical layer/state we think we're in.

**Key insight:** when sim's super call invokes `runAction`, the action is attached to a REAL scene-graph node — the same `m_player1`, `m_cameraNode`, `m_groundLayer`, `m_middleground`, etc. that the real game uses, because sim runs ON the real PlayLayer (not a fork). The action persists past sim's `snap.restore()` because the snapshot only restores VALUE fields (rotations, positions, flags), not the per-node action queue. Next real-engine tick, `CCActionManager::update` finds the leftover sim-spawned action and steps it, immediately undoing the snapshot's value restore.

## GD-side action-spawning methods (the leak surface)

### Layer-level (GJBaseGameLayer / PlayLayer)

Methods in `class GJBaseGameLayer` (GeometryDash.bro:7283) and `class PlayLayer` (GeometryDash.bro:14766) that are signature-likely to spawn a CCAction or call sub-routines that do. Sim crosses these via `updateCamera`/`postUpdate`/`collisionCheckObjects`/etc. super calls.

| File:line | Method | What it tweens | On which node | Currently hooked? | Notes |
| --- | --- | --- | --- | --- | --- |
| GeometryDash.bro:7325 | `updateScreenRotation(float, bool, bool, float duration, int easingType, ...)` | screen rotation angle | `m_objectLayer` / rotation node | **YES** (Hooks.cpp:251) | Suppressed during sim. Wraps `CCRotateTo` + ease. |
| GeometryDash.bro:7376 | `animatePortalY(float fromY, float toY, float duration, float easingRate)` | `m_portalY` bound (camera ceiling) | layer-side tween node | **YES** (Hooks.cpp:322) | Mode-portal Y ceiling tween. |
| GeometryDash.bro:7373 | `animateInDualGroundNew(GameObject*, float height, bool instant, float duration)` | dual-mode ground bar Y | `m_groundLayer2` / `m_groundLayer` | NO (deliberate — see Hooks.cpp:326-332) | Suppressing breaks ball-portal physics; mitigated via `stopAllActions()` + position restore in Trajectory.cpp:205-216 instead. |
| GeometryDash.bro:7374 | `animateInGroundNew(bool, float, bool)` | ground bar Y in dual mode | `m_groundLayer` | NO (deliberate) | Same rationale. |
| GeometryDash.bro:7375 | `animateOutGroundNew(bool instant)` | ground bar Y returning | `m_groundLayer` / `m_groundLayer2` | NO (deliberate) | Same rationale. |
| GeometryDash.bro:7386 | `bumpPlayer(PlayerObject*, EffectGameObject*)` | layer-side bump effect | layer + player | NO | Effect-trigger blocks fire this. Currently un-hooked; `playBumpEffect` on the player IS hooked (Hooks.cpp:399). Investigate whether layer-side `bumpPlayer` is reached during sim. |
| GeometryDash.bro:7389 | `cameraMoveX(float, float duration, float rate, bool)` | `m_cameraPosition.x` | layer | **YES** (Hooks.cpp:337) | Camera tween. |
| GeometryDash.bro:7390 | `cameraMoveY(float, float duration, float rate, bool force)` | `m_cameraPosition.y` | layer | **YES** (Hooks.cpp:341) | Camera tween. |
| GeometryDash.bro:7428 | `exitStaticCamera(bool, bool, float time, int easingType, float easingRate, ...)` | exits static camera with ease | layer | NO | Symmetric counterpart to `updateStaticCameraPos`. Likely fires when sim leaves a custom camera area. Recommended: suppress at source. |
| GeometryDash.bro:7429 | `flipFinished()` | callback after flip; can re-fire animations | layer + groundLayer | NO | Empty inline on most platforms. Called by `toggleFlipped`. Worth confirming. |
| GeometryDash.bro:7490 | `lightningFlash(CCPoint, ccColor3B)` and overloads | brief screen flash sprite + fade-out | layer | **YES** (Hooks.cpp:286, 290) | Both 2-arg and 8-arg overloads gated. |
| GeometryDash.bro:7500 | `moveCameraToPos(CCPoint)` | camera position | layer | **YES** (Hooks.cpp:247) | |
| GeometryDash.bro:7523 | `playFlashEffect(float duration, int flashes, float)` | full-screen color flash | `m_flashNode` (PlayLayer:7982) | **YES** (Hooks.cpp:259) | Wrapped CCSequence(CCFadeIn,CCFadeOut). |
| GeometryDash.bro:7525 | `playSpeedParticle(float timeMod)` | particle burst on speed change | layer | **YES** (Hooks.cpp:301) | |
| GeometryDash.bro:7586 | `resetCamera()` | snaps camera; may use tween | layer | NO | Calls during `playEndAnimationToPos` paths; those ARE gated (Hooks.cpp:55, 60). Unlikely reached on sim outside that gate. |
| GeometryDash.bro:7587 | `resetGradientLayers()` | resets gradient triggers; may run tween | layer | NO | Gradient triggers are normally handled per-tick; unlikely sim-crossable, but worth a debug log to confirm. |
| GeometryDash.bro:7609 | `shakeCamera(float, float, float)` | camera-shake CCSequence wraps brief CCMoveBy(s) | layer | **YES** (Hooks.cpp:243) | |
| GeometryDash.bro:7617 | `spawnParticle(...)` | particle factory | layer | **YES** (Hooks.cpp:271) | Returns nullptr in sim. |
| GeometryDash.bro:7618 | `spawnParticleTrigger(SpawnParticleGameObject*)` | particle factory | layer | **YES** (Hooks.cpp:277) | |
| GeometryDash.bro:7619 | `spawnParticleTrigger(int, CCPoint, float, float)` | particle factory | layer | **YES** (Hooks.cpp:281) | |
| GeometryDash.bro:7623 | `stopAllGroundActions()` | safe op; cancels actions on ground bars | layer's ground bars | n/a (it cancels, doesn't spawn) | Useful as a sim-boundary helper. Worth invoking from `LayerStateSnapshot::restore` in addition to our per-node `stopAllActions()` calls. |
| GeometryDash.bro:7624 | `stopCameraShake()` | cancels shake | layer | n/a | Useful as a sim-boundary helper if shake somehow slips past our `shakeCamera` gate. |
| GeometryDash.bro:7640 | `toggleFlipped(bool, bool noEffects)` | X-flip portal flip animation | layer's main object node | **YES** (Hooks.cpp:231) | We force `noEffects=true` to skip the visual flip tween. |
| GeometryDash.bro:7649 | `triggerAreaEffect(EnterEffectObject*)` | area effect transitions; spawns CCActions for visual phases | layer + objects in area | NO | Area effects (enter/leave). Currently un-hooked. If sim crosses an enter-effect boundary, the visual transition could leak. Investigate via debug log. |
| GeometryDash.bro:7650 | `triggerAreaEffectAnimation(EnterEffectObject*)` | area effect animation | layer + objects | NO | Same class as above. |
| GeometryDash.bro:7657 | `triggerShaderCommand(ShaderGameObject*)` | dispatches to `ShaderLayer::triggerXxx` (all duration-based) | shader layer | NO | All `triggerInvertColor`, `triggerShockLine`, `triggerShockWave`, `triggerBulge`, `triggerLensCircle`, ... in ShaderLayer (GeometryDash.bro:18504-18519) are tween factories. If sim crosses a shader trigger, these can leak. Strong candidate. |
| GeometryDash.bro:7675 | `updateCameraOffsetX(float, float duration, int easingType, float easingRate, ...)` | camera offset X tween | layer | **YES** (Hooks.cpp:345) | |
| GeometryDash.bro:7676 | `updateCameraOffsetY(float, float duration, int easingType, float easingRate, ...)` | camera offset Y tween | layer | **YES** (Hooks.cpp:351) | |
| GeometryDash.bro:7679 | `updateDualGround(PlayerObject*, int mode, bool instant, float duration)` | dual ground bar tween | `m_groundLayer` / `m_groundLayer2` | NO (deliberate, same as `animateInDualGroundNew`) | Calls `animateInDualGroundNew`. |
| GeometryDash.bro:7696 | `updateMGOffsetY(float, float duration, int easingType, float easingRate, ...)` | middleground offset Y tween | `m_middleground` | NO | The state side IS captured/restored (we call `stopAllActions()` on `m_middleground` in Trajectory.cpp:213-215), but suppressing the spawn would be cleaner. Recommended: add hook. |
| GeometryDash.bro:7708 | `updateStaticCameraPos(CCPoint, bool, bool, bool, float time, int easingType, float easingRate)` | static-camera position tween | layer | **YES** (Hooks.cpp:357) | |
| GeometryDash.bro:7709 | `updateStaticCameraPosToGroup(int, bool, bool, bool, float, float duration, int easingType, float easingRate, bool, float)` | follow-group camera tween | layer | **YES** (Hooks.cpp:365) | |
| GeometryDash.bro:7712 | `updateZoom(float zoom, float duration, int easing, float rate, int uniqueID, int controlID)` | camera zoom tween | layer | NO | Likely fires from zoom triggers; can leak. Recommended: suppress at source. |
| GeometryDash.bro:14795 | `playGravityEffect(bool flip)` (PlayLayer override) | spawns gravity-effect particle ribbon + tween | `m_gravityEffects` array | **YES** (Hooks.cpp:69) | Note: GJBaseGameLayer's playGravityEffect is inline and not hookable; we hook the PlayLayer override only. See Hooks.cpp:378. |
| GeometryDash.bro:14831 | `gravityEffectFinished()` | callback when gravity-effect ribbon ends | `m_gravityEffects` element | NO | Called by the leaked ribbon's `CCCallFunc` — only invoked if the ribbon's tween completes. If `playGravityEffect` is suppressed, this won't fire. |
| GeometryDash.bro:14835 | `levelComplete()` | end-of-level cascade (animations, sound) | layer + player + UI | NO (indirect: gated by `destroyPlayer` + `playEndAnimationToPos`) | sim never reaches this if our `destroyPlayer`+`playEndAnimation` gates are working. |
| GeometryDash.bro:14846 | `playEndAnimationToPos(CCPoint)` | end animation: player fly-out, camera move, fade | player + camera + UI | **YES** (Hooks.cpp:55) | |
| GeometryDash.bro:14847 | `playPlatformerEndAnimationToPos(CCPoint, bool)` | platformer-end animation | player + camera + UI | **YES** (Hooks.cpp:60) | |
| GeometryDash.bro:14876 | `spawnCircle()` (PlayLayer level) | spawn a CCCircleWave with a tween | `m_circleWaveArray` | NO | Different from `PlayerObject::spawnCircle`. Triggered by some pickup/event paths. Investigate. |
| GeometryDash.bro:14877 | `spawnFirework()` | end-of-level firework particle + tween | scene tree | NO | Reached only via `levelComplete` path → gated indirectly. |
| GeometryDash.bro:14869 | `showCompleteEffect()` / 14870 `showCompleteText()` | end completion popup with CCSequence animations | UI nodes | NO (indirect) | Gated by destroyPlayer/end-animation gates. |

### Object-level (GameObject / EffectGameObject / EnhancedGameObject)

| File:line | Method | What it tweens | On which node | Currently hooked? | Notes |
| --- | --- | --- | --- | --- | --- |
| GeometryDash.bro:4415 | `EnhancedGameObject::createRotateAction(float angle, int clockwiseDirection)` | endless object rotation | the object itself | NO (indirect) | Object rotate-action setup runs at object setup, not at runtime activation. Rotations are state-driven via `updateRotateAction(dt)` (4422), which integrates against `m_rotationSpeed`. State-tracked — snapshot territory, not a leak source. |
| GeometryDash.bro:4422 | `EnhancedGameObject::updateRotateAction(float dt)` | per-tick rotation step | object | n/a | Not a CCAction. Stateful. |
| GeometryDash.bro:6292 | `GameObject::playDestroyObjectAnim(GJBaseGameLayer*)` | object death anim (scale + fade) wrapped in CCSequence | object | NO | Called by `destroyObject`. We already gate `destroyObject` for sim (Hooks.cpp:118), but for `destroyPlayer` paths the object's death anim might still fire — investigate. |
| GeometryDash.bro:6293/6294 | `GameObject::playPickupAnimation(...)` | coin/secret/key pickup anim (move + fade in CCSequence) | sprite on `m_objectLayer` | NO | Gated indirectly via our `collisionCheckObjects` coin filter (Hooks.cpp:176): coins/keys are dropped from the sim's collision candidate list, so the engine's pickup path can't reach them and won't call playPickupAnimation. Verify by debug log. |
| GeometryDash.bro:6295 | `GameObject::playShineEffect()` | shine sprite + fade tween | object | **YES** (Hooks.cpp:550) | |
| GeometryDash.bro:18504 | `ShaderLayer::triggerInvertColor(float fadeTime, ..., int easingType, float easingRate)` | shader uniform tween (CCActionTween-like) | shader layer | NO | All ShaderLayer triggers are duration-based tweens. Reached from `triggerShaderCommand` on layer. |
| GeometryDash.bro:18514 | `ShaderLayer::triggerShockLine(...)` | shockline shader | shader layer | NO | |
| GeometryDash.bro:18515 | `ShaderLayer::triggerShockWave(...)` | shockwave shader | shader layer | NO | |
| GeometryDash.bro:18496-18519 | `ShaderLayer::triggerBulge/Chromatic*/ColorChange/Glitch/Grayscale/HueShift/InvertColor/LensCircle/MotionBlur*/Pinch*/Pixelate*/RadialBlur/Sepia/SplitScreen*/tweenValue` | various shader uniform tweens | shader layer | NO | Suppressing `triggerShaderCommand` at source on `GJBaseGameLayer` would cover all of these without per-method hooks. |
| GeometryDash.bro:14512 | `EnterEffectInstance::animateValue(int key, float, float, float duration, int easingType, float easingRate, int easingBuffer)` | tween a numeric key on an enter-effect | per-instance state | NO | Called by area-effect activation. If sim crosses an enter-effect zone, the visual side could leak. |

### Player-level (PlayerObject)

| File:line | Method | What it tweens | On which node | Currently hooked? | Notes |
| --- | --- | --- | --- | --- | --- |
| GeometryDash.bro:14267 | `PlayerObject::animatePlatformerJump(float scale)` | scale-pulse on jump in platformer mode | player sprite | NO | Likely a CCSequence(CCScaleBy, CCScaleBy). If sim jumps in platformer mode, leaks to the real player sprite. Recommended: suppress. |
| GeometryDash.bro:14269 | `PlayerObject::bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject*)` | bump anim | player | NO (indirect: `playBumpEffect` IS gated, Hooks.cpp:399) | `noEffects=true` path skips visuals. Sim should pass `noEffects=true` or the wrapper should be gated. Verify call path. |
| GeometryDash.bro:14293 | `PlayerObject::exitPlatformerAnimateJump()` | end of platformer-jump scale pulse | player sprite | NO | Counterpart to `animatePlatformerJump`. Same risk. |
| GeometryDash.bro:14294 | `PlayerObject::fadeOutStreak2(float duration)` | trail fade-out tween | `m_regularTrail` / `m_shipStreak` | NO | Streaks DO normally fade on mode-switch / portal. Currently mitigated implicitly because trails are sim-side (we set them up on `m_simP1`/`m_simP2`). But verify — if the engine fades the real player's streak during sim, leak. |
| GeometryDash.bro:14295 | `PlayerObject::flashPlayer(float, float, ccColor3B, ccColor3B)` | color flash tween | player sprite | **YES** (Hooks.cpp:407) | |
| GeometryDash.bro:14336 | `PlayerObject::playCompleteEffect(bool noEffects, bool instant)` | end-of-level player anim | player sprite | NO (indirect) | Reached only via end-animation gates. |
| GeometryDash.bro:14337 | `PlayerObject::playDeathEffect()` | death particle + scale + fade | player sprite + particles | NO (indirect) | Gated via `destroyPlayer` (Hooks.cpp:50). |
| GeometryDash.bro:14345 | `PlayerObject::playSpawnEffect()` | respawn animation | player sprite | NO | Called from `playerDestroyed`/respawn paths. `destroyPlayer` is gated — confirm `playSpawnEffect` is not reached otherwise. |
| GeometryDash.bro:14346 | `PlayerObject::playSpiderDashEffect(CCPoint, CCPoint)` | spider-dash trail tween | scene tree | **YES** (Hooks.cpp:394) | |
| GeometryDash.bro:14375 | `PlayerObject::runBallRotation(float speed)` | ball-mode roll (CCRotateBy + CCRepeatForever) | player sprite | NO | If sim transitions to/from ball mode the rotation action may leak. **Strong leak candidate**. Recommended: suppress for sim players, or `stopAllActions()` on the player sprite at sim boundary. |
| GeometryDash.bro:14376 | `PlayerObject::runBallRotation2()` | secondary ball rotation | player sprite | NO | Same class. |
| GeometryDash.bro:14378 | `PlayerObject::runNormalRotation(bool, float speed)` | normal-mode rotation (CCRotateBy / CCRepeat) | player sprite | NO | Same class. |
| GeometryDash.bro:14379 | `PlayerObject::runRotateAction(bool ground, int type)` | dispatcher to the above | player sprite | NO | Suppressing this would cover all three. |
| GeometryDash.bro:14384 | `PlayerObject::spawnCircle()` | spawn-circle CCCircleWave + scale tween | scene tree | NO | Called by ring-jump / portal-jump paths. If sim crosses an orb the engine may fire spawnCircle on the real player — see `RingObject::spawnCircle` which we DO gate (Orbs.cpp:33), but PlayerObject::spawnCircle is its own path. Investigate. |
| GeometryDash.bro:14386 | `PlayerObject::spawnDualCircle()` | dual-mode circle | scene tree | NO | Mode-switch path. |
| GeometryDash.bro:14388 | `PlayerObject::spawnPortalCircle(ccColor3B, float)` | portal-circle anim | scene tree | NO | Mode-switch path. If sim crosses a portal, real player's portal circle visual may flash. |
| GeometryDash.bro:14389 | `PlayerObject::spawnScaleCircle()` | scale-circle anim | scene tree | NO | Same class. |
| GeometryDash.bro:14402 | `PlayerObject::stopRotation(bool ground, int type)` | cancels rotation | player | n/a (cancels) | Useful as boundary helper. |
| GeometryDash.bro:14411 | `PlayerObject::toggleGhostEffect(GhostType)` | ghost trail toggle + fade | player | NO | If sim crosses a ghost orb (we DO gate `triggerActivated` on RingObject), this path should not be reached, but worth verifying. |
| GeometryDash.bro:14418 | `PlayerObject::toggleVisibility(bool)` | visibility toggle, may include fade | player sprite | NO | Used in dual-mode entry/exit. Likely benign if `noEffects` flag is honored elsewhere. |

## Currently-mitigated paths

For each method already hooked, location of the hook and the mitigation strategy:

### Layer-level (`src/Hooks.cpp` lines 243-376)

| Method | Hook line | Mitigation |
| --- | --- | --- |
| `shakeCamera` | 243 | early-return during sim |
| `moveCameraToPos` | 247 | early-return |
| `updateScreenRotation` | 251 | early-return |
| `playFlashEffect` | 259 | early-return |
| `spawnParticle` | 271 | return nullptr |
| `spawnParticleTrigger(SpawnParticleGameObject*)` | 277 | early-return |
| `spawnParticleTrigger(int,CCPoint,float,float)` | 281 | early-return |
| `lightningFlash(CCPoint,ccColor3B)` | 286 | early-return |
| `lightningFlash(CCPoint,CCPoint,ccColor3B,float,float,int,bool,float)` | 290 | early-return |
| `playSpeedParticle` | 301 | early-return |
| `animatePortalY` | 322 | early-return |
| `cameraMoveX` | 337 | early-return |
| `cameraMoveY` | 341 | early-return |
| `updateCameraOffsetX` | 345 | early-return |
| `updateCameraOffsetY` | 351 | early-return |
| `updateStaticCameraPos` | 357 | early-return |
| `updateStaticCameraPosToGroup` | 365 | early-return |
| `toggleFlipped` | 231 | force `noEffects=true` |
| `destroyObject` | 118 | mark sim-destroyed, suppress |
| `flipGravity` | 193 | force `noEffects=true` for sim player; block real-player path |
| `playGravityEffect` (PlayLayer) | 69 | early-return |
| `playEndAnimationToPos` | 55 | early-return |
| `playPlatformerEndAnimationToPos` | 60 | early-return |
| `destroyPlayer` | 50 | mark sim-dead instead of destroying |

### Player-level (`src/Hooks.cpp` lines 383-426)

| Method | Hook line | Mitigation |
| --- | --- | --- |
| `playSpiderDashEffect` | 394 | early-return |
| `playBumpEffect` | 399 | early-return |
| `flashPlayer` | 407 | early-return |
| `incrementJumps` | 389 | early-return (stats, not visual, but unwanted) |
| `updateTimeMod` (per-player) | 421 | block for real player when sim crossing a speed portal |

### Object-level (`src/Hooks.cpp` line 549; `src/Orbs.cpp`)

| Method | Hook | Mitigation |
| --- | --- | --- |
| `GameObject::playShineEffect` | Hooks.cpp:550 | early-return |
| `RingObject::triggerActivated` | Orbs.cpp:23 | early-return |
| `RingObject::powerOnObject` | Orbs.cpp:28 | early-return |
| `RingObject::spawnCircle` | Orbs.cpp:33 | early-return |
| `HardStreak::addPoint` | Hooks.cpp:597 | early-return (streak point accumulation) |
| `EffectGameObject::triggerObject` | Hooks.cpp:517 | block all non-speed effects; speed effects: save/restore real player speeds, restore activation flags |
| `EffectGameObject::triggerActivated` | Hooks.cpp:533 | same pattern |
| `EnhancedGameObject::activatedByPlayer` | Hooks.cpp:557 | mark-only for sim player; super skipped |

## stopAllActions() boundaries

Locations where we currently call `stopAllActions()` at sim restore time, in `src/Trajectory.cpp`:

```cpp
// Trajectory.cpp:205-216 (inside LayerStateSnapshot::restore):
if (hadGroundLayer && pl->m_groundLayer) {
    pl->m_groundLayer->stopAllActions();
    pl->m_groundLayer->setPosition(groundLayerPos);
}
if (hadGroundLayer2 && pl->m_groundLayer2) {
    pl->m_groundLayer2->stopAllActions();
    pl->m_groundLayer2->setPosition(groundLayer2Pos);
}
if (hadMiddleground && pl->m_middleground) {
    pl->m_middleground->stopAllActions();
    pl->m_middleground->setPosition(middlegroundPos);
}
```

These nodes are covered:
- `m_groundLayer` (PlayLayer ground bar; mode-portal `animateInGroundNew` tween target)
- `m_groundLayer2` (dual-mode ground bar; `animateInDualGroundNew` target)
- `m_middleground` (parallax middleground; `updateMGOffsetY` target)

**Nodes that may have leaked actions and are NOT currently covered:**

| Node | Reason at risk | Recommended action |
| --- | --- | --- |
| `pl` itself (the PlayLayer / GJBaseGameLayer) | Some camera tweens may attach to the layer node directly (we suppress the spawners, so currently the action set on `pl` should be empty during sim — confirm via `pl->numberOfRunningActions()` debug logging) | Add `pl->stopAllActions()` as defense-in-depth, OR add per-method debug logs and reach for it only if logs show leaks |
| `m_player1` / `m_player2` (real players) | `runBallRotation`, `runNormalRotation`, `animatePlatformerJump`, `exitPlatformerAnimateJump`, `flashPlayer` (gated), `playBumpEffect` (gated), `playSpiderDashEffect` (gated), `playSpawnEffect`, `playCompleteEffect`. Ungated rotation actions are the strongest leak candidates. | Either (a) add hooks for `runBallRotation`/`runNormalRotation`/`animatePlatformerJump`, or (b) `stopAllActions()` on `pl->m_player1` and `pl->m_player2` at sim boundary. Be cautious: real players DO have legitimate rotation actions running from real-player gameplay; stopping them would freeze the real player's roll/rotate visuals. The (a) approach is safer. |
| `m_objectLayer` | `updateScreenRotation` writes a rotation on the object-layer node; suppressed, but stale (pre-sim) rotation tweens would survive a sim entry/exit cycle | Likely safe — sim doesn't add actions here. Confirm with debug log. |
| `m_cameraNode` (or whatever node the engine writes camera transforms to) | Camera tweens are suppressed via `cameraMoveX/Y` etc.; should be empty during sim. | Should be safe; verify. |
| `m_flashNode` (PlayLayer:7982) | Target of `playFlashEffect` CCFadeIn+CCFadeOut sequence; suppressed at source. | Safe given current suppression. |
| `m_gravityEffects` (PlayLayer:7953) array of GravityEffectSprites | Target of `playGravityEffect`'s ribbon tween; suppressed. | Safe. |
| `m_circleWaveArray` (PlayLayer:14937) | Target of PlayLayer `spawnCircle` / spawnFirework / similar wave-spawns | Not currently gated. If sim ever spawns a circle wave, it leaks. Recommended: hook `PlayLayer::spawnCircle` (NOT to be confused with `PlayerObject::spawnCircle` or `RingObject::spawnCircle`). |
| `m_shipStreak`, `m_regularTrail`, `m_waveTrail` (PlayerObject:14544-14546) | Streak/trail nodes attached to the player. Sim runs on `m_simP1`/`m_simP2`, but if the engine ever fires `fadeOutStreak2` on the real player during sim, leak. | Verify via debug log; if firing, add hook. |
| `m_robotSprite`, `m_spiderSprite` (PlayerObject:14583-14584) | Mode-specific sub-sprites that have their own animation pipeline (`AnimatedSpriteDelegate`). Mode-switch tweens like `tweenToAnimation` / `tweenToFrame` (Cocos2d.bro:1036, 1421) attach actions here. | Worth verifying. Likely fine because mode-switch already passes `noEffects`, but sim mode-switches DO physically transition, so the visuals could leak. |

## Leak candidates (unhooked, exhaustive)

These are methods that COULD spawn a CCAction during sim's super call paths and are NOT currently suppressed. Each entry includes call-site context, visual symptom, and recommended fix.

### 1. `GJBaseGameLayer::updateZoom` (GeometryDash.bro:7712)
- **Call site:** Zoom triggers (`EffectGameObject` with zoom action) and camera-mode changes.
- **Action spawned:** Wraps a camera-zoom CCActionInterval with easing.
- **On which node:** Layer (camera zoom is a layer-level transform).
- **Visual symptom:** If sim crosses a zoom trigger, the real game's zoom slowly drifts to the trigger's target value after sim ends.
- **Fix:** Suppress at source. Mirror the `cameraMoveX/Y` pattern.

### 2. `GJBaseGameLayer::exitStaticCamera` (GeometryDash.bro:7428)
- **Call site:** Counterpart to `updateStaticCameraPos` — fires when leaving a static-camera area effect.
- **Action spawned:** Camera-position tween with easing.
- **On which node:** Layer.
- **Visual symptom:** Sim crossing the boundary of a static-camera region causes the real camera to slide to/from the static position.
- **Fix:** Suppress at source.

### 3. `GJBaseGameLayer::updateMGOffsetY` (GeometryDash.bro:7696)
- **Call site:** Middleground Y-offset triggers and area effects.
- **Action spawned:** `m_middleground` Y-position tween with easing.
- **On which node:** `m_middleground`.
- **Visual symptom:** Middleground parallax slides after sim. **Already mitigated reactively** via `stopAllActions()` + position restore (Trajectory.cpp:213), but suppress-at-source would be cleaner.
- **Fix:** Add hook.

### 4. `GJBaseGameLayer::triggerShaderCommand` (GeometryDash.bro:7657) — the entire `ShaderLayer::triggerXxx` family
- **Call site:** Shader trigger objects in the level.
- **Action spawned:** Shader uniform tweens (CCActionTween-like, often wrapping shader-state updates in a CCSequence).
- **On which node:** ShaderLayer.
- **Visual symptom:** Shader effects (chromatic aberration, shockwave, color invert, etc.) slowly drift in/out on the real screen after sim crosses a shader trigger.
- **Fix:** Suppress `triggerShaderCommand` for sim; covers all 16+ shader trigger methods in one shot.

### 5. `PlayerObject::runBallRotation` / `runBallRotation2` / `runNormalRotation` / `runRotateAction` (GeometryDash.bro:14375-14379)
- **Call site:** Mode transitions (cube ↔ ball, ball-roll resume after slope), `updateRotation` post-collision restart.
- **Action spawned:** `CCRepeatForever(CCRotateBy(...))` typical — keeps spinning forever.
- **On which node:** `m_player1` / `m_player2` (the REAL players, because sim's `m_simP1`/`m_simP2` are separate but copyAttributes propagates state mid-tick).
- **Visual symptom:** Real player keeps spinning after sim ends; visually most noticeable in ball mode.
- **Fix:** Gate `runRotateAction` (the dispatcher) for sim players: if the player passed in is a sim player, early-return.

### 6. `PlayerObject::animatePlatformerJump` / `exitPlatformerAnimateJump` (GeometryDash.bro:14267, 14293)
- **Call site:** Platformer-mode jump animation.
- **Action spawned:** CCSequence(CCScaleBy(squash), CCScaleBy(stretch)).
- **On which node:** Player sprite (sim's player; but with platformer mode the action may end up on the real `m_player1` depending on the engine path).
- **Visual symptom:** Real player sprite scales up/down briefly after a sim-only platformer jump.
- **Fix:** Suppress for sim.

### 7. `PlayerObject::spawnCircle` / `spawnDualCircle` / `spawnPortalCircle` / `spawnScaleCircle` (GeometryDash.bro:14384-14389)
- **Call site:** Mode-switch portals, ring-jump effects, dual-mode entry.
- **Action spawned:** Each spawns a CCCircleWave with a scale-up + fade-out CCSequence.
- **On which node:** Scene tree (added as child of player or layer).
- **Visual symptom:** Circle waves briefly appear on screen when sim crosses an orb/portal.
- **Fix:** Suppress for sim. `RingObject::spawnCircle` IS gated, but PlayerObject's variants aren't.

### 8. `PlayLayer::spawnCircle` (GeometryDash.bro:14876)
- **Call site:** Different from `PlayerObject::spawnCircle`. Some end-level / event paths.
- **Action spawned:** CCCircleWave delegate with tween.
- **On which node:** PlayLayer node tree.
- **Visual symptom:** Circle wave on level-end if sim ever crosses such a trigger.
- **Fix:** Likely already gated via end-animation gates; verify and add explicit suppression if not.

### 9. `GameObject::playDestroyObjectAnim` (GeometryDash.bro:6292)
- **Call site:** Object destruction (breakable blocks, fragmenting decorations).
- **Action spawned:** CCSequence(CCScaleTo, CCFadeOut, CCRemoveSelf-like).
- **On which node:** The object.
- **Visual symptom:** Objects sim-destroys play their destroy animation on the real game.
- **Fix:** `destroyObject` is gated at the layer level (Hooks.cpp:118), so `playDestroyObjectAnim` should not fire. But if there's any other path that calls it directly (e.g. some end-of-level cleanup or trigger), this would leak. Add a debug log inside our `destroyObject` hook bypass: if it ever fires, audit further. Recommended fallback: suppress at source via a GameObject hook.

### 10. `GameObject::playPickupAnimation` (GeometryDash.bro:6293, 6294)
- **Call site:** Coin/key/diamond pickup paths.
- **Action spawned:** CCSequence(move-to-corner, fade-out).
- **On which node:** A pickup sprite attached to `m_objectLayer`.
- **Visual symptom:** Floating coin/key sprites appear on the real screen if sim picks one up.
- **Fix:** Currently mitigated indirectly via the `collisionCheckObjects` coin filter (Hooks.cpp:176). Confirm no other code path fires it — debug log inside the hook.

### 11. `EnterEffectInstance::animateValue` (GeometryDash.bro:4512)
- **Call site:** Area effect transitions (fade/move/tint/transform/rotate area effects).
- **Action spawned:** Numerical-key tween on an enter-effect instance.
- **On which node:** Per-instance state, but the visual side dispatches to objects in the area.
- **Visual symptom:** Objects in area effects slowly transition to/from their effect state after sim crosses the area boundary.
- **Fix:** Hook `GJBaseGameLayer::triggerAreaEffect` and/or `triggerAreaEffectAnimation` at source.

### 12. `GJBaseGameLayer::resetGradientLayers` / gradient-layer paths (GeometryDash.bro:7587)
- **Call site:** Gradient trigger activation, level setup.
- **Action spawned:** Gradient overlay opacity/color tweens.
- **On which node:** Gradient layer nodes.
- **Visual symptom:** Color gradient transitions persist on the real screen after sim.
- **Fix:** Investigate via debug log first. If reached during sim, add a hook on `triggerGradientCommand` (GeometryDash.bro:7653).

### 13. `PlayerObject::toggleGhostEffect` (GeometryDash.bro:14411)
- **Call site:** Ghost trail orb activation, mode-specific ghost effects.
- **Action spawned:** Trail fade-in / fade-out + child node lifecycle.
- **On which node:** `m_ghostTrail` (PlayerObject:14530).
- **Visual symptom:** Ghost trail glitches on the real player after sim crosses a ghost orb.
- **Fix:** Suppress for sim players. Note: `RingObject::triggerActivated` is gated (Orbs.cpp:23), but the player-side `toggleGhostEffect` may also be reachable directly from collision paths.

### 14. `PlayerObject::fadeOutStreak2` (GeometryDash.bro:14294)
- **Call site:** Mode switch, end of platformer dash, streak deactivation.
- **Action spawned:** CCFadeOut on `m_regularTrail` / `m_shipStreak` / `m_waveTrail`.
- **On which node:** Streak/trail node attached to the player.
- **Visual symptom:** Streak/trail fades on the real player after sim mode-switches.
- **Fix:** Suppress for sim players, or `stopAllActions()` on the streak nodes at sim boundary.

### 15. `PlayerObject::bumpPlayer` (GeometryDash.bro:14269 — note: layer-side `GJBaseGameLayer::bumpPlayer` at 7386 is a separate method)
- **Call site:** Pad activations, bouncing effect blocks.
- **Action spawned:** Bump animation via `playBumpEffect` (which IS gated, Hooks.cpp:399). But `bumpPlayer` itself runs collision/physics + potentially other anims.
- **On which node:** Player sprite + sub-particles.
- **Visual symptom:** Likely benign if `playBumpEffect` is suppressed, but verify.
- **Fix:** Add a debug log to confirm `bumpPlayer` is reached during sim with a real-player argument.

## Recommended hooks

Sketches in the style of existing `src/Hooks.cpp` entries:

```cpp
// --- Add to TrajBaseLayerHook (GJBaseGameLayer) in src/Hooks.cpp ---

// Camera zoom CCAction. Same class as cameraMoveX/Y.
void updateZoom(float zoom, float duration, int easing, float rate,
                int uniqueID, int controlID) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::updateZoom(zoom, duration, easing, rate, uniqueID, controlID);
}

// Static-camera exit tween (counterpart to updateStaticCameraPos).
void exitStaticCamera(bool exitX, bool exitY, float time, int easingType,
                      float easingRate, bool smoothVelocity,
                      float smoothVelocityMod, bool exitInstant) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::exitStaticCamera(exitX, exitY, time, easingType,
                                      easingRate, smoothVelocity,
                                      smoothVelocityMod, exitInstant);
}

// Middleground Y-offset tween. Belt-and-suspenders to our stopAllActions
// call in Trajectory.cpp:213.
void updateMGOffsetY(float offsetY, float duration, int easingType,
                     float easingRate, int uniqueID, int controlID) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::updateMGOffsetY(offsetY, duration, easingType, easingRate,
                                     uniqueID, controlID);
}

// Shader command dispatcher. Catches every ShaderLayer::triggerXxx in one shot.
void triggerShaderCommand(ShaderGameObject* object) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::triggerShaderCommand(object);
}

// Area effects (fade/move/tint/transform/rotate area effects).
void triggerAreaEffect(EnterEffectObject* object) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::triggerAreaEffect(object);
}
void triggerAreaEffectAnimation(EnterEffectObject* object) {
    if (sim().isSimulating()) return;
    GJBaseGameLayer::triggerAreaEffectAnimation(object);
}

// --- Add to TrajPlayerObjectHook (PlayerObject) in src/Hooks.cpp ---

// Endless rotation actions on the player sprite (CCRepeatForever).
// runRotateAction dispatches to runBallRotation / runBallRotation2 /
// runNormalRotation, so gating it covers all three.
void runRotateAction(bool ground, int type) {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::runRotateAction(ground, type);
}

// Platformer jump squash-and-stretch CCSequence on the player sprite.
void animatePlatformerJump(float scale) {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::animatePlatformerJump(scale);
}
void exitPlatformerAnimateJump() {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::exitPlatformerAnimateJump();
}

// Mode-switch / portal-jump circle waves on the player.
// RingObject::spawnCircle is already gated in Orbs.cpp:33; these are
// the PlayerObject-side variants.
void spawnCircle() {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::spawnCircle();
}
void spawnDualCircle() {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::spawnDualCircle();
}
void spawnPortalCircle(cocos2d::ccColor3B color, float startRadius) {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::spawnPortalCircle(color, startRadius);
}
void spawnScaleCircle() {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::spawnScaleCircle();
}

// Streak/trail fade-out.
void fadeOutStreak2(float duration) {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::fadeOutStreak2(duration);
}

// Ghost trail toggle.
void toggleGhostEffect(GhostType type) {
    if (sim().isSimPlayer(this)) return;
    PlayerObject::toggleGhostEffect(type);
}
```

## Reference: how to detect action leaks in practice

1. **Per-method debug log.** Add to each suspect method before super:
   ```cpp
   if (sim().isSimulating()) {
       geode::log::debug("[sim-action-leak] {} fired during sim", __func__);
   }
   ```
   Any line that fires during a normal trajectory search indicates a real leak path reached by sim's super call paths but not currently suppressed.

2. **`numberOfRunningActions()` watchdog.** At the start of `LayerStateSnapshot::restore`, query `pl->numberOfRunningActions()`, `pl->m_player1->numberOfRunningActions()`, etc. Compare against the count captured pre-sim. Any positive delta = a leak.

3. **Run with all current suppressions in place, plus per-method debug logs on the unhooked candidates above.** Trigger trajectory search in a level that contains each candidate's trigger type (zoom triggers, shader triggers, area effects, ball portals, platformer mode segments, ghost orbs). Any log that fires identifies the smoking gun.

4. **Visual smoke test.** Run a trajectory search on `Sonic Wave Infinity` or similar, watching the screen. Any "ghost movement" of the camera, ground bar, parallax middleground, screen shake, color flash, etc., after the bot finishes computing = a leak that bypassed our current suppression net.

5. **Tag-based leak isolation.** If a specific leak is hard to attribute, temporarily wrap the suspect spawn with a tagged action: `action->setTag(0xCAFE)` before `runAction`. After sim, walk `pl->m_player1->getActionByTag(0xCAFE)` and friends; any hit identifies which method spawned the leaked action.
