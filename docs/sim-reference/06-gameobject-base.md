# GameObject family base classes — sim-relevance audit

*Every object placed in a level inherits from `GameObject`. Animated/interactive ones (orbs, rings, animated decor) inherit further from `EnhancedGameObject`. Triggers and special effects inherit from `EffectGameObject`. The mod has class-level hooks on each (TrajGameObjectHook for shine effect, TrajEnhancedHook for orb activation, TrajEffectHook for triggers). Bindings file is `_deps/bindings-src/bindings/2.208/GeometryDash.bro`; `GameObject` declared at line 6085, `EnhancedGameObject` at line 4387, `EffectGameObject` at line 4010 (note: derived classes appear earlier in the file than their bases).*

## GameObject

### Summary
- Methods: 158 total (1 hooked — `playShineEffect`)
  - Constructors/destructor: 2
  - Static factories/helpers: 5
  - Virtual: 86
  - Non-virtual member: 165 (counting overloads of `addRotation`, `slopeYPos`, `getObjectRect`)
- Fields: 175 `m_*` members

### Methods table

| Line | Name | Signature | Virtual? | Classification | Sim impact / notes |
|------|------|-----------|----------|----------------|--------------------|
| 6086 | `GameObject` | `GameObject()` | no | engine-internal | constructor |
| 6087 | `~GameObject` | `~GameObject()` | no | engine-internal | destructor |
| 6089 | `createWithFrame` | `static GameObject* (char const*)` | static | engine-internal | factory; not called during sim |
| 6090 | `createWithKey` | `static GameObject* (int)` | static | engine-internal | factory |
| 6091 | `isBasicEnterEffect` | `static bool (int id)` | static | read-only | id classifier |
| 6092 | `objectFromVector` | `static GameObject* (vector<string>&, vector<void*>&, GJBaseGameLayer*, bool)` | static | engine-internal | level load |
| 6093 | `resetMID` | `static void ()` | static | engine-internal | resets monotonic ID counter |
| 6095 | `update` | `void (float dt)` | virtual | visual | called on visible objects each frame; base impl is a stub (`inline`) |
| 6096 | `setScaleX` | `void (float)` | virtual | state-mutating-self | writes `m_scaleX` + cocos node scale; persistent if leaked |
| 6097 | `setScaleY` | `void (float)` | virtual | state-mutating-self | writes `m_scaleY` + cocos node scale |
| 6098 | `setScale` | `void (float)` | virtual | state-mutating-self | wraps both setScaleX/Y |
| 6099 | `setPosition` | `void (CCPoint const&)` | virtual | state-mutating-self | writes `m_positionX/Y` and cocos position; persistent on shared node |
| 6100 | `setVisible` | `void (bool)` | virtual | visual | dirtifies sprite tree |
| 6101 | `setRotation` | `void (float)` | virtual | state-mutating-self | writes rotation; affects hitbox via OBB |
| 6102 | `setRotationX` | `void (float)` | virtual | state-mutating-self | same |
| 6103 | `setRotationY` | `void (float)` | virtual | state-mutating-self | same |
| 6104 | `setOpacity` | `void (unsigned char)` | virtual | visual | propagates to color sprite/glow |
| 6105 | `initWithTexture` | `bool (CCTexture2D*)` | virtual | engine-internal | init only |
| 6106 | `setChildColor` | `void (ccColor3B const&)` | virtual | visual | color sprite update |
| 6107 | `setFlipX` | `void (bool)` | virtual | state-mutating-self | writes `m_isFlipX` |
| 6108 | `setFlipY` | `void (bool)` | virtual | state-mutating-self | writes `m_isFlipY` |
| 6109 | `firstSetup` | `void ()` | virtual | engine-internal | base is no-op |
| 6110 | `customSetup` | `void ()` | virtual | engine-internal | level-load path |
| 6111 | `setupCustomSprites` | `void (string)` | virtual | engine-internal | sprite tree build |
| 6112 | `addMainSpriteToParent` | `void (bool reorder)` | virtual | visual | parents the color sprite |
| 6113 | `resetObject` | `void ()` | virtual | state-mutating-self | engine resets `m_isActivated`, position, etc. on respawn; CALLED during PlayLayer::resetLevel which the sim wraps |
| 6114 | `triggerObject` | `void (GJBaseGameLayer*, int, vector<int>const*)` | virtual | engine-internal | base is no-op; overridden in EffectGameObject |
| 6115 | `activateObject` | `void ()` | virtual | state-mutating-self | sets `m_isActivated = true` and possibly visual side effects |
| 6116 | `deactivateObject` | `void (bool)` | virtual | state-mutating-self | inverse of activateObject |
| 6117 | `transferObjectRect` | `void (CCRect&)` | virtual | read-only | writes into the caller-supplied rect |
| 6118 | `getObjectRect` | `CCRect const& ()` | virtual | read-only | hot path for collision |
| 6119 | `getObjectRect` | `CCRect (float, float)` | virtual | read-only | overload returning by value |
| 6120 | `getObjectRect2` | `CCRect const& (float, float)` | virtual | read-only | mutates cached `m_objectRect` (lazy compute) |
| 6121 | `getObjectTextureRect` | `CCRect const& ()` | virtual | read-only | returns `m_textureRect` |
| 6122 | `getRealPosition` | `CCPoint ()` | virtual | read-only | reads position + offsets |
| 6123 | `setStartPos` | `void (CCPoint)` | virtual | state-mutating-self | one-shot writer; called at load |
| 6124 | `updateStartValues` | `void ()` | virtual | state-mutating-self | snapshots current → start fields |
| 6125 | `customObjectSetup` | `void (vector<string>&, vector<void*>&)` | virtual | engine-internal | level load |
| 6126 | `getSaveString` | `string (GJBaseGameLayer*)` | virtual | engine-internal | editor save |
| 6127 | `claimParticle` | `void ()` | virtual | visual | attaches particle child |
| 6128 | `unclaimParticle` | `void ()` | virtual | visual | detaches particle |
| 6129 | `particleWasActivated` | `void ()` | virtual | visual | sets `m_isParticleSpriteLocked` |
| 6130 | `isFlipX` | `bool ()` | virtual | read-only | accessor |
| 6131 | `isFlipY` | `bool ()` | virtual | read-only | accessor |
| 6132 | `setRScaleX` | `void (float)` | virtual | state-mutating-self | rotation-aware scale |
| 6133 | `setRScaleY` | `void (float)` | virtual | state-mutating-self | |
| 6134 | `setRScale` | `void (float)` | virtual | state-mutating-self | |
| 6135 | `getRScaleX` | `float ()` | virtual | read-only | |
| 6136 | `getRScaleY` | `float ()` | virtual | read-only | |
| 6137 | `setRRotation` | `void (float)` | virtual | state-mutating-self | |
| 6138 | `triggerActivated` | `void (float xPosition)` | virtual | engine-internal | base no-op; overridden in EffectGameObject |
| 6139 | `setObjectColor` | `void (ccColor3B const&)` | virtual | state-mutating-layer | writes `m_baseColor`/`m_detailColor` (which point into layer's effect manager) |
| 6140 | `setGlowColor` | `void (ccColor3B const&)` | virtual | visual | glow sprite color |
| 6141 | `restoreObject` | `void ()` | virtual | state-mutating-self | reverts pulse/move state; called on object reset |
| 6142 | `animationTriggered` | `void ()` | virtual | engine-internal | base no-op |
| 6143 | `selectObject` | `void (ccColor3B)` | virtual | visual | editor selection highlight |
| 6144 | `activatedByPlayer` | `void (PlayerObject*)` | virtual | state-mutating-self | base no-op; overridden in EnhancedGameObject. **HOOKED at EnhancedGameObject** |
| 6145 | `hasBeenActivatedByPlayer` | `bool (PlayerObject*)` | virtual | read-only | base no-op (returns false); overridden in EnhancedGameObject. **HOOKED at EnhancedGameObject** |
| 6146 | `hasBeenActivated` | `bool ()` | virtual | read-only | base reads `m_isActivated` |
| 6147 | `getOrientedBox` | `OBB2D* ()` | virtual | read-only | returns `m_orientedBox`; lazily allocates on first call |
| 6148 | `updateOrientedBox` | `void ()` | virtual | state-mutating-self | recomputes `m_orientedBox` from position/rotation/scale; cached |
| 6149 | `getObjectRotation` | `float ()` | virtual | read-only | |
| 6150 | `updateMainColor` | `void (ccColor3B const&)` | virtual | state-mutating-self | overload writing m_groupColor (the base method) |
| 6151 | `updateSecondaryColor` | `void (ccColor3B const&)` | virtual | state-mutating-self | |
| 6152 | `addToGroup` | `int (int id)` | virtual | state-mutating-self | grows `m_groups[]` |
| 6153 | `removeFromGroup` | `void (int id)` | virtual | state-mutating-self | shrinks `m_groups[]` |
| 6154 | `saveActiveColors` | `void ()` | virtual | state-mutating-self | snapshots colors for pulse-trigger restore |
| 6155 | `spawnXPosition` | `float ()` | virtual | read-only | base returns `m_unmodifiedPositionX`; overridden in EffectGameObject |
| 6156 | `canAllowMultiActivate` | `bool ()` | virtual | read-only | base false |
| 6157 | `blendModeChanged` | `void ()` | virtual | visual | base no-op |
| 6158 | `updateParticleColor` | `void (ccColor3B const&)` | virtual | visual | particle child color |
| 6159 | `updateParticleOpacity` | `void (unsigned char)` | virtual | visual | particle opacity |
| 6160 | `updateMainParticleOpacity` | `void (unsigned char)` | virtual | visual | base no-op |
| 6161 | `updateSecondaryParticleOpacity` | `void (unsigned char)` | virtual | visual | base no-op |
| 6162 | `canReverse` | `bool ()` | virtual | read-only | base false; overridden in EffectGameObject |
| 6163 | `isSpecialSpawnObject` | `bool ()` | virtual | read-only | base false |
| 6164 | `canBeOrdered` | `bool ()` | virtual | read-only | base false |
| 6165 | `getObjectLabel` | `CCLabelBMFont* ()` | virtual | read-only | base returns nullptr |
| 6166 | `setObjectLabel` | `void (CCLabelBMFont*)` | virtual | state-mutating-self | base no-op |
| 6167 | `shouldDrawEditorHitbox` | `bool ()` | virtual | read-only | editor only |
| 6168 | `getHasSyncedAnimation` | `bool ()` | virtual | read-only | base false |
| 6169 | `getHasRotateAction` | `bool ()` | virtual | read-only | base false |
| 6170 | `canMultiActivate` | `bool (bool)` | virtual | read-only | base false |
| 6171 | `updateTextKerning` | `void (int)` | virtual | state-mutating-self | base no-op (TextGameObject) |
| 6172 | `getTextKerning` | `int ()` | virtual | read-only | base 0 |
| 6173 | `getObjectRectDirty` | `bool () const` | virtual | read-only | |
| 6174 | `setObjectRectDirty` | `void (bool)` | virtual | state-mutating-self | |
| 6175 | `getOrientedRectDirty` | `bool () const` | virtual | read-only | |
| 6176 | `setOrientedRectDirty` | `void (bool)` | virtual | state-mutating-self | |
| 6177 | `getType` | `GameObjectType () const` | virtual | read-only | reads `m_objectType` |
| 6178 | `setType` | `void (GameObjectType)` | virtual | state-mutating-self | |
| 6179 | `getStartPos` | `CCPoint () const` | virtual | read-only | |
| 6181 | `addColorSprite` | `void (string)` | no | visual | builds color sprite child |
| 6182 | `addColorSpriteToParent` | `void ()` | no | visual | |
| 6183 | `addColorSpriteToSelf` | `void ()` | no | visual | |
| 6184 | `addCustomBlackChild` | `CCSprite* (string, float, bool)` | no | visual | |
| 6185 | `addCustomChild` | `CCSprite* (string, CCPoint, int)` | no | visual | |
| 6186 | `addCustomColorChild` | `CCSprite* (string)` | no | visual | |
| 6187 | `addEmptyGlow` | `void ()` | no | visual | |
| 6188 | `addGlow` | `void (string)` | no | visual | |
| 6189 | `addInternalChild` | `CCSprite* (CCSprite*, string, CCPoint, int)` | no | visual | |
| 6190 | `addInternalCustomColorChild` | `CCSprite* (string, CCPoint, int)` | no | visual | |
| 6191 | `addInternalGlowChild` | `CCSprite* (string, CCPoint)` | no | visual | |
| 6192 | `addNewSlope01` | `void (bool)` | no | visual | |
| 6193 | `addNewSlope01Glow` | `void (bool)` | no | visual | |
| 6194 | `addNewSlope02` | `void (bool)` | no | visual | |
| 6195 | `addNewSlope02Glow` | `void (bool)` | no | visual | |
| 6196 | `addRotation` | `void (float)` | no | state-mutating-self | rotates by delta |
| 6197 | `addRotation` | `void (float, float)` | no | state-mutating-self | overload |
| 6198 | `addToColorGroup` | `void (int)` | no | state-mutating-self | grows `m_colorGroups[]` |
| 6199 | `addToCustomScaleX` | `void (float)` | no | state-mutating-self | |
| 6200 | `addToCustomScaleY` | `void (float)` | no | state-mutating-self | |
| 6201 | `addToOpacityGroup` | `void (int)` | no | state-mutating-self | grows `m_opacityGroups[]` |
| 6202 | `addToTempOffset` | `void (double, double)` | no | state-mutating-self | move trigger uses this |
| 6203 | `assignUniqueID` | `void ()` | no | state-mutating-self | sets `m_uniqueID` |
| 6204 | `belongsToGroup` | `bool (int)` | no | read-only | |
| 6205 | `calculateOrientedBox` | `void ()` | no | state-mutating-self | populates `m_orientedBox` |
| 6206 | `canChangeCustomColor` | `bool ()` | no | read-only | |
| 6207 | `canChangeMainColor` | `bool ()` | no | read-only | |
| 6208 | `canChangeSecondaryColor` | `bool ()` | no | read-only | |
| 6209 | `canRotateFree` | `bool ()` | no | read-only | |
| 6210 | `colorForMode` | `ccColor3B const& (int, bool)` | no | read-only | |
| 6211 | `commonInteractiveSetup` | `void ()` | no | engine-internal | |
| 6212 | `commonSetup` | `void ()` | no | engine-internal | |
| 6213 | `copyGroups` | `void (GameObject*)` | no | state-mutating-self | level-load only |
| 6214 | `createAndAddParticle` | `CCParticleSystemQuad* (int, char const*, int, tCCPositionType)` | no | visual | creates child node — leak risk |
| 6215 | `createColorGroupContainer` | `void (int)` | no | state-mutating-self | |
| 6216 | `createGlow` | `void (string)` | no | visual | |
| 6217 | `createGroupContainer` | `void (int)` | no | state-mutating-self | |
| 6218 | `createOpacityGroupContainer` | `void (int)` | no | state-mutating-self | |
| 6219 | `createSpriteColor` | `void (int)` | no | visual | |
| 6220 | `deselectObject` | `void ()` | no | visual | editor only |
| 6221 | `destroyObject` | `void ()` | no | state-mutating-self | sets `m_isDisabled`; PlayLayer::destroyObject calls this |
| 6222 | `determineSlopeDirection` | `void ()` | no | state-mutating-self | computes `m_slopeDirection` |
| 6223 | `didScaleXChange` | `bool ()` | no | read-only | |
| 6224 | `didScaleYChange` | `bool ()` | no | read-only | |
| 6225 | `dirtifyObjectPos` | `void ()` | no | state-mutating-self | sets `m_isObjectPosDirty` |
| 6226 | `dirtifyObjectRect` | `void ()` | no | state-mutating-self | sets `m_isObjectRectDirty` |
| 6227 | `disableObject` | `void ()` | no | state-mutating-self | sets `m_isDisabled2` |
| 6228 | `dontCountTowardsLimit` | `bool ()` | no | read-only | |
| 6229 | `duplicateAttributes` | `void (GameObject*)` | no | state-mutating-self | editor copy |
| 6230 | `duplicateColorMode` | `void (GameObject*)` | no | state-mutating-self | editor copy |
| 6231 | `duplicateValues` | `void (GameObject*)` | no | state-mutating-self | editor copy |
| 6232 | `editorColorForCustomMode` | `ccColor3B (int)` | no | read-only | editor only |
| 6233 | `editorColorForMode` | `ccColor3B (int)` | no | read-only | editor only |
| 6234 | `fastRotateObject` | `void (float)` | no | state-mutating-self | |
| 6235 | `getActiveColorForMode` | `ccColor3B const& (int, bool)` | no | read-only | |
| 6236 | `getBallFrame` | `char const* (int)` | no | read-only | |
| 6237 | `getBoundingRect` | `CCRect ()` | no | read-only | |
| 6238 | `getBoxOffset` | `CCPoint const& ()` | no | read-only | |
| 6239 | `getColorFrame` | `string (string)` | no | read-only | |
| 6240 | `getColorIndex` | `int ()` | no | read-only | |
| 6241 | `getColorKey` | `string (bool, bool)` | no | read-only | |
| 6242 | `getCustomZLayer` | `ZLayer ()` | no | read-only | |
| 6243 | `getGlowFrame` | `string (string)` | no | read-only | |
| 6244 | `getGroupDisabled` | `bool ()` | no | read-only | |
| 6245 | `getGroupID` | `int (int)` | no | read-only | |
| 6246 | `getGroupString` | `string ()` | no | read-only | |
| 6247 | `getLastPosition` | `CCPoint const& ()` | no | read-only | |
| 6248 | `getMainColor` | `GJSpriteColor* ()` | no | read-only | |
| 6249 | `getMainColorMode` | `int ()` | no | read-only | |
| 6250 | `getObjectDirection` | `int ()` | no | read-only | |
| 6251 | `getObjectRadius` | `float ()` | no | read-only | |
| 6252 | `getObjectRectPointer` | `CCRect* ()` | no | read-only | |
| 6253 | `getObjectZLayer` | `ZLayer ()` | no | read-only | |
| 6254 | `getObjectZOrder` | `int ()` | no | read-only | |
| 6255 | `getOuterObjectRect` | `CCRect ()` | no | read-only | |
| 6256 | `getParentMode` | `int ()` | no | read-only | |
| 6257 | `getRelativeSpriteColor` | `GJSpriteColor* (int)` | no | read-only | |
| 6258 | `getScalePosDelta` | `CCPoint ()` | no | read-only | |
| 6259 | `getSecondaryColor` | `GJSpriteColor* ()` | no | read-only | |
| 6260 | `getSecondaryColorMode` | `int ()` | no | read-only | |
| 6261 | `getSlopeAngle` | `float ()` | no | read-only | |
| 6262 | `getUnmodifiedPosition` | `CCPoint ()` | no | read-only | |
| 6263 | `groupColor` | `ccColor3B const& (ccColor3B const&, bool)` | no | read-only | |
| 6264 | `groupOpacityMod` | `float ()` | no | read-only | reads `m_opacityMod` |
| 6265 | `groupWasDisabled` | `void ()` | no | state-mutating-self | sets `m_isGroupDisabled` (group triggers fire this) |
| 6266 | `groupWasEnabled` | `void ()` | no | state-mutating-self | inverse |
| 6267 | `hasSecondaryColor` | `bool ()` | no | read-only | |
| 6268 | `ignoreEditorDuration` | `bool ()` | no | read-only | |
| 6269 | `ignoreEnter` | `bool ()` | no | read-only | |
| 6270 | `ignoreFade` | `bool ()` | no | read-only | |
| 6271 | `init` | `bool (char const*)` | no | engine-internal | |
| 6272 | `isBasicTrigger` | `bool ()` | no | read-only | classifier |
| 6273 | `isColorObject` | `bool ()` | no | read-only | classifier |
| 6274 | `isColorTrigger` | `bool ()` | no | read-only | classifier |
| 6275 | `isConfigurablePortal` | `bool ()` | no | read-only | classifier |
| 6276 | `isEditorSpawnableTrigger` | `bool ()` | no | read-only | classifier |
| 6277 | `isFacingDown` | `bool ()` | no | read-only | classifier |
| 6278 | `isFacingLeft` | `bool ()` | no | read-only | classifier |
| 6279 | `isSettingsObject` | `bool ()` | no | read-only | classifier |
| 6280 | `isSpawnableTrigger` | `bool ()` | no | read-only | classifier |
| 6281 | `isSpecialObject` | `bool ()` | no | read-only | classifier |
| 6282 | `isSpeedObject` | `bool ()` | no | read-only | classifier — sim could use this to detect speed portals (currently uses `m_speedModType`) |
| 6283 | `isStoppableTrigger` | `bool ()` | no | read-only | classifier |
| 6284 | `isTrigger` | `bool ()` | no | read-only | classifier |
| 6285 | `loadGroupsFromString` | `void (string)` | no | engine-internal | load only |
| 6286 | `makeInvisible` | `void ()` | no | visual | sets `m_isInvisible` |
| 6287 | `makeVisible` | `void ()` | no | visual | unsets `m_isInvisible` |
| 6288 | `opacityModForMode` | `float (int, bool)` | no | read-only | |
| 6289 | `parentForZLayer` | `CCNode* (int, bool, int)` | no | state-mutating-layer | looks up parent in layer's section indices |
| 6290 | `perspectiveColorFrame` | `string (char const*, int)` | no | read-only | |
| 6291 | `perspectiveFrame` | `string (char const*, int)` | no | read-only | |
| 6292 | `playDestroyObjectAnim` | `void (GJBaseGameLayer*)` | no | visual | particle/sprite spawn on destroy — leak risk if called during sim |
| 6293 | `playPickupAnimation` | `void (CCSprite*, float, float, float, float)` | no | visual | leak risk |
| 6294 | `playPickupAnimation` | `void (CCSprite*, float, float, float, float, float, float, float, float, bool, float, float)` | no | visual | overload, leak risk |
| 6295 | `playShineEffect` | `void ()` | no | visual | **HOOKED (src/Hooks.cpp:550-553)**: suppressed entirely during sim. Spawns persistent sprite child — leaks visible orb-pop effect on shared GameObject |
| 6296 | `quickUpdatePosition` | `void ()` | no | state-mutating-self | |
| 6297 | `quickUpdatePosition2` | `void ()` | no | state-mutating-self | |
| 6298 | `removeColorSprite` | `void ()` | no | visual | |
| 6299 | `removeGlow` | `void ()` | no | visual | |
| 6300 | `reorderColorSprite` | `void ()` | no | visual | |
| 6301 | `resetColorGroups` | `void ()` | no | state-mutating-self | |
| 6302 | `resetGroupDisabled` | `void ()` | no | state-mutating-self | |
| 6303 | `resetGroups` | `void ()` | no | state-mutating-self | |
| 6304 | `resetMainColorMode` | `void ()` | no | state-mutating-self | |
| 6305 | `resetMoveOffset` | `void ()` | no | state-mutating-self | zeros `m_positionXOffset/Y` |
| 6306 | `resetRScaleForced` | `void ()` | no | state-mutating-self | |
| 6307 | `resetSecondaryColorMode` | `void ()` | no | state-mutating-self | |
| 6308 | `setAreaOpacity` | `void (float, float, int)` | no | state-mutating-self | |
| 6309 | `setCustomZLayer` | `void (int)` | no | state-mutating-self | |
| 6310 | `setDefaultMainColorMode` | `void (int)` | no | state-mutating-self | |
| 6311 | `setDefaultSecondaryColorMode` | `void (int)` | no | state-mutating-self | |
| 6312 | `setGlowOpacity` | `void (unsigned char)` | no | visual | |
| 6313 | `setLastPosition` | `void (CCPoint const&)` | no | state-mutating-self | |
| 6314 | `setMainColorMode` | `void (int)` | no | state-mutating-self | |
| 6315 | `setSecondaryColorMode` | `void (int)` | no | state-mutating-self | |
| 6316 | `setupColorSprite` | `void (int, bool)` | no | visual | |
| 6317 | `setupPixelScale` | `void ()` | no | engine-internal | |
| 6318 | `setupSpriteSize` | `void ()` | no | engine-internal | |
| 6319 | `shouldBlendColor` | `bool (GJSpriteColor*, bool)` | no | read-only | |
| 6320 | `shouldLockX` | `bool ()` | no | read-only | |
| 6321 | `shouldNotHideAnimFreeze` | `bool ()` | no | read-only | |
| 6322 | `shouldShowPickupEffects` | `bool ()` | no | read-only | |
| 6323 | `slopeFloorTop` | `bool ()` | no | read-only | |
| 6324 | `slopeWallLeft` | `bool ()` | no | read-only | |
| 6325 | `slopeYPos` | `double (GameObject*)` | no | read-only | physics-relevant slope query |
| 6326 | `slopeYPos` | `double (CCRect)` | no | read-only | overload |
| 6327 | `slopeYPos` | `double (float)` | no | read-only | overload |
| 6328 | `spawnDefaultPickupParticle` | `void (GJBaseGameLayer*)` | no | visual | leak risk — particle child |
| 6329 | `updateBlendMode` | `void ()` | no | visual | |
| 6330 | `updateCustomColorType` | `void (short)` | no | state-mutating-self | |
| 6331 | `updateCustomScaleX` | `void (float)` | no | state-mutating-self | |
| 6332 | `updateCustomScaleY` | `void (float)` | no | state-mutating-self | |
| 6333 | `updateHSVState` | `void ()` | no | state-mutating-self | |
| 6334 | `updateIsOriented` | `void ()` | no | state-mutating-self | |
| 6335 | `updateMainColor` | `void ()` | no | state-mutating-layer | reads from effect manager |
| 6336 | `updateMainColorOnly` | `void ()` | no | state-mutating-layer | |
| 6337 | `updateMainOpacity` | `void ()` | no | state-mutating-self | |
| 6338 | `updateObjectEditorColor` | `void ()` | no | visual | editor only |
| 6339 | `updateSecondaryColor` | `void ()` | no | state-mutating-layer | |
| 6340 | `updateSecondaryColorOnly` | `void ()` | no | state-mutating-layer | |
| 6341 | `updateSecondaryOpacity` | `void ()` | no | state-mutating-self | |
| 6342 | `updateStartPos` | `void ()` | no | state-mutating-self | |
| 6343 | `updateUnmodifiedPositions` | `void ()` | no | state-mutating-self | |
| 6344 | `usesFreezeAnimation` | `bool ()` | no | read-only | |
| 6345 | `usesSpecialAnimation` | `bool ()` | no | read-only | |

### Fields table

| Line | Name | Type | Classification | Sim impact / notes |
|------|------|------|----------------|--------------------|
| 6347 | `m_someOtherIndex` | int | engine-internal | section indexing |
| 6348 | `m_innerSectionIndex` | int | engine-internal | section indexing |
| 6349 | `m_outerSectionIndex` | int | engine-internal | section indexing |
| 6350 | `m_middleSectionIndex` | int | engine-internal | section indexing |
| 6352 | `m_hasExtendedCollision` | bool | static-level-data | property 511 |
| 6353 | `m_groupColor` | ccColor3B | dynamic-runtime | color trigger writes this |
| 6354 | `m_isColorSpriteBlack` | bool | static-level-data | |
| 6355 | `m_isObjectBlack` | bool | static-level-data | |
| 6356 | `m_blackChildOpacity` | float | dynamic-runtime | |
| 6357 | `m_blackChildOpacityLocked` | bool | static-level-data | |
| 6358 | `m_editorEnabled` | bool | static-level-data | editor only |
| 6359 | `m_isGroupDisabled` | bool | dynamic-runtime | group toggle trigger writes |
| 6360 | `m_isGroupDisabledTemp` | bool | dynamic-runtime | temp toggle |
| 6361 | `m_unk28c` | bool | unknown | |
| 6363 | `m_activeMainColorID` | int | dynamic-runtime | live color channel |
| 6364 | `m_activeDetailColorID` | int | dynamic-runtime | live color channel |
| 6365 | `m_baseUsesHSV` | bool | static-level-data | |
| 6366 | `m_detailUsesHSV` | bool | static-level-data | |
| 6367 | `m_positionXOffset` | float | dynamic-runtime | **move trigger writes** — sim could mutate via super of triggerObject (gated by hooks) |
| 6368 | `m_positionYOffset` | float | dynamic-runtime | same |
| 6369 | `m_rotationXOffset` | float | dynamic-runtime | rotate trigger writes |
| 6370 | `m_unk2A8` | float | unknown | |
| 6371 | `m_rotationYOffset` | float | dynamic-runtime | rotate trigger writes |
| 6372 | `m_unk2B0` | float | unknown | |
| 6373 | `m_scaleXOffset` | float | dynamic-runtime | scale trigger writes |
| 6374 | `m_scaleYOffset` | float | dynamic-runtime | scale trigger writes |
| 6375 | `m_unk2BC` | float | unknown | |
| 6376 | `m_unk2C0` | float | unknown | |
| 6377 | `m_tempOffsetXRelated` | bool | dynamic-runtime | |
| 6378 | `m_isFlipX` | bool | static-level-data | |
| 6379 | `m_isFlipY` | bool | static-level-data | |
| 6380 | `m_customBoxOffset` | CCPoint | static-level-data | |
| 6381 | `m_boxOffsetCalculated` | bool | dynamic-runtime | dirty flag |
| 6382 | `m_boxOffset` | CCPoint | dynamic-runtime | cached |
| 6383 | `m_orientedBox` | OBB2D* | dynamic-runtime | hitbox; lazily allocated; sim relies on getOrientedBox |
| 6384 | `m_shouldUseOuterOb` | bool | static-level-data | |
| 6385 | `m_glowSprite` | CCSprite* | engine-internal | visual child |
| 6386 | `m_isRingPoweredOn` | bool | dynamic-runtime | overlap with EnhancedGameObject::m_poweredOn? |
| 6387 | `m_width` | float | static-level-data | |
| 6388 | `m_height` | float | static-level-data | |
| 6389 | `m_addToNodeContainer` | bool | static-level-data | |
| 6390 | `m_isActivated` | bool | activation-flags | **read by sim**; written by activateObject/deactivateObject and various triggers |
| 6391 | `m_isDisabled2` | bool | dynamic-runtime | set by `disableObject` |
| 6392 | `m_particle` | CCParticleSystemQuad* | engine-internal | visual child |
| 6393 | `m_particleString` | string | static-level-data | |
| 6394 | `m_hasParticles` | bool | static-level-data | property 146 |
| 6396 | `m_particleUseObjectColor` | bool | static-level-data | |
| 6397 | `m_hasColorSprite` | bool | static-level-data | |
| 6398 | `m_particleOffset` | CCPoint | static-level-data | |
| 6399 | `m_isParticleSpriteLocked` | bool | dynamic-runtime | |
| 6400 | `m_textureRect` | CCRect | static-level-data | |
| 6401 | `m_isDirty` | bool | dynamic-runtime | |
| 6402 | `m_isObjectPosDirty` | bool | dynamic-runtime | |
| 6403 | `m_isUnmodifiedPosDirty` | bool | dynamic-runtime | |
| 6404 | `m_fadeMargin` | float | static-level-data | |
| 6405 | `m_objectRect` | CCRect | dynamic-runtime | cached hitbox rect |
| 6406 | `m_isObjectRectDirty` | bool | dynamic-runtime | |
| 6407 | `m_isOrientedBoxDirty` | bool | dynamic-runtime | |
| 6408 | `m_colorSpriteLocked` | bool | static-level-data | |
| 6409 | `m_unk353` | bool | unknown | |
| 6410 | `m_canRotateFree` | bool | static-level-data | |
| 6411 | `m_isMirroredByScale` | bool | dynamic-runtime | |
| 6413 | `m_linkedGroup` | int | static-level-data | property 108 |
| 6414 | `m_unk35C` | int | unknown | |
| 6415 | `m_colorType` | short | static-level-data | |
| 6416 | `m_childColorType` | short | static-level-data | |
| 6417 | `m_shouldBlendBase` | bool | static-level-data | |
| 6418 | `m_shouldBlendDetail` | bool | static-level-data | |
| 6419 | `m_hasCustomChild` | bool | static-level-data | |
| 6420 | `m_unk367` | bool | unknown | |
| 6421 | `m_colorSprite` | CCSprite* | engine-internal | |
| 6422 | `m_unk370` | bool | unknown | |
| 6423 | `m_objectRadius` | float | static-level-data | |
| 6424 | `m_isRotationAligned` | bool | static-level-data | |
| 6425 | `m_spriteWidthScale` | float | static-level-data | |
| 6426 | `m_spriteHeightScale` | float | static-level-data | |
| 6427 | `m_uniqueID` | int | static-level-data | monotonic; assigned at load |
| 6428 | `m_objectType` | GameObjectType | static-level-data | classification (Player, Ring, Pad, etc.) |
| 6430 | `m_savedObjectType` | GameObjectType | static-level-data | snapshot for PlayerObject::gameEventTriggered |
| 6431 | `m_unk390` | int | unknown | |
| 6432 | `m_unmodifiedPositionX` | float | static-level-data | original X |
| 6433 | `m_unmodifiedPositionY` | float | static-level-data | original Y |
| 6434 | `m_positionX` | double | dynamic-runtime | **live position**; written by move triggers |
| 6435 | `m_positionY` | double | dynamic-runtime | same |
| 6436 | `m_startPosition` | CCPoint | static-level-data | |
| 6437 | `m_usesAudioScale` | bool | static-level-data | |
| 6439 | `m_hasNoAudioScale` | bool | static-level-data | property 372 |
| 6440 | `m_isDisabled` | bool | dynamic-runtime | set by destroyObject |
| 6441 | `m_startRotationX` | float | static-level-data | |
| 6442 | `m_startRotationY` | float | static-level-data | |
| 6443 | `m_startScaleX` | float | static-level-data | |
| 6444 | `m_startScaleY` | float | static-level-data | |
| 6445 | `m_customScaleX` | float | dynamic-runtime | written by `updateCustomScaleX` |
| 6446 | `m_customScaleY` | float | dynamic-runtime | |
| 6447 | `m_startFlipX` | bool | static-level-data | |
| 6448 | `m_startFlipY` | bool | static-level-data | |
| 6449 | `m_unk3ee` | bool | unknown | |
| 6450 | `m_isInvisible` | bool | dynamic-runtime | makeInvisible/makeVisible |
| 6451 | `m_unk3D8` | int | unknown | |
| 6452 | `m_varianceIndex` | short | static-level-data | |
| 6453 | `m_unk3DE` | bool | unknown | |
| 6454 | `m_enterType` | short | static-level-data | |
| 6455 | `m_exitType` | short | static-level-data | property 343 |
| 6457 | `m_enterChannel` | short | static-level-data | property 446 |
| 6459 | `m_objectMaterial` | short | static-level-data | |
| 6460 | `m_unk3E8` | bool | unknown | |
| 6461 | `m_parentMode` | short | static-level-data | property 96 |
| 6463 | `m_hasNoGlow` | bool | static-level-data | property 23 |
| 6465 | `m_targetColor` | int | static-level-data | property 1 |
| 6467 | `m_objectID` | int | static-level-data | the GD object ID number |
| 6468 | `m_unk3F8` | bool | unknown | |
| 6469 | `m_intrinsicDontFade` | bool | static-level-data | |
| 6470 | `m_ignoreEnter` | bool | static-level-data | |
| 6471 | `m_ignoreFade` | bool | static-level-data | |
| 6473 | `m_isSolidColorBlock` | bool | static-level-data | IDs 207-213, 693-694 |
| 6474 | `m_unk3FD` | bool | unknown | |
| 6475 | `m_customSpriteColor` | bool | static-level-data | |
| 6477 | `m_customColorType` | short | static-level-data | property 497 |
| 6479 | `m_isDontEnter` | bool | static-level-data | property 67 |
| 6481 | `m_isDontFade` | bool | static-level-data | property 64 |
| 6483 | `m_hasNoEffects` | bool | static-level-data | property 116 |
| 6485 | `m_hasNoParticles` | bool | static-level-data | property 507 |
| 6486 | `m_defaultZOrder` | int | static-level-data | |
| 6487 | `m_unk40C` | bool | unknown | |
| 6488 | `m_colorZLayerRelated` | bool | static-level-data | |
| 6489 | `m_customAudioScale` | bool | static-level-data | |
| 6490 | `m_minAudioScale` | float | static-level-data | |
| 6491 | `m_maxAudioScale` | float | static-level-data | |
| 6492 | `m_particleLocked` | bool | dynamic-runtime | |
| 6494 | `m_property53` | int | static-level-data | property 53 |
| 6495 | `m_isInvisibleBlock` | bool | static-level-data | |
| 6496 | `m_customGlowColor` | bool | static-level-data | |
| 6497 | `m_glowColorIsLBG` | bool | static-level-data | |
| 6498 | `m_cantColorGlow` | bool | static-level-data | |
| 6499 | `m_opacityMod` | float | dynamic-runtime | |
| 6500 | `m_slopeUphill` | bool | static-level-data | |
| 6501 | `m_slopeDirection` | int | static-level-data | |
| 6502 | `m_slopeIsHazard` | bool | static-level-data | |
| 6503 | `m_opacityMod2` | float | dynamic-runtime | |
| 6505 | `m_baseColor` | GJSpriteColor* | engine-internal | pointer into effect manager — layer-owned |
| 6507 | `m_detailColor` | GJSpriteColor* | engine-internal | same |
| 6508 | `m_baseOrDetailBlending` | bool | static-level-data | |
| 6509 | `m_defaultZLayer` | ZLayer | static-level-data | |
| 6510 | `m_zFixedZLayer` | bool | static-level-data | |
| 6512 | `m_zLayer` | ZLayer | static-level-data | property 24 |
| 6514 | `m_zOrder` | int | static-level-data | property 25 |
| 6515 | `m_wasSelected` | bool | dynamic-runtime | editor |
| 6516 | `m_isSelected` | bool | dynamic-runtime | editor |
| 6517 | `m_unk460` | float | unknown | |
| 6518 | `m_unk464` | CCPoint | unknown | |
| 6519 | `m_updateParents` | bool | dynamic-runtime | |
| 6520 | `m_updateEditorColor` | bool | dynamic-runtime | editor |
| 6522 | `m_hasGroupParent` | bool | static-level-data | property 34 |
| 6524 | `m_hasAreaParent` | bool | static-level-data | property 279 |
| 6526 | `m_scaleX` | float | dynamic-runtime | property 128; scale trigger writes |
| 6528 | `m_scaleY` | float | dynamic-runtime | property 129 |
| 6530 | `m_groups` | array<short, 10>* | static-level-data | property 57 |
| 6532 | `m_groupCount` | short | static-level-data | |
| 6534 | `m_hasGroupParentsString` | bool | static-level-data | property 274 |
| 6535 | `m_colorGroups` | array<short, 10>* | static-level-data | |
| 6536 | `m_colorGroupCount` | short | static-level-data | |
| 6537 | `m_opacityGroups` | array<short, 10>* | static-level-data | |
| 6538 | `m_opacityGroupCount` | short | static-level-data | |
| 6540 | `m_editorLayer` | short | static-level-data | property 20 |
| 6542 | `m_editorLayer2` | short | static-level-data | property 61 |
| 6543 | `m_enabledGroupsCounter` | int | dynamic-runtime | group toggle ref count |
| 6544 | `m_updateCustomContentSize` | bool | dynamic-runtime | |
| 6545 | `m_hasContentSize` | bool | static-level-data | |
| 6547 | `m_isNoTouch` | bool | static-level-data | property 121 |
| 6548 | `m_lastSize` | CCSize | dynamic-runtime | |
| 6549 | `m_lastPosition` | CCPoint | dynamic-runtime | |
| 6550 | `m_unk4C0` | int | unknown | |
| 6551 | `m_unk4C4` | int | unknown | |
| 6552 | `m_unk4C8` | int | unknown | |
| 6553 | `m_unk4CC` | int | unknown | |
| 6554 | `m_classType` | GameObjectClassType | static-level-data | |
| 6555 | `m_isTrigger` | bool | static-level-data | |
| 6556 | `m_isSpawnOrderTrigger` | bool | static-level-data | |
| 6557 | `m_isColorTrigger` | bool | static-level-data | |
| 6558 | `m_dontIgnoreDuration` | bool | static-level-data | |
| 6559 | `m_canBeControlled` | bool | static-level-data | |
| 6560 | `m_activateTriggerInEditor` | bool | static-level-data | |
| 6561 | `m_isStartPos` | bool | static-level-data | |
| 6563 | `m_isHighDetail` | bool | static-level-data | property 103 |
| 6564 | `m_mainActionSprite` | ColorActionSprite* | engine-internal | |
| 6565 | `m_detailActionSprite` | ColorActionSprite* | engine-internal | |
| 6566 | `m_goEffectManager` | GJEffectManager* | engine-internal | layer's effect manager pointer |
| 6567 | `m_unk4F8` | bool | unknown | |
| 6568 | `m_isDecoration` | bool | static-level-data | |
| 6569 | `m_isDecoration2` | bool | static-level-data | |
| 6570 | `m_unk4fb` | bool | unknown | |
| 6571 | `m_maybeNotColorable` | bool | static-level-data | |
| 6573 | `m_isPassable` | bool | static-level-data | property 134; physics-relevant |
| 6575 | `m_isHide` | bool | static-level-data | property 135 |
| 6577 | `m_isNonStickX` | bool | static-level-data | property 136; physics |
| 6579 | `m_isNonStickY` | bool | static-level-data | property 289; physics |
| 6581 | `m_isIceBlock` | bool | static-level-data | property 137; physics |
| 6583 | `m_isGripSlope` | bool | static-level-data | property 193; physics |
| 6585 | `m_isScaleStick` | bool | static-level-data | property 356; physics |
| 6587 | `m_isExtraSticky` | bool | static-level-data | property 495; physics |
| 6589 | `m_isDontBoostY` | bool | static-level-data | property 496; physics |
| 6591 | `m_isDontBoostX` | bool | static-level-data | property 509; physics |
| 6592 | `m_unk507` | bool | unknown | |
| 6593 | `m_unk508` | bool | unknown | |
| 6594 | `m_unk50C` | float | unknown | |
| 6595 | `m_pixelScaleX` | float | static-level-data | |
| 6596 | `m_pixelScaleY` | float | static-level-data | |
| 6598 | `m_mainColorKeyIndex` | int | static-level-data | property 155 |
| 6600 | `m_detailColorKeyIndex` | int | static-level-data | property 156 |
| 6601 | `m_areaOpacityRelated` | uint8_t | dynamic-runtime | |
| 6602 | `m_areaOpacityValue` | float | dynamic-runtime | |
| 6603 | `m_areaOpacityIndex` | int | dynamic-runtime | |
| 6604 | `m_unk52C` | int | unknown | |
| 6605 | `m_unk530` | bool | unknown | |
| 6606 | `m_isUIObject` | bool | static-level-data | |
| 6607 | `m_greenDebugDraw` | bool | static-level-data | |

## EnhancedGameObject (adds to GameObject)

### Summary
- Methods: 27 (2 hooked: `activatedByPlayer`, `hasBeenActivatedByPlayer`)
  - Constructor: 1
  - Static factory: 1
  - Virtual: 18 (most override GameObject)
  - Non-virtual: 11
- Fields: 30 added

### Methods table

| Line | Name | Signature | Virtual? | Classification | Sim impact / notes |
|------|------|-----------|----------|----------------|--------------------|
| 4389 | `EnhancedGameObject` | constructor | no | engine-internal | |
| 4391 | `create` | `static EnhancedGameObject* (char const*)` | static | engine-internal | factory |
| 4393 | `customSetup` | `void ()` override | virtual | engine-internal | level load; sets up animation |
| 4394 | `resetObject` | `void ()` override | virtual | state-mutating-self | resets `m_activated`, `m_activatedByPlayer1/2`, animation; called on respawn |
| 4395 | `deactivateObject` | `void (bool)` override | virtual | state-mutating-self | |
| 4396 | `customObjectSetup` | `void (vector<string>&, vector<void*>&)` override | virtual | engine-internal | load |
| 4397 | `getSaveString` | `string (GJBaseGameLayer*)` override | virtual | engine-internal | editor |
| 4398 | `triggerActivated` | `void (float xPosition)` override | virtual | state-mutating-self | base no-op for non-effect objects |
| 4399 | `restoreObject` | `void ()` override | virtual | state-mutating-self | resets animation state |
| 4400 | `animationTriggered` | `void ()` override | virtual | visual | kicks off animation |
| 4401 | `activatedByPlayer` | `void (PlayerObject*)` override | virtual | state-mutating-self | **HOOKED (src/Hooks.cpp:557-573)**. Engine impl sets `m_activated = true`, `m_activatedByPlayer1/2 = true`, plays visual side effects (shine, particle, animation). Sim path calls `markActivated` to spoof flags then early-returns |
| 4402 | `hasBeenActivatedByPlayer` | `bool (PlayerObject*)` override | virtual | read-only | **HOOKED (src/Hooks.cpp:582-593)**. Engine impl reads `m_activatedByPlayer1/2` accounting for `m_isMultiActivate`. Sim path checks per-run set + real-player flags |
| 4403 | `hasBeenActivated` | `bool ()` override | virtual | read-only | reads `m_activated` |
| 4404 | `saveActiveColors` | `void ()` override | virtual | state-mutating-self | |
| 4405 | `canAllowMultiActivate` | `bool ()` override | virtual | read-only | |
| 4406 | `getHasSyncedAnimation` | `bool ()` override | virtual | read-only | |
| 4407 | `getHasRotateAction` | `bool ()` override | virtual | read-only | |
| 4408 | `canMultiActivate` | `bool (bool)` override | virtual | read-only | |
| 4409 | `powerOnObject` | `void (int state)` | virtual | state-mutating-self | sets `m_poweredOn` (used by rings/orbs whose visual differs when powered) |
| 4410 | `powerOffObject` | `void ()` | virtual | state-mutating-self | |
| 4411 | `stateSensitiveOff` | `void (GJBaseGameLayer*)` | virtual | state-mutating-self | base no-op |
| 4412 | `updateSyncedAnimation` | `void (float, int)` | virtual | visual | animation frame advance |
| 4413 | `updateAnimateOnTrigger` | `void (bool)` | virtual | state-mutating-self | |
| 4415 | `createRotateAction` | `void (float, int)` | no | state-mutating-layer | adds CCAction to runner |
| 4416 | `init` | `bool (char const*)` | no | engine-internal | |
| 4417 | `previewAnimateOnTrigger` | `void ()` | no | engine-internal | editor |
| 4418 | `refreshRotateAction` | `void ()` | no | state-mutating-self | |
| 4419 | `resetSyncedAnimation` | `void ()` | no | state-mutating-self | |
| 4420 | `setupAnimationVariables` | `void ()` | no | state-mutating-self | |
| 4421 | `triggerAnimation` | `void ()` | no | visual | |
| 4422 | `updateRotateAction` | `void (float dt)` | no | state-mutating-self | |
| 4423 | `updateState` | `void (int)` | no | state-mutating-self | |
| 4424 | `updateUserCoin` | `void ()` | no | state-mutating-layer | |
| 4425 | `waitForAnimationTrigger` | `void ()` | no | state-mutating-self | |

### Fields table

| Line | Name | Type | Classification | Sim impact / notes |
|------|------|------|----------------|--------------------|
| 4427 | `m_poweredOn` | bool | dynamic-runtime | written by powerOnObject/powerOffObject — could leak if sim crosses an orb that triggers a power-on |
| 4428 | `m_state` | int | dynamic-runtime | |
| 4429 | `m_animationRandomizedStartValue` | int | static-level-data | |
| 4430 | `m_animationStart` | float | static-level-data | |
| 4431 | `m_unk540` | float | unknown | |
| 4432 | `m_unk544` | float | unknown | |
| 4433 | `m_unk548` | bool | unknown | |
| 4434 | `m_randomFrameTime` | float | static-level-data | |
| 4435 | `m_visible` | bool | dynamic-runtime | |
| 4436 | `m_shouldNotHideAnimFreeze` | bool | static-level-data | |
| 4437 | `m_usesSpecialAnimation` | bool | static-level-data | |
| 4438 | `m_frameTime` | float | static-level-data | |
| 4439 | `m_frames` | short | static-level-data | |
| 4440 | `m_hasCustomAnimation` | bool | static-level-data | |
| 4441 | `m_hasCustomRotation` | bool | static-level-data | |
| 4443 | `m_disableRotation` | bool | static-level-data | property 98 |
| 4445 | `m_rotationSpeed` | float | static-level-data | property 97 |
| 4446 | `m_rotationAngle` | float | dynamic-runtime | actively rotates |
| 4447 | `m_rotationDelta` | float | dynamic-runtime | |
| 4448 | `m_rotationAnimationSpeed` | float | static-level-data | |
| 4450 | `m_animationRandomizedStart` | bool | static-level-data | property 106 |
| 4452 | `m_animationSpeed` | float | static-level-data | property 107 |
| 4454 | `m_animationShouldUseSpeed` | bool | static-level-data | property 122 |
| 4456 | `m_animateOnTrigger` | bool | static-level-data | property 123 |
| 4458 | `m_disableDelayedLoop` | bool | static-level-data | property 126 |
| 4460 | `m_disableAnimShine` | bool | static-level-data | property 127 |
| 4462 | `m_singleFrame` | int | static-level-data | property 462 |
| 4464 | `m_animationOffset` | bool | static-level-data | property 592 |
| 4465 | `m_animationTriggered` | bool | dynamic-runtime | written by animationTriggered |
| 4466 | `m_unkAnimationInt` | int | unknown | |
| 4467 | `m_maybeAnimationVariableXInt` | int | unknown | |
| 4468 | `m_maybeAnimationVariableYInt` | int | unknown | |
| 4470 | `m_animateOnlyWhenActive` | bool | static-level-data | property 214 |
| 4472 | `m_isNoMultiActivate` | bool | static-level-data | property 444 |
| 4474 | `m_isMultiActivate` | bool | static-level-data | property 99 |
| 4475 | `m_activated` | bool | activation-flags | **SPOOFED via markActivated (src/Trajectory.cpp:428)**. Snapshot/restore via clearActivated at runPlan end (src/Trajectory.cpp:440) |
| 4476 | `m_activatedByPlayer1` | bool | activation-flags | **SPOOFED + restored (Trajectory.cpp:429,441)**; ALSO save-restored around speed-mod triggerObject/triggerActivated (Hooks.cpp:521-527, 536-542) |
| 4477 | `m_activatedByPlayer2` | bool | activation-flags | **SPOOFED + restored (Trajectory.cpp:430,442)**; ALSO save-restored as above |
| 4478 | `m_hasUniqueCoin` | bool | static-level-data | |

Animation fields: all are visual only EXCEPT `m_disableRotation`/`m_rotationSpeed`/`m_rotationAngle` — rotating objects affect collision via `getOrientedBox`. The engine's `updateRotateAction` mutates `m_rotationAngle` over time but this happens via cocos action scheduling, not via the sim's super-call path, so the sim doesn't re-trigger it. **Not currently a leak vector but worth noting**: animated rotating spikes are a known hitbox-changes-with-time class.

## EffectGameObject (adds to EnhancedGameObject)

### Summary
- Methods: 19 (2 hooked: `triggerObject`, `triggerActivated`)
  - Constructor: 1
  - Static factory: 1
  - Virtual: 14 (most override base classes)
  - Non-virtual: 10
- Fields: 91 added

### Methods table

| Line | Name | Signature | Virtual? | Classification | Sim impact / notes |
|------|------|-----------|----------|----------------|--------------------|
| 4012 | `EffectGameObject` | constructor | no | engine-internal | |
| 4014 | `create` | `static EffectGameObject* (char const*)` | static | engine-internal | factory |
| 4016 | `setOpacity` | `void (unsigned char)` override | virtual | visual | |
| 4017 | `firstSetup` | `void ()` override | virtual | engine-internal | |
| 4018 | `customSetup` | `void ()` override | virtual | engine-internal | |
| 4019 | `triggerObject` | `void (GJBaseGameLayer*, int, vector<int>const*)` override | virtual | state-mutating-layer | **HOOKED (src/Hooks.cpp:517-531)**. Suppressed during sim EXCEPT when `m_speedModType != 0`. Engine impl dispatches into all trigger families (color/move/spawn/pulse/etc.) and writes into layer's GJEffectManager and group containers |
| 4020 | `customObjectSetup` | `void (vector<string>&, vector<void*>&)` override | virtual | engine-internal | |
| 4021 | `getSaveString` | `string (GJBaseGameLayer*)` override | virtual | engine-internal | |
| 4022 | `setRScaleX` | `void (float)` override | virtual | state-mutating-self | |
| 4023 | `setRScaleY` | `void (float)` override | virtual | state-mutating-self | |
| 4024 | `triggerActivated` | `void (float xPosition)` override | virtual | state-mutating-layer | **HOOKED (src/Hooks.cpp:533-546)**. Same gating as triggerObject. Engine impl handles touch/spawn-trigger activation lifecycle |
| 4025 | `restoreObject` | `void ()` override | virtual | state-mutating-self | resets trigger state on respawn |
| 4026 | `spawnXPosition` | `float ()` override | virtual | read-only | reads `m_spawnXPosition` |
| 4027 | `canReverse` | `bool ()` override | virtual | read-only | |
| 4028 | `isSpecialSpawnObject` | `bool ()` override | virtual | read-only | |
| 4029 | `canBeOrdered` | `bool ()` override | virtual | read-only | |
| 4030 | `getObjectLabel` | `CCLabelBMFont* ()` override | virtual | read-only | |
| 4031 | `setObjectLabel` | `void (CCLabelBMFont*)` override | virtual | state-mutating-self | |
| 4032 | `stateSensitiveOff` | `void (GJBaseGameLayer*)` override | virtual | state-mutating-layer | overrides base no-op |
| 4034 | `getTargetColorIndex` | `int ()` | no | read-only | |
| 4035 | `init` | `bool (char const*)` | no | engine-internal | |
| 4036 | `playTriggerEffect` | `void ()` | no | visual | leak risk |
| 4037 | `resetSpawnTrigger` | `void ()` | no | state-mutating-self | |
| 4038 | `setTargetID` | `void (int)` | no | state-mutating-self | |
| 4039 | `setTargetID2` | `void (int)` | no | state-mutating-self | |
| 4040 | `triggerEffectFinished` | `void ()` | no | state-mutating-self | |
| 4041 | `updateInteractiveHover` | `void (float)` | no | visual | |
| 4042 | `updateSpecialColor` | `void ()` | no | state-mutating-self | |
| 4043 | `updateSpeedModType` | `void ()` | no | state-mutating-self | computes `m_speedModType` from object ID; called at load |

### Fields table

| Line | Name | Type | Classification | Sim impact / notes |
|------|------|------|----------------|--------------------|
| 4046 | `m_unknownBool` | bool | unknown | |
| 4047 | `m_triggerTargetColor` | ccColor3B | static-trigger-config | color trigger config |
| 4049 | `m_duration` | float | static-trigger-config | property 10 |
| 4051 | `m_opacity` | float | static-trigger-config | property 35 |
| 4052 | `m_triggerEffectPlaying` | bool | dynamic-runtime | written when trigger fires — would leak into real game if sim fires a trigger |
| 4054 | `m_targetGroupID` | int | static-trigger-config | property 51 |
| 4056 | `m_centerGroupID` | int | static-trigger-config | property 71 |
| 4058 | `m_isTouchTriggered` | bool | static-trigger-config | property 11 |
| 4060 | `m_isSpawnTriggered` | bool | static-trigger-config | property 62 |
| 4062 | `m_hasCenterEffect` | bool | static-trigger-config | property 369 |
| 4064 | `m_shakeStrength` | float | static-trigger-config | property 75 |
| 4066 | `m_shakeInterval` | float | static-trigger-config | property 84 |
| 4068 | `m_tintGround` | bool | static-trigger-config | property 14 |
| 4070 | `m_usesPlayerColor1` | bool | static-trigger-config | property 15 |
| 4072 | `m_usesPlayerColor2` | bool | static-trigger-config | property 16 |
| 4074 | `m_usesBlending` | bool | static-trigger-config | property 17 |
| 4076 | `m_moveOffset` | CCPoint | static-trigger-config | property 28, 29 |
| 4078 | `m_easingType` | EasingType | static-trigger-config | property 30 |
| 4080 | `m_easingRate` | float | static-trigger-config | property 85 |
| 4082 | `m_lockToPlayerX` | bool | static-trigger-config | property 58 |
| 4084 | `m_lockToPlayerY` | bool | static-trigger-config | property 59 |
| 4086 | `m_lockToCameraX` | bool | static-trigger-config | property 141 |
| 4088 | `m_lockToCameraY` | bool | static-trigger-config | property 142 |
| 4090 | `m_useMoveTarget` | bool | static-trigger-config | property 100 |
| 4092 | `m_moveTargetMode` | MoveTargetType | static-trigger-config | property 101 |
| 4094 | `m_moveModX` | float | static-trigger-config | property 143 |
| 4096 | `m_moveModY` | float | static-trigger-config | property 144 |
| 4098 | `m_smallStep` | bool | static-trigger-config | property 393 |
| 4100 | `m_isDirectionFollowSnap360` | bool | static-trigger-config | property 394 |
| 4102 | `m_targetModCenterID` | int | static-trigger-config | property 395 |
| 4104 | `m_directionModeDistance` | float | static-trigger-config | property 396 |
| 4106 | `m_isDynamicMode` | bool | static-trigger-config | property 397 |
| 4108 | `m_isSilent` | bool | static-trigger-config | property 544 |
| 4110 | `m_specialTarget` | int | static-trigger-config | property 538 |
| 4112 | `m_rotationDegrees` | float | static-trigger-config | property 68 |
| 4114 | `m_times360` | int | static-trigger-config | property 69 |
| 4116 | `m_lockObjectRotation` | bool | static-trigger-config | property 70 |
| 4118 | `m_rotationTargetID` | int | static-trigger-config | property 401 |
| 4120 | `m_rotationOffset` | float | static-trigger-config | property 402 |
| 4122 | `m_dynamicModeEasing` | int | static-trigger-config | property 403 |
| 4124 | `m_followXMod` | float | static-trigger-config | property 72 |
| 4126 | `m_followYMod` | float | static-trigger-config | property 73 |
| 4128 | `m_followYSpeed` | float | static-trigger-config | property 90 |
| 4130 | `m_followYDelay` | float | static-trigger-config | property 91 |
| 4132 | `m_followYOffset` | int | static-trigger-config | property 92 |
| 4134 | `m_followYMaxSpeed` | float | static-trigger-config | property 105 |
| 4136 | `m_fadeInDuration` | float | static-trigger-config | property 45 |
| 4138 | `m_holdDuration` | float | static-trigger-config | property 46 |
| 4140 | `m_fadeOutDuration` | float | static-trigger-config | property 47 |
| 4142 | `m_pulseMode` | int | static-trigger-config | property 48 |
| 4144 | `m_pulseTargetType` | int | static-trigger-config | property 52 |
| 4146 | `m_hsvValue` | ccHSVValue | static-trigger-config | property 49 |
| 4148 | `m_copyColorID` | int | static-trigger-config | property 50 |
| 4150 | `m_copyOpacity` | bool | static-trigger-config | property 60 |
| 4152 | `m_pulseMainOnly` | bool | static-trigger-config | property 65 |
| 4154 | `m_pulseDetailOnly` | bool | static-trigger-config | property 66 |
| 4156 | `m_pulseExclusive` | bool | static-trigger-config | property 86 |
| 4158 | `m_legacyHSV` | bool | static-trigger-config | property 210 |
| 4160 | `m_activateGroup` | bool | static-trigger-config | property 56 |
| 4162 | `m_touchHoldMode` | bool | static-trigger-config | property 81 |
| 4164 | `m_touchToggleMode` | TouchTriggerType | static-trigger-config | property 82 |
| 4166 | `m_touchPlayerMode` | TouchTriggerControl | static-trigger-config | property 198 |
| 4168 | `m_isDualMode` | bool | static-trigger-config | property 89 |
| 4170 | `m_animationID` | int | static-trigger-config | property 76 |
| 4171 | `m_spawnXPosition` | float | static-level-data | computed at load |
| 4172 | `m_spawnOrder` | int | static-level-data | |
| 4174 | `m_isMultiTriggered` | bool | static-trigger-config | property 87 |
| 4176 | `m_previewDisable` | bool | static-trigger-config | property 102 |
| 4178 | `m_spawnOrdered` | bool | static-trigger-config | property 441 |
| 4180 | `m_triggerOnExit` | bool | static-trigger-config | property 93 |
| 4182 | `m_itemID2` | int | static-trigger-config | property 95 |
| 4184 | `m_controlID` | int | static-trigger-config | property 534 |
| 4186 | `m_targetControlID` | bool | static-trigger-config | property 535 |
| 4188 | `m_isDynamicBlock` | bool | static-trigger-config | property 94 |
| 4190 | `m_itemID` | int | static-trigger-config | property 80 |
| 4192 | `m_targetPlayer1` | bool | static-trigger-config | property 138 |
| 4194 | `m_targetPlayer2` | bool | static-trigger-config | property 200 |
| 4196 | `m_followCPP` | bool | static-trigger-config | property 201 |
| 4198 | `m_subtractCount` | bool | static-trigger-config | property 78 |
| 4200 | `m_collectibleIsPickupItem` | bool | static-trigger-config | property 381 |
| 4202 | `m_collectibleIsToggleTrigger` | bool | static-trigger-config | property 382 |
| 4204 | `m_collectibleParticleID` | int | static-trigger-config | property 440 |
| 4206 | `m_collectiblePoints` | int | static-trigger-config | property 383 |
| 4208 | `m_hasNoAnimation` | bool | static-trigger-config | property 463 |
| 4209 | `m_unk698` | void* | unknown | |
| 4210 | `m_forceModID` | int | static-trigger-config | |
| 4211 | `m_rotateFollowP1` | bool | static-trigger-config | |
| 4212 | `m_rotateFollowP2` | bool | static-trigger-config | |
| 4213 | `m_unk6a8` | float | unknown | |
| 4214 | `m_unk6ac` | float | unknown | |
| 4215 | `m_unk6b0` | float | unknown | |
| 4216 | `m_unk6b4` | bool | unknown | |
| 4218 | `m_gravityValue` | float | static-trigger-config | property 148 |
| 4220 | `m_isSinglePTouch` | bool | static-trigger-config | property 284 |
| 4222 | `m_zoomValue` | float | static-trigger-config | property 371 |
| 4224 | `m_cameraIsFreeMode` | bool | static-trigger-config | property 111 |
| 4226 | `m_cameraEditCameraSettings` | bool | static-trigger-config | property 112 |
| 4228 | `m_cameraEasingValue` | float | static-trigger-config | property 113 |
| 4230 | `m_cameraPaddingValue` | float | static-trigger-config | property 114 |
| 4232 | `m_cameraDisableGridSnap` | bool | static-trigger-config | property 370 |
| 4234 | `m_endReversed` | bool | static-trigger-config | property 118 |
| 4236 | `m_timeWarpTimeMod` | float | static-trigger-config | property 120 |
| 4238 | `m_shouldPreview` | bool | static-trigger-config | property 13 |
| 4240 | `m_ordValue` | int | static-trigger-config | property 115 |
| 4242 | `m_channelValue` | int | static-trigger-config | property 170 |
| 4244 | `m_isReverse` | bool | static-trigger-config | property 117 |
| 4245 | `m_speedModType` | short | static-trigger-config | **read by sim (src/Hooks.cpp:487)** to gate triggerObject/triggerActivated suppression. Nonzero means this is a speed portal. Set at load by `updateSpeedModType` |
| 4246 | `m_speedStart` | CCPoint | static-level-data | |
| 4248 | `m_secretCoinID` | int | static-trigger-config | property 12 |
| 4249 | `m_unk6f4` | bool | unknown | |
| 4250 | `m_unk6f5` | bool | unknown | |
| 4251 | `m_endPosition` | CCPoint | dynamic-runtime | written when trigger applies move |
| 4252 | `m_spawnTriggerDelay` | float | static-trigger-config | |
| 4253 | `m_gravityMod` | float | static-trigger-config | |
| 4254 | `m_unk708` | bool | unknown | |
| 4255 | `m_objectLabel` | CCLabelBMFont* | engine-internal | label child |
| 4257 | `m_ignoreGroupParent` | bool | static-trigger-config | property 280 |
| 4259 | `m_ignoreLinkedObjects` | bool | static-trigger-config | property 281 |
| 4260 | `m_channelChanged` | bool | dynamic-runtime | |

Note on inherited `m_activatedByPlayer1/2`: these are declared on `EnhancedGameObject` but are written by the engine in trigger-context too. Specifically, `EffectGameObject::triggerObject` for a speed portal calls into `GJBaseGameLayer::updateTimeMod(speed, players=true, noEffects=true)` which writes `m_player1/m_player2->m_playerSpeed` inline, and the engine also marks the portal's `m_activatedByPlayer1/2` flags. Hook saves and restores these around the speed-mod super-call (`src/Hooks.cpp:521-527, 536-542`) to keep "has the real player crossed me?" state untouched while still letting the sim's physics observe the new speed.

## Cross-reference: methods currently hooked

All hooks on these base classes live in `src/Hooks.cpp`:

| File:Line | Hook target | What it does |
|-----------|-------------|--------------|
| `src/Hooks.cpp:479-547` | `EffectGameObject::triggerObject(GJBaseGameLayer*, int, vector<int>const*)` | During sim: suppresses entirely unless `m_speedModType != 0`; for speed mods, calls super inside `saveRestoreRealPlayerSpeeds` wrapper and save-restores `m_activatedByPlayer1/2` |
| `src/Hooks.cpp:533-546` | `EffectGameObject::triggerActivated(float xPosition)` | Same gating + save/restore as `triggerObject` |
| `src/Hooks.cpp:549-554` | `GameObject::playShineEffect()` | Suppresses entirely during sim (sprite-child leak) |
| `src/Hooks.cpp:556-573` | `EnhancedGameObject::activatedByPlayer(PlayerObject*)` | For sim players: calls `markActivated` (spoofs flags + snapshots prior values) and skips super. For real players: calls super |
| `src/Hooks.cpp:582-593` | `EnhancedGameObject::hasBeenActivatedByPlayer(PlayerObject*)` | For sim players: returns true if per-run set contains object OR either real player has activated it (delegating to engine's flag-reading impl). For real players: returns engine result |

Supporting sim-side bookkeeping (not hooks but related):
- `src/Trajectory.cpp:388-431` — `markActivated`: snapshots `m_activated`/`m_activatedByPlayer1`/`m_activatedByPlayer2` on first sim hit, then sets all three to true matching the sim player
- `src/Trajectory.cpp:433-435` — `hasBeenActivated`: checks per-run set
- `src/Trajectory.cpp:437-445` — `clearActivated`: restores all three flags from snapshot at runPlan/runBranch boundary

## Leak candidates

Methods on these base classes that could be called during sim's super, mutate state shared with real game, and are NOT currently hooked or otherwise neutralized:

1. **`GameObject::resetObject`** (6113, virtual) — if super of any sim path triggers it (e.g. via PlayLayer::resetLevel reentry), it clears `m_isActivated`, position offsets, rotation, and other runtime state on shared objects. Symptom: every orb/ring in level "un-uses" itself mid-run, real player would re-trigger consumed pickups.

2. **`GameObject::activateObject` / `GameObject::deactivateObject`** (6115, 6116) — direct writers of `m_isActivated`. The activation flag spoof in `markActivated` covers the `EnhancedGameObject::activatedByPlayer` entry point but not these. If any sim code path hits `activateObject` on a non-Enhanced object (e.g. a CheckpointGameObject in the EffectGameObject family), `m_isActivated` is written without being captured in `m_activated` snapshot. Symptom: real player encounters an already-activated checkpoint/dynamic-block.

3. **`GameObject::destroyObject`** (6221, non-virtual) — sets `m_isDisabled`. Symptom: sim "destroys" a hazard, then real player passes through where the wall should be.

4. **`GameObject::disableObject`** (6227) — sets `m_isDisabled2`. Same risk class.

5. **`GameObject::playDestroyObjectAnim`** (6292), **`GameObject::playPickupAnimation`** (6293-6294), **`GameObject::spawnDefaultPickupParticle`** (6328), **`GameObject::createAndAddParticle`** (6214) — all spawn particle/sprite children attached to the shared object. Symptom: persistent visual artifacts pile up over many sim iterations (similar bug class to the playShineEffect leak that motivated TrajGameObjectHook in the first place).

6. **`GameObject::setObjectColor`** / **`setGlowColor`** / **`setChildColor`** (6139, 6140, 6106) — write `m_baseColor`/`m_detailColor` pointers and visual child colors. The `m_baseColor`/`m_detailColor` GJSpriteColor pointers themselves reference layer-owned objects so this is shared. Symptom: real player sees colors flicker if a sim trigger path reaches these.

7. **`GameObject::updateMainColor` / `updateSecondaryColor`** (virtual at 6150-6151, non-virtual at 6335, 6339) — refresh color sprite from layer's effect manager. Same risk class.

8. **`GameObject::saveActiveColors`** (6154, virtual; overridden in EnhancedGameObject at 4404) — snapshots color into per-object buffer used by pulse triggers. Could leak pulse history into the shared object if invoked.

9. **`GameObject::groupWasDisabled`** / **`groupWasEnabled`** (6265, 6266) — written by group toggle triggers via `m_isGroupDisabled`. The hook on `EffectGameObject::triggerObject` suppresses the trigger entry, but if any sim code calls these directly (e.g. via a group-enable-counter side path), the toggle leaks. Symptom: collectible orbs in a group stay disabled / enabled after sim, real player loses access.

10. **`GameObject::resetGroupDisabled`** / **`resetMoveOffset`** / **`resetColorGroups`** / **`updateStartValues`** / **`updateStartPos`** (6302, 6305, 6301, 6124, 6342) — writers of dynamic state. Symptom: same family of object-state-leak.

11. **`EnhancedGameObject::resetObject`** (4394) — same risk as base, but specifically clears the activation flags we're spoofing. Symptom: sim flag spoof immediately undone.

12. **`EnhancedGameObject::powerOnObject`** / **`powerOffObject`** / **`stateSensitiveOff`** (4409-4411) — write `m_poweredOn`/`m_state`. Symptom: power-on/off orbs (rare but exist) leak their state to real player.

13. **`EnhancedGameObject::animationTriggered`** / **`triggerAnimation`** / **`updateAnimateOnTrigger`** / **`updateState`** (4400, 4421, 4413, 4423) — write `m_animationTriggered`/`m_state`, kick off cocos action animations. Symptom: shared object stuck animating after sim crosses an animate-on-trigger object.

14. **`EnhancedGameObject::saveActiveColors`** (4404) — same as base override.

15. **`EffectGameObject::restoreObject`** (4025) — resets trigger state; if called outside the snapshot system, leaks the reset.

16. **`EffectGameObject::stateSensitiveOff`** (4032) — engine writes into layer's GJEffectManager via this; not currently gated.

17. **`EffectGameObject::playTriggerEffect`** / **`triggerEffectFinished`** / **`resetSpawnTrigger`** / **`updateSpecialColor`** (4036, 4040, 4037, 4042) — non-virtual side helpers that mutate `m_triggerEffectPlaying` / `m_channelChanged` / `m_endPosition` on the shared trigger object. Symptom: trigger appears to be "still running" or already-completed for real player.

18. **`EffectGameObject::setTargetID`** / **`setTargetID2`** (4038, 4039) — write `m_targetGroupID`/`m_itemID2`. Static config but mutable. Symptom: if sim ever calls these (unlikely path), trigger reroutes to wrong group.

19. **`GameObject::setPosition` / `setScale` / `setRotation` / setFlip*` family** (6096-6108) — direct cocos setters. The move/scale/rotate triggers go through `EffectGameObject::triggerObject` (hooked), but if any sim physics path calls these setters directly on a non-player game object, the shared object's position changes. Symptom: a block "moves" because sim re-positioned it.

20. **`GameObject::makeInvisible` / `makeVisible`** (6286, 6287) — toggle `m_isInvisible`. Symptom: blocks turn invisible for real player.

21. **`GameObject::addRotation`** (6196-6197) — increments rotation. Same risk as setRotation.

22. **`GameObject::dirtifyObjectPos` / `dirtifyObjectRect` / `dirtifyOrientedBox` (via setObjectRectDirty)`** (6225, 6226, 6174) — dirty-flag toggles. Mostly safe (just causes recompute) but combined with a position write could mismatch real player's view of the object's hitbox cache.

The mod has explicit hooks covering the highest-traffic entry points (player→ring activation, effect-trigger dispatch, shine effect), but the broader surface — every cocos-setter on `GameObject`, every `addToTempOffset`, every `resetObject` — relies on no sim code path calling these on shared objects. Worth adding a `assert(!sim().isSimulating())` in the highest-risk non-hooked setters during debug, or expanding the hook set to gate every state-mutating-self method that isn't already covered.
