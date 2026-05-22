# PlayerCheckpoint — engine's authoritative player-state struct

*`PlayerCheckpoint` is the engine's own "everything that defines a `PlayerObject`'s state at a moment in time" struct (bindings 2.208, lines 14006–14200). `PlayerObject::saveToCheckpoint(PlayerCheckpoint*)` and `PlayerObject::loadFromCheckpoint(PlayerCheckpoint*)` round-trip these fields — they are the engine's own answer to "what is player state?". We can use this round-trip in lieu of (or in addition to) our hand-curated copy lists in `runBranch` / `runPlan::initSim` — this is the `sim-checkpoint-copy` path. The bindings give no inline-doc context around `loadFromCheckpoint` (14328) or `saveToCheckpoint` (14380); the field list IS the documentation.*

## Summary stats
- Total fields in PlayerCheckpoint: **186**
- Fields whose names correspond 1:1 to PlayerObject fields: **180**
- Fields with renamed/related correspondence (name differs slightly but maps): **2** (`m_goingLeft` -> `m_isGoingLeft`, `m_maybeChangedDirectionsAngle` -> `m_maybeChangedDirectionAngle`)
- Fields that look like internal-only checkpoint state: **0** (every checkpoint field has a corresponding PlayerObject field)

## Fields table

| Line | Name | Type | PlayerObject field equivalent | Sim relevance | Notes |
|---|---|---|---|---|---|
| 14014 | `m_position` | `cocos2d::CCPoint` | `m_position` | critical-physics | Player position |
| 14015 | `m_lastPosition` | `cocos2d::CCPoint` | n/a — not in PlayerObject struct list | critical-physics | Likely tracked via base class / collision history; no direct PlayerObject equivalent |
| 14016 | `m_yVelocityUnrounded` | `double` | n/a — not in PlayerObject struct list | critical-physics | Possibly held as local in update path; no direct PlayerObject field |
| 14017 | `m_isUpsideDown` | `bool` | `m_isUpsideDown` | critical-physics | Gravity direction |
| 14018 | `m_isSideways` | `bool` | `m_isSideways` | critical-physics | Sideways mode (H portal) |
| 14019 | `m_isShip` | `bool` | `m_isShip` | mode-specific | Ship mode flag |
| 14020 | `m_isBall` | `bool` | `m_isBall` | mode-specific | Ball mode flag |
| 14021 | `m_isBird` | `bool` | `m_isBird` | mode-specific | UFO mode flag |
| 14022 | `m_isSwing` | `bool` | `m_isSwing` | mode-specific | Swing mode flag |
| 14023 | `m_isDart` | `bool` | `m_isDart` | mode-specific | Wave (dart) mode flag |
| 14024 | `m_isRobot` | `bool` | `m_isRobot` | mode-specific | Robot mode flag |
| 14025 | `m_isSpider` | `bool` | `m_isSpider` | mode-specific | Spider mode flag |
| 14026 | `m_isOnGround` | `bool` | `m_isOnGround` | flags | Primary on-ground flag |
| 14027 | `m_ghostType` | `GhostType` | `m_ghostType` | vehicle | Ghost effect type |
| 14028 | `m_isMini` | `bool` | n/a — not in PlayerObject struct list | mode-specific | Mini-icon size flag; possibly derived from scale on PlayerObject |
| 14029 | `m_playerSpeed` | `float` | `m_playerSpeed` | critical-physics | X-speed multiplier (1x/2x/3x/4x) |
| 14030 | `m_isHidden` | `bool` | `m_isHidden` | flags | Visibility |
| 14031 | `m_goingLeft` | `bool` | `m_isGoingLeft` | critical-physics | RENAMED — checkpoint drops `is` prefix |
| 14032 | `m_maybeReverseSpeed` | `double` | `m_maybeReverseSpeed` | critical-physics | Reverse-portal speed |
| 14033 | `m_jumpBuffered` | `bool` | `m_jumpBuffered` | flags | Jump-buffer state |
| 14034 | `m_isDashing` | `bool` | `m_isDashing` | dash-state | Currently dashing |
| 14035 | `m_dashStartTimeold` | `float` | n/a — not in PlayerObject struct list | dash-state | Older dash-time field; no direct PlayerObject equivalent |
| 14036 | `m_dashX` | `double` | `m_dashX` | dash-state | Dash X channel |
| 14037 | `m_dashY` | `double` | `m_dashY` | dash-state | Dash Y channel |
| 14038 | `m_dashAngle` | `double` | `m_dashAngle` | dash-state | Dash angle |
| 14039 | `m_dashStartTime` | `double` | `m_dashStartTime` | dash-state | Dash start time |
| 14040 | `m_dashRing` | `DashRingObject*` | `m_dashRing` | dash-state | Active dash-ring pointer |
| 14041 | `m_shouldStop` | `bool` | n/a — not in PlayerObject struct list | flags | No direct equivalent on PlayerObject — possibly StopTrigger state |
| 14042 | `m_lastFlipTime` | `double` | `m_lastFlipTime` | transient-physics | Last gravity flip time |
| 14043 | `m_gravityMod` | `float` | `m_gravityMod` | critical-physics | Gravity modifier (1.0, 0.5, etc.) |
| 14044 | `m_objectSnappedTo` | `GameObject*` | `m_objectSnappedTo` | transient-physics | Snap target object |
| 14045 | `m_snapDistance` | `double` | `m_snapDistance` | transient-physics | Snap distance |
| 14046 | `m_accelerationOrSpeed` | `double` | `m_accelerationOrSpeed` | critical-physics | Acceleration/speed param |
| 14047 | `m_decreaseBoostSlide` | `bool` | `m_decreaseBoostSlide` | flags | Boost-slide decrease flag |
| 14048 | `m_followRelated` | `int` | `m_followRelated` | unknown | Follow trigger related |
| 14049 | `m_playerFollowFloats` | `gd::vector<float>` | `m_playerFollowFloats` | unknown | Follow-related float vector |
| 14050 | `m_unk838` | `float` | `m_unk838` | unknown | Unnamed internal field |
| 14051 | `m_isOnSlope` | `bool` | `m_isOnSlope` | transient-physics | On slope flag |
| 14052 | `m_wasOnSlope` | `bool` | `m_wasOnSlope` | transient-physics | Previous slope state |
| 14053 | `m_slopeVelocity` | `float` | `m_slopeVelocity` | transient-physics | Slope-tangent velocity |
| 14054 | `m_rotation` | `float` | n/a — uses CCNode `m_rotation` | mode-specific | Player rotation (CCNode-level; PlayerObject inherits) |
| 14055 | `m_wasTeleported` | `bool` | `m_wasTeleported` | flags | Teleport just-happened flag |
| 14056 | `m_fixGravityBug` | `bool` | `m_fixGravityBug` | flags | Gravity bug fix flag |
| 14057 | `m_reverseSync` | `bool` | `m_reverseSync` | flags | Reverse sync flag |
| 14058 | `m_yVelocityBeforeSlope` | `double` | `m_yVelocityBeforeSlope` | transient-physics | Y-vel before slope contact |
| 14059 | `m_slopeStartTime` | `double` | `m_slopeStartTime` | transient-physics | Slope-contact start time |
| 14060 | `m_justPlacedStreak` | `bool` | `m_justPlacedStreak` | flags | Streak placement flag |
| 14061 | `m_lastCollisionBottom` | `int` | `m_lastCollisionBottom` | transient-physics | Last collision ID — bottom |
| 14062 | `m_lastCollisionTop` | `int` | `m_lastCollisionTop` | transient-physics | Last collision ID — top |
| 14063 | `m_lastCollisionLeft` | `int` | `m_lastCollisionLeft` | transient-physics | Last collision ID — left |
| 14064 | `m_lastCollisionRight` | `int` | `m_lastCollisionRight` | transient-physics | Last collision ID — right |
| 14065 | `m_unk50C` | `int` | `m_unk50C` | unknown | Unnamed |
| 14066 | `m_unk510` | `int` | `m_unk510` | unknown | Unnamed |
| 14067 | `m_currentSlope2` | `GameObject*` | `m_currentSlope2` | transient-physics | Secondary slope pointer |
| 14068 | `m_preLastGroundObject` | `GameObject*` | `m_preLastGroundObject` | transient-physics | Prior ground object |
| 14069 | `m_slopeAngle` | `float` | `m_slopeAngle` | transient-physics | Slope angle (degrees) |
| 14070 | `m_slopeSlidingMaybeRotated` | `bool` | `m_slopeSlidingMaybeRotated` | transient-physics | Slide-on-slope rotated flag |
| 14071 | `m_quickCheckpointMode` | `bool` | `m_quickCheckpointMode` | flags | Quick-checkpoint mode flag |
| 14072 | `m_collidedObject` | `GameObject*` | `m_collidedObject` | transient-physics | Last collided object |
| 14073 | `m_lastGroundObject` | `GameObject*` | `m_lastGroundObject` | transient-physics | Last ground object |
| 14074 | `m_collidingWithLeft` | `GameObject*` | `m_collidingWithLeft` | transient-physics | Left-side collision target |
| 14075 | `m_collidingWithRight` | `GameObject*` | `m_collidingWithRight` | transient-physics | Right-side collision target |
| 14076 | `m_scaleXRelated2` | `double` | `m_scaleXRelated2` | unknown | ScaleX-related field |
| 14077 | `m_groundYVelocity` | `double` | `m_groundYVelocity` | critical-physics | Y-vel at ground contact |
| 14078 | `m_yVelocityRelated` | `double` | `m_yVelocityRelated` | critical-physics | Y-vel related |
| 14079 | `m_scaleXRelated3` | `double` | `m_scaleXRelated3` | unknown | ScaleX-related |
| 14080 | `m_scaleXRelated4` | `double` | `m_scaleXRelated4` | unknown | ScaleX-related |
| 14081 | `m_scaleXRelated5` | `double` | `m_scaleXRelated5` | unknown | ScaleX-related |
| 14082 | `m_isCollidingWithSlope` | `bool` | `m_isCollidingWithSlope` | transient-physics | Slope-collision flag |
| 14083 | `m_isBallRotating` | `bool` | `m_isBallRotating` | mode-specific | Ball-rotation flag |
| 14084 | `m_unk669` | `bool` | `m_unk669` | unknown | Unnamed |
| 14085 | `m_currentPotentialSlope` | `GameObject*` | `m_currentPotentialSlope` | transient-physics | Potential slope ref |
| 14086 | `m_currentSlope` | `GameObject*` | `m_currentSlope` | transient-physics | Active slope ref |
| 14087 | `unk_584` | `double` | `unk_584` | unknown | Unnamed |
| 14088 | `m_collidingWithSlopeId` | `int` | `m_collidingWithSlopeId` | transient-physics | Slope-collision ID |
| 14089 | `m_slopeFlipGravityRelated` | `bool` | `m_slopeFlipGravityRelated` | transient-physics | Slope gravity-flip flag |
| 14090 | `m_slopeAngleRadians` | `float` | `m_slopeAngleRadians` | transient-physics | Slope angle (radians) |
| 14091 | `m_rotationSpeed` | `float` | `m_rotationSpeed` | mode-specific | Rotation speed |
| 14092 | `m_rotateSpeed` | `float` | `m_rotateSpeed` | mode-specific | Rotate speed (cf. rotationSpeed) |
| 14093 | `m_isRotating` | `bool` | `m_isRotating` | mode-specific | Rotating flag |
| 14094 | `m_isBallRotating2` | `bool` | `m_isBallRotating2` | mode-specific | Ball-rotation flag (2nd) |
| 14095 | `m_speedMultiplier` | `double` | `m_speedMultiplier` | critical-physics | Speed multiplier |
| 14096 | `m_yStart` | `double` | `m_yStart` | critical-physics | Y-coord baseline |
| 14097 | `m_gravity` | `double` | `m_gravity` | critical-physics | Gravity magnitude |
| 14098 | `m_trailingParticleLife` | `float` | `m_trailingParticleLife` | unknown | Trail particle life (visual but checkpoint-relevant) |
| 14099 | `m_gameModeChangedTime` | `double` | `m_gameModeChangedTime` | mode-specific | Time of last mode change |
| 14100 | `m_padRingRelated` | `bool` | `m_padRingRelated` | ring-state | Pad/ring flag |
| 14101 | `m_maybeIsFalling` | `bool` | `m_maybeIsFalling` | flags | Probably-falling flag |
| 14102 | `m_shouldTryPlacingCheckpoint` | `bool` | `m_shouldTryPlacingCheckpoint` | flags | Try-place-checkpoint flag |
| 14103 | `m_playEffects` | `bool` | `m_playEffects` | flags | Play visual effects flag |
| 14104 | `m_maybeCanRunIntoBlocks` | `bool` | `m_maybeCanRunIntoBlocks` | flags | Can-run-into-blocks flag |
| 14105 | `m_isOnGround3` | `bool` | `m_isOnGround3` | flags | On-ground variant 3 |
| 14106 | `m_lastSpiderFlipTime` | `double` | `m_lastSpiderFlipTime` | mode-specific | Spider-flip timestamp |
| 14107 | `m_unkBool5` | `bool` | `m_unkBool5` | unknown | Unnamed |
| 14108 | `m_ringJumpRelated` | `bool` | `m_ringJumpRelated` | ring-state | Ring-jump flag |
| 14109 | `m_ringRelatedSet` | `gd::unordered_set<int>` | `m_ringRelatedSet` | ring-state | Ring-related ID set |
| 14110 | `m_maybeSpriteRelated` | `bool` | `m_maybeSpriteRelated` | flags | Sprite-related flag |
| 14111 | `m_landParticlesAngle` | `float` | `m_landParticlesAngle` | unknown | Land-particles angle |
| 14112 | `m_landParticleRelatedY` | `float` | `m_landParticleRelatedY` | unknown | Land-particles Y |
| 14113 | `m_slopeRotation` | `double` | `m_slopeRotation` | transient-physics | Slope rotation amount |
| 14114 | `m_currentSlopeYVelocity` | `double` | `m_currentSlopeYVelocity` | transient-physics | Slope-Y velocity |
| 14115 | `m_unk3d0` | `double` | `m_unk3d0` | unknown | Unnamed |
| 14116 | `m_blackOrbRelated` | `double` | `m_blackOrbRelated` | ring-state | Black-orb related |
| 14117 | `m_unk3e0` | `bool` | `m_unk3e0` | unknown | Unnamed |
| 14118 | `m_unk3e1` | `bool` | `m_unk3e1` | unknown | Unnamed |
| 14119 | `m_isAccelerating` | `bool` | `m_isAccelerating` | flags | Accelerating flag |
| 14120 | `m_isCurrentSlopeTop` | `bool` | `m_isCurrentSlopeTop` | transient-physics | Slope-top flag |
| 14121 | `m_collidedTopMinY` | `double` | `m_collidedTopMinY` | transient-physics | Top collision Y-min |
| 14122 | `m_collidedBottomMaxY` | `double` | `m_collidedBottomMaxY` | transient-physics | Bottom collision Y-max |
| 14123 | `m_collidedLeftMaxX` | `double` | `m_collidedLeftMaxX` | transient-physics | Left collision X-max |
| 14124 | `m_collidedRightMinX` | `double` | `m_collidedRightMinX` | transient-physics | Right collision X-min |
| 14125 | `m_wasJumpBuffered` | `bool` | `m_wasJumpBuffered` | flags | Previous jump-buffer state |
| 14126 | `m_wasRobotJump` | `bool` | `m_wasRobotJump` | mode-specific | Robot-jump prior flag |
| 14127 | `m_stateJumpBuffered` | `unsigned __int8` | `m_stateJumpBuffered` | flags | State jump-buffered |
| 14128 | `m_stateRingJump2` | `bool` | `m_stateRingJump2` | ring-state | State ring-jump (2nd) |
| 14129 | `m_touchedRing` | `bool` | `m_touchedRing` | ring-state | Touched-ring flag |
| 14130 | `m_touchedCustomRing` | `bool` | `m_touchedCustomRing` | ring-state | Touched custom-ring flag |
| 14131 | `m_touchedGravityPortal` | `bool` | `m_touchedGravityPortal` | flags | Gravity-portal touch flag |
| 14132 | `m_maybeTouchedBreakableBlock` | `bool` | `m_maybeTouchedBreakableBlock` | flags | Breakable-block touch flag |
| 14133 | `m_touchedPad` | `bool` | `m_touchedPad` | ring-state | Pad-touch flag |
| 14134 | `m_yVelocity` | `double` | `m_yVelocity` | critical-physics | Y velocity (primary) |
| 14135 | `m_fallSpeed` | `double` | `m_fallSpeed` | critical-physics | Fall speed |
| 14136 | `m_maybeUpsideDownSlope` | `bool` | `m_maybeUpsideDownSlope` | transient-physics | Upside-down-slope flag |
| 14137 | `m_reverseRelated` | `int` | `m_reverseRelated` | critical-physics | Reverse-related int |
| 14138 | `m_maybeReverseAcceleration` | `double` | `m_maybeReverseAcceleration` | critical-physics | Reverse acceleration |
| 14139 | `m_xVelocityRelated2` | `float` | `m_xVelocityRelated2` | critical-physics | X-vel related (2nd) |
| 14140 | `m_shipRotation` | `cocos2d::CCPoint` | `m_shipRotation` | mode-specific | Ship rotation packed-CCPoint |
| 14141 | `m_lastPortalPos` | `cocos2d::CCPoint` | `m_lastPortalPos` | transient-physics | Last portal position |
| 14142 | `m_unkUnused3` | `float` | `m_unkUnused3` | unknown | Unused |
| 14143 | `m_isOnGround2` | `bool` | `m_isOnGround2` | flags | On-ground variant 2 |
| 14144 | `m_lastLandTime` | `double` | `m_lastLandTime` | transient-physics | Last landing time |
| 14145 | `m_platformerVelocityRelated` | `float` | `m_platformerVelocityRelated` | critical-physics | Platformer velocity |
| 14146 | `m_maybeIsBoosted` | `bool` | `m_maybeIsBoosted` | flags | Boosted flag |
| 14147 | `m_scaleXRelatedTime` | `double` | `m_scaleXRelatedTime` | unknown | ScaleX time field |
| 14148 | `m_isLocked` | `bool` | `m_isLocked` | flags | Player-locked flag |
| 14149 | `m_controlsDisabled` | `bool` | `m_controlsDisabled` | flags | Controls-disabled flag |
| 14150 | `m_lastGroundedPos` | `cocos2d::CCPoint` | `m_lastGroundedPos` | transient-physics | Last grounded position |
| 14151 | `m_touchingRings` | `cocos2d::CCArray*` | `m_touchingRings` | ring-state | Array of currently-touching rings |
| 14152 | `m_touchedRings` | `gd::unordered_set<int>` | `m_touchedRings` | ring-state | Set of consumed ring IDs (cooldown) |
| 14153 | `m_lastActivatedPortal` | `GameObject*` | `m_lastActivatedPortal` | transient-physics | Last activated portal |
| 14154 | `m_totalTime` | `double` | `m_totalTime` | critical-physics | Total time elapsed |
| 14155 | `m_yVelocityRelated3` | `float` | `m_yVelocityRelated3` | critical-physics | Y-vel related (3rd) |
| 14156 | `m_defaultMiniIcon` | `bool` | `m_defaultMiniIcon` | mode-specific | Default mini-icon flag |
| 14157 | `m_swapColors` | `bool` | `m_swapColors` | unknown | Color swap flag (visual but checkpoint) |
| 14158 | `m_switchDashFireColor` | `bool` | `m_switchDashFireColor` | dash-state | Dash-fire color swap |
| 14159 | `m_stateOnGround` | `int` | `m_stateOnGround` | flags | State on-ground int |
| 14160 | `m_stateUnk` | `unsigned char` | `m_stateUnk` | unknown | State unk |
| 14161 | `m_stateNoStickX` | `unsigned char` | `m_stateNoStickX` | unknown | State no-stick X |
| 14162 | `m_stateNoStickY` | `unsigned char` | `m_stateNoStickY` | unknown | State no-stick Y |
| 14163 | `m_stateUnk2` | `unsigned char` | `m_stateUnk2` | unknown | State unk 2 |
| 14164 | `m_stateBoostX` | `int` | `m_stateBoostX` | critical-physics | State boost X |
| 14165 | `m_stateBoostY` | `int` | `m_stateBoostY` | critical-physics | State boost Y |
| 14166 | `m_maybeStateForce2` | `int` | `m_maybeStateForce2` | critical-physics | State force (2nd) |
| 14167 | `m_stateScale` | `int` | `m_stateScale` | mode-specific | State scale |
| 14168 | `m_platformerXVelocity` | `double` | `m_platformerXVelocity` | critical-physics | Platformer X velocity |
| 14169 | `m_holdingRight` | `bool` | `m_holdingRight` | flags | Holding-right input |
| 14170 | `m_holdingLeft` | `bool` | `m_holdingLeft` | flags | Holding-left input |
| 14171 | `m_leftPressedFirst` | `bool` | `m_leftPressedFirst` | flags | Left-pressed-first flag |
| 14172 | `m_scaleXRelated` | `double` | `m_scaleXRelated` | unknown | ScaleX-related |
| 14173 | `m_maybeHasStopped` | `bool` | `m_maybeHasStopped` | flags | Has-stopped flag |
| 14174 | `m_xVelocityRelated` | `float` | `m_xVelocityRelated` | critical-physics | X-vel related |
| 14175 | `m_maybeGoingCorrectSlopeDirection` | `bool` | `m_maybeGoingCorrectSlopeDirection` | transient-physics | Correct-slope-direction flag |
| 14176 | `m_isSliding` | `bool` | `m_isSliding` | transient-physics | Sliding flag |
| 14177 | `m_maybeSlopeForce` | `double` | `m_maybeSlopeForce` | transient-physics | Slope force |
| 14178 | `m_isOnIce` | `bool` | `m_isOnIce` | transient-physics | On-ice flag |
| 14179 | `m_physDeltaRelated` | `double` | `m_physDeltaRelated` | critical-physics | Physics-delta related |
| 14180 | `m_isOnGround4` | `bool` | `m_isOnGround4` | flags | On-ground variant 4 |
| 14181 | `m_maybeSlidingTime` | `int` | `m_maybeSlidingTime` | transient-physics | Sliding time |
| 14182 | `m_maybeSlidingStartTime` | `double` | `m_maybeSlidingStartTime` | transient-physics | Slide start time |
| 14183 | `m_changedDirectionsTime` | `double` | `m_changedDirectionsTime` | transient-physics | Direction-change timestamp |
| 14184 | `m_slopeEndTime` | `double` | `m_slopeEndTime` | transient-physics | Slope-end time |
| 14185 | `m_isMoving` | `bool` | `m_isMoving` | flags | Moving flag |
| 14186 | `m_platformerMovingLeft` | `bool` | `m_platformerMovingLeft` | flags | Platformer moving left |
| 14187 | `m_platformerMovingRight` | `bool` | `m_platformerMovingRight` | flags | Platformer moving right |
| 14188 | `m_isSlidingRight` | `bool` | `m_isSlidingRight` | transient-physics | Sliding-right flag |
| 14189 | `m_maybeChangedDirectionsAngle` | `double` | `m_maybeChangedDirectionAngle` | transient-physics | RENAMED — checkpoint has plural `Directions` |
| 14190 | `m_unkUnused2` | `double` | `m_unkUnused2` | unknown | Unused 2 |
| 14191 | `m_stateNoAutoJump` | `int` | `m_stateNoAutoJump` | flags | State no-auto-jump |
| 14192 | `m_stateDartSlide` | `int` | `m_stateDartSlide` | mode-specific | State dart-slide |
| 14193 | `m_stateHitHead` | `int` | `m_stateHitHead` | flags | State hit-head |
| 14194 | `m_stateFlipGravity` | `int` | `m_stateFlipGravity` | flags | State flip-gravity |
| 14195 | `m_stateForce` | `int` | `m_stateForce` | critical-physics | State force |
| 14196 | `m_stateForceVector` | `cocos2d::CCPoint` | `m_stateForceVector` | critical-physics | State force vector |
| 14197 | `m_affectedByForces` | `bool` | `m_affectedByForces` | flags | Force-affected flag |
| 14198 | `m_jumpPadRelated` | `gd::map<int, bool>` | `m_jumpPadRelated` | ring-state | Jump-pad ID -> consumed map |
| 14199 | `m_fallStartY` | `float` | `m_fallStartY` | transient-physics | Fall-start Y position |

Note on `m_isMini`, `m_yVelocityUnrounded`, `m_lastPosition`, `m_dashStartTimeold`, `m_shouldStop`, `m_rotation`: these six PlayerCheckpoint fields have NO directly-named PlayerObject equivalent. `m_rotation` is the CCNode-inherited rotation (PlayerObject::setRotation overrides it). `m_isMini` is likely the engine reconstructing mini-state from `m_vehicleSize` / scale on PlayerObject. The others (`m_yVelocityUnrounded`, `m_lastPosition`, `m_shouldStop`, `m_dashStartTimeold`) are either computed-on-save or live on GameObject/ancestor classes. From the sim's perspective: if `loadFromCheckpoint` writes these fields back into the player, the round-trip is fully closed inside the engine; we just don't have a name to copy them by hand.

## Cross-reference with mod's hand-curated copy list (src/Trajectory.cpp)

### Fields explicitly copied in `runBranch` (lines 544–586) and `runPlan::initSim` (lines 634–667)

Both code paths copy the same set (besides the per-tick boilerplate). The list is:

| Field | Also in PlayerCheckpoint? | Notes |
|---|---|---|
| `m_gravityMod` | Yes (14043) | Redundant under checkpoint path |
| `m_isOnGround` | Yes (14026) | Redundant under checkpoint path |
| `m_isSliding` | Yes (14176) | Redundant under checkpoint path |
| `m_maybeSlopeForce` | Yes (14177) | Redundant under checkpoint path |
| `m_slopeAngle` | Yes (14069) | Redundant under checkpoint path |
| `m_slopeSlidingMaybeRotated` | Yes (14070) | Redundant under checkpoint path |
| `m_isOnIce` | Yes (14178) | Redundant under checkpoint path |
| `m_maybeGoingCorrectSlopeDirection` | Yes (14175) | Redundant under checkpoint path |
| `m_maybeUpsideDownSlope` | Yes (14136) | Redundant under checkpoint path |
| `m_groundObjectMaterial` | **No** | NOT in PlayerCheckpoint; explicit copy still needed |
| `m_stateOnGround` | Yes (14159) | Redundant under checkpoint path |
| `m_lastGroundObject` | Yes (14073) | Redundant under checkpoint path |
| `m_preLastGroundObject` | Yes (14068) | Redundant under checkpoint path |
| `m_currentSlope2` | Yes (14067) | Redundant under checkpoint path |
| `m_collidedObject` | Yes (14072) | Redundant under checkpoint path |
| `m_collidingWithLeft` | Yes (14074) | Redundant under checkpoint path |
| `m_collidingWithRight` | Yes (14075) | Redundant under checkpoint path |
| `m_jumpBuffered` | Yes (14033) | Redundant under checkpoint path |
| `m_wasJumpBuffered` | Yes (14125) | Redundant under checkpoint path |
| `m_stateJumpBuffered` | Yes (14127) | Redundant under checkpoint path |
| `m_isOnGround2` | Yes (14143) | Redundant under checkpoint path |
| `m_isOnGround3` | Yes (14105) | Redundant under checkpoint path |
| `m_isOnGround4` | Yes (14180) | Redundant under checkpoint path |
| `m_lastLandTime` | Yes (14144) | Redundant under checkpoint path |
| `m_lastGroundedPos` | Yes (14150) | Redundant under checkpoint path |
| `m_isOnSlope` | Yes (14051) | Redundant under checkpoint path |
| `m_wasOnSlope` | Yes (14052) | Redundant under checkpoint path |
| `m_slopeVelocity` | Yes (14053) | Redundant under checkpoint path |
| `m_vehicleSize` | **No** | NOT in PlayerCheckpoint; explicit copy still needed |
| `getPosition()` setPosition round-trip | Yes — `m_position` (14014) | Redundant under checkpoint path |

**Of the 30 explicit-copy entries: 28 are redundant under the checkpoint path; 2 (`m_groundObjectMaterial`, `m_vehicleSize`) are NOT in PlayerCheckpoint and MUST remain even after switching to checkpoint round-trip.**

### Fields PlayerCheckpoint covers that the hand-curated list does NOT

The PlayerCheckpoint round-trip would additionally seed the following fields that the legacy `copyAttributes` + explicit-copy list does NOT explicitly handle (some may already be inside `copyAttributes` — bindings don't show its implementation, so this is the conservative "checkpoint covers but legacy code path is silent on" set):

Critical / state-defining fields conspicuously missing from the hand-curated list:

- `m_position`, `m_lastPosition` (the legacy path uses `setPosition` for current position but does not touch `m_lastPosition`)
- `m_yVelocity`, `m_yVelocityUnrounded`, `m_yVelocityRelated`, `m_yVelocityRelated3`, `m_yVelocityBeforeSlope`, `m_groundYVelocity` (relies on `copyAttributes` covering velocity — unverified)
- `m_xVelocityRelated`, `m_xVelocityRelated2`, `m_platformerXVelocity`, `m_platformerVelocityRelated`
- `m_fallSpeed`, `m_fallStartY`
- `m_isUpsideDown`, `m_isSideways`, `m_goingLeft` (`m_isGoingLeft`)
- `m_isShip`, `m_isBall`, `m_isBird`, `m_isSwing`, `m_isDart`, `m_isRobot`, `m_isSpider` (mode flags)
- `m_isDashing`, `m_dashX`, `m_dashY`, `m_dashAngle`, `m_dashStartTime`, `m_dashStartTimeold`, `m_dashRing`, `m_switchDashFireColor` (dash state)
- `m_touchedRing`, `m_touchedCustomRing`, `m_touchedRings`, `m_touchingRings`, `m_ringRelatedSet`, `m_padRingRelated`, `m_ringJumpRelated`, `m_stateRingJump2`, `m_touchedPad`, `m_jumpPadRelated`, `m_blackOrbRelated` (ring/pad state — partially handled by `clearSimRingState(sim)` in legacy path)
- `m_lastActivatedPortal`, `m_lastPortalPos`, `m_touchedGravityPortal` (portal state)
- `m_gravity`, `m_speedMultiplier`, `m_playerSpeed`, `m_accelerationOrSpeed`, `m_maybeReverseSpeed`, `m_maybeReverseAcceleration`, `m_reverseRelated`, `m_reverseSync` (speed/reverse state)
- `m_wasTeleported`, `m_fixGravityBug`, `m_lastFlipTime`, `m_lastSpiderFlipTime` (flip/teleport timestamps)
- `m_holdingLeft`, `m_holdingRight`, `m_leftPressedFirst`, `m_platformerMovingLeft`, `m_platformerMovingRight`, `m_isMoving` (input/movement flags)
- `m_isLocked`, `m_controlsDisabled` (control-lock flags)
- `m_totalTime`, `m_gameModeChangedTime`, `m_changedDirectionsTime`, `m_slopeEndTime`, `m_maybeSlidingStartTime`, `m_slopeStartTime` (timing fields)
- The full `m_state*` family (`m_stateBoostX/Y`, `m_stateForce`, `m_stateForceVector`, `m_stateScale`, `m_stateNoStickX/Y`, `m_stateNoAutoJump`, `m_stateDartSlide`, `m_stateHitHead`, `m_stateFlipGravity`, `m_maybeStateForce2`, `m_stateUnk`, `m_stateUnk2`)
- `m_affectedByForces`, `m_isAccelerating`, `m_maybeIsFalling`, `m_maybeIsBoosted`, `m_maybeHasStopped`, `m_decreaseBoostSlide`, `m_shouldStop`
- All slope sub-state (`m_slopeRotation`, `m_slopeAngleRadians`, `m_currentSlopeYVelocity`, `m_isCurrentSlopeTop`, `m_slopeFlipGravityRelated`, `m_currentSlope`, `m_currentPotentialSlope`, `m_collidingWithSlopeId`)
- All collision-extent fields (`m_collidedTopMinY`, `m_collidedBottomMaxY`, `m_collidedLeftMaxX`, `m_collidedRightMinX`)
- `m_lastCollisionBottom/Top/Left/Right`
- Rotation fields (`m_rotation`, `m_rotationSpeed`, `m_rotateSpeed`, `m_isRotating`, `m_isBallRotating`, `m_isBallRotating2`, `m_shipRotation`)
- `m_objectSnappedTo`, `m_snapDistance`
- `m_isMini`, `m_isHidden`, `m_defaultMiniIcon`, `m_swapColors`, `m_ghostType`
- `m_followRelated`, `m_playerFollowFloats`
- The collection of `m_unk*` / `m_scaleXRelated*` slots whose semantics we don't know

If `copyAttributes` already covers many of these, the redundancy is moot. If it does not, switching to `sim-checkpoint-copy` is a **strict expansion** of coverage.

## Fields PlayerObject has but PlayerCheckpoint does NOT

Walking every `m_*` data member on PlayerObject (lines 14471–14762) and checking against the PlayerCheckpoint field list:

### State-suspect (could affect sim divergence — NOT in checkpoint)

| Field | Type | Why state-suspect |
|---|---|---|
| `m_vehicleSize` (14661) | `float` | Hitbox scale — directly affects collision width/height. Already in hand-curated list; checkpoint does NOT cover this. |
| `m_groundObjectMaterial` (14660) | `int` | Material-specific ground physics (ice / normal). Already in hand-curated list; checkpoint does NOT cover this. |
| `m_isDead` (14649) | `bool` | Liveness flag. Legacy path uses `clearSimDead()` instead. Checkpoint does NOT cover. |
| `m_isPlatformer` (14729) | `bool` | Platformer-mode flag. Checkpoint does NOT cover. |
| `m_stateRingJump` (14625) | `bool` | NOTE — checkpoint has `m_stateRingJump2` (14128) but NOT plain `m_stateRingJump`. State-defining ring-jump flag. |
| `m_maybeIsColliding` (14623) | `bool` | Currently-colliding flag. Checkpoint does NOT cover. |
| `m_jumpRelatedAC2` (14634) | `geode::SeedValueRSV` | Anticheat-seed bound to jump. Checkpoint does NOT cover. |
| `m_fixRobotJump` (14741) | `bool` | Robot-jump fix flag. Checkpoint does NOT cover. |
| `m_unk9e8` (14659) | `int` | Unknown int. Checkpoint does NOT cover. |
| `m_unkA29` (14672) | `bool` | Unknown. Checkpoint does NOT cover. |
| `m_unkA99` (14685) | `bool` | Unknown. Checkpoint does NOT cover. |
| `m_holdingButtons` (14742) | `gd::map<int, bool>` | Input-button hold-state map. Checkpoint does NOT cover. |
| `m_inputsLocked` (14743) | `bool` | Input-lock flag (distinct from `m_controlsDisabled` which IS in checkpoint). |
| `m_hasEverJumped` (14679) | `bool` | "Ever jumped" history flag. Checkpoint does NOT cover. |
| `m_hasEverHitRing` (14680) | `bool` | "Ever hit ring" history flag. Checkpoint does NOT cover. |
| `m_isOutOfBounds` (14755) | `bool` | Out-of-bounds flag. Checkpoint does NOT cover. |
| `m_isBeingSpawnedByDualPortal` (14687) | `bool` | Dual-portal spawn flag. Checkpoint does NOT cover. |
| `m_audioScale` (14688) | `float` | Could affect timing-driven effects. Probably visual. |
| `m_unkAngle1` (14689) | `float` | Unknown angle. Checkpoint does NOT cover. |
| `m_somethingPlayerSpeedTime` (14739) | `float` | Speed-time tracking. Checkpoint does NOT cover. |
| `m_playerSpeedAC` (14740) | `float` | Anticheat speed. Checkpoint does NOT cover. |
| `m_onFlyCheckpointTries` (14582) | `int` | Fly-mode checkpoint retry count. Checkpoint does NOT cover. |
| `m_canPlaceCheckpoint` (14618) | `bool` | Can-place-checkpoint flag. Checkpoint does NOT cover. |
| `m_checkpointTimeout` (14562) | `bool` | Checkpoint timeout. Checkpoint does NOT cover. |
| `m_lastCheckpointTime` (14563) | `double` | Last-checkpoint time. Checkpoint does NOT cover. |
| `m_lastJumpTime` (14564) | `double` | Last-jump time. Checkpoint does NOT cover. |
| `m_maybeSavedPlayerFrame` (14503) | `int` | Saved-frame index. Checkpoint does NOT cover. |
| `m_isSecondPlayer` (14684) | `bool` | P1/P2 identity. Per-player static; would be wrong to copy from base anyway. |
| `m_pendingCheckpoint` (14581) | `CheckpointObject*` | Pending checkpoint. Sim should not have one anyway. |
| `m_streakStrokeWidth` (14601), `m_disableStreakTint` (14602), `m_alwaysShowStreak` (14603), `m_shipStreakType` (14604), `m_playerStreak` (14600) | various | Streak-related. Visual but checkpoint does NOT cover. |
| `m_gv0123` (14745) | `bool` | Game-variable flag. Checkpoint does NOT cover. |
| `m_iconRequestID` (14746) | `int` | Icon-request ID. Likely visual. |
| `m_enable22Changes` (14762) | `bool` | 2.2-changes flag. Likely a config bit. |

### Cosmetic / visual-only / engine-machinery (safe to ignore for sim correctness)

These are visual-children / animation / sprite refs that the engine does not consider state-defining (the checkpoint's omission is correct):

- `m_mainLayer`, `m_iconSprite`, `m_iconSpriteSecondary`, `m_iconSpriteWhitener`, `m_iconGlow`, `m_vehicleSprite`, `m_vehicleSpriteSecondary`, `m_birdVehicle`, `m_vehicleSpriteWhitener`, `m_vehicleGlow`, `m_dashSpritesContainer`, `m_dashFireSprite`
- `m_regularTrail`, `m_shipStreak`, `m_waveTrail` (motion streaks)
- `m_swingFireMiddle/Bottom/Top`, `m_robotFire`
- `m_robotSprite`, `m_spiderSprite`, `m_robotBatchNode`, `m_spiderBatchNode`, `m_robotAnimation1Enabled`, `m_robotAnimation2Enabled`, `m_spiderAnimationEnabled`, `m_currentRobotAnimation`
- `m_ghostTrail`
- All `*Particles*` fields (14586–14597, plus `m_useLandParticles0`)
- `m_particleSystems`, `m_collisionLogTop/Bottom/Left/Right` (CCDictionary collision-log mirrors)
- `m_hasGlow`, `m_glowColor`, `m_hasCustomGlowColor`, `m_originalMainColor`, `m_originalSecondColor`, `m_playerColor1`, `m_playerColor2`, `m_maybeIsVehicleGlowing`, `m_switchWaveTrailColor`
- `m_flashTime`, `m_flashDuration`, `m_flashDelay`, `m_flashMainColor`, `m_flashSecondColor` (flash effect)
- `m_unk648`, `m_unk958`, `m_unkUnused` (unknown internals)
- `m_hasGroundParticles`, `m_hasShipParticles`, `m_fadeOutStreak`
- `m_maybeLastGroundObject` (visual `CCNode*` parallel to `m_lastGroundObject`)
- `m_potentialSlopeMap`, `m_rotateObjectsRelated` (per-tick scratch maps; cleared each frame anyway)
- `m_practiceDeathEffect`
- `m_disablePlayerSqueeze`, `m_ignoreDamage`
- `m_actionManager`, `m_gameLayer`, `m_parentLayer` (parent layer pointers — must stay the layer's pointer, not be overwritten from base)
- `m_position` on PlayerObject (14683) — this is the cached CCPoint field; the checkpoint's `m_position` covers it and `setPosition` syncs the CCNode `m_obPosition`. Sim uses `setPosition(base->getPosition())` already.

### Summary of leak risks

If we drop the hand-curated copy list and rely **solely** on the checkpoint round-trip, the following are the highest-priority leak-risk fields (state-defining, not in PlayerCheckpoint, not handled by sim's separate clear helpers):

1. **`m_vehicleSize`** — hitbox dimensions (already in hand-curated list, would be lost)
2. **`m_groundObjectMaterial`** — ice vs normal physics (already in hand-curated list, would be lost)
3. **`m_stateRingJump`** — ring-jump state distinct from `m_stateRingJump2`
4. **`m_isDead`** — sim-side death tracking (currently handled by `clearSimDead()`)
5. **`m_holdingButtons`** — input button map (already handled by pushButton/releaseButton)
6. **`m_inputsLocked`**, **`m_isPlatformer`**, **`m_fixRobotJump`** — mode/lock state
7. **`m_isOutOfBounds`** — OOB flag
8. **`m_hasEverJumped`**, **`m_hasEverHitRing`** — history flags (could affect ring/jump-related branching)
9. **`m_maybeIsColliding`** — currently-colliding flag (collision-tick-fresh; possibly OK if `resetCollisionLog(true)` covers it)
10. **`m_jumpRelatedAC2`** — anticheat seed bound to jump
11. **`m_lastJumpTime`**, **`m_lastCheckpointTime`** — timing fields
12. **`m_somethingPlayerSpeedTime`**, **`m_playerSpeedAC`** — anticheat/speed timing
13. **`m_isPlatformer`** — platformer-mode flag (if level mixes modes this matters; otherwise constant for level)

## How to use this reference

- **When debugging "sim drifts from real"**: cross-check our explicit-copy list against the checkpoint list. Any field on PlayerObject that's not in **either** list = uncovered state.
- **When considering "should we drop the explicit-copy list and rely fully on checkpoint"**: the "Fields PlayerObject has but PlayerCheckpoint does NOT" section above lists fields you'd LOSE coverage on. At minimum the `sim-checkpoint-copy` path must STILL copy `m_vehicleSize` and `m_groundObjectMaterial` explicitly — those two are state-defining and not in the engine's own checkpoint struct.
- **Recommendation**: treat `sim-checkpoint-copy` as `loadFromCheckpoint(cp)` **plus** the small residual copy of `m_vehicleSize`, `m_groundObjectMaterial`, and any fields from the "State-suspect" subsection above that show up in divergence logs.
