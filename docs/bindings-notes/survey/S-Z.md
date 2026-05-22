# Bindings Survey: S-Z

Total files in range: 195. Reviewed: 195.

Source root for line numbers: `bindings/bindings/Geode/binding_arm/<File>.hpp` (the `binding/` headers in this range are 9-line stubs that delegate to `binding_arm/` or `binding_intel/`; ARM and Intel layouts are identical for all member structs cited below).

LayerStateSnapshot reference (Trajectory.cpp:30-115) currently captures: cameraZoom, targetCameraZoom, cameraOffset, cameraPosition, cameraPosition2, cameraAngle, targetCameraAngle, cameraEdge[0..3], cameraShakeEnabled, cameraShakeFactor, cameraStepDiff, isDualMode, dualRelated, levelFlipping, gravityRelated, portalY, lastActivatedPortal1, lastActivatedPortal2, timeWarp, queuedTimeWarp, timeWarpRelated, currentChannel, rotateChannel.

Legend:
- **[INTEREST C1..C6]** = directly relevant to one of the user's six unsolved problems (1 portal particles, 2 camera leak, 3 trajectory divergence, 4 orb false-hit, 5 perf/spatial cull, 6 staircase).
- **[INTEREST]** = state-mutation / reference data worth keeping handy.
- **[FX-FLAG]** = visual effect that should be sim-suppressed.

---

## S

- `SaveLayer.hpp` — UI/menu skip.
- `SavedActiveObjectState.hpp` — **[INTEREST C4]** Tiny struct: `m_gameObject`, `m_activatedByPlayer1`, `m_activatedByPlayer2` (binding_arm/SavedActiveObjectState.hpp:15-17). This is the per-object "did P1/P2 already trip me" flag for non-trivial objects (orbs, pads, rings, triggers). For the orb false-hit problem: snapshot a `gd::vector<SavedActiveObjectState>` (or whatever container `GJBaseGameLayer::m_activeObjects` uses) before sim, restore after. Same logic that fixes "ring already activated" needs to apply here.
- `SavedObjectStateRef.hpp` — **[INTEREST]** Per-object positional snapshot: `m_positionX/Y` (double), `m_rotationXOffset/YOffset`, `m_addToCustomScaleX/Y`, `m_unkFloat3/4` (binding_arm/SavedObjectStateRef.hpp:15-23). This is the container GD itself uses for tween/transform restoration; mirroring its layout could simplify our own per-object snapshot for triggers that mutate position/rotation/scale during sim.
- `SavedSpecialObjectState.hpp` — **[INTEREST]** Stores `m_gameObject` + `m_animationID` (binding_arm/SavedSpecialObjectState.hpp:15-16). Used for SpecialAnimGameObject restoration (animated portal/spider/UFO frames). Worth grabbing if sim ever advances animations.
- `ScrollingLayer.hpp` — Cocos scrollable layer (CCLayerColor + touch handling). UI primitive, not gameplay state. Skip.
- `SearchButton.hpp` — UI/menu skip.
- `SearchSFXPopup.hpp` — UI/menu skip.
- `SecretGame01Layer.hpp` — UI/menu skip (vault minigame).
- `SecretLayer.hpp` — UI/menu skip.
- `SecretLayer2.hpp` — UI/menu skip.
- `SecretLayer3.hpp` — UI/menu skip.
- `SecretLayer4.hpp` — UI/menu skip.
- `SecretLayer5.hpp` — UI/menu skip.
- `SecretLayer6.hpp` — UI/menu skip.
- `SecretNumberLayer.hpp` — UI/menu skip.
- `SecretRewardsLayer.hpp` — UI/menu skip.
- `SelectArtDelegate.hpp` — UI/menu skip.
- `SelectArtLayer.hpp` — UI/menu skip.
- `SelectEventLayer.hpp` — UI/menu skip.
- `SelectFontLayer.hpp` — UI/menu skip.
- `SelectListIconDelegate.hpp` — UI/menu skip.
- `SelectListIconLayer.hpp` — UI/menu skip.
- `SelectPremadeDelegate.hpp` — UI/menu skip.
- `SelectPremadeLayer.hpp` — UI/menu skip.
- `SelectSettingDelegate.hpp` — UI/menu skip.
- `SelectSettingLayer.hpp` — UI/menu skip.
- `SelectSFXSortDelegate.hpp` — UI/menu skip.
- `SelectSFXSortLayer.hpp` — UI/menu skip.
- `SequenceTriggerGameObject.hpp` — Sequence trigger (sequenceMode/resetMode/sequenceTotalCount, binding_arm/SequenceTriggerGameObject.hpp:125-131). Mutates a per-object `SequenceTriggerState` (see below) and counters in `m_sequenceTimes`/`m_sequenceIndices`. Inherits ChanceTriggerGameObject. Triggers spawn other groups when fired. Per-trigger state mutation; not in LayerStateSnapshot but the side effects (group activation) are the same as any spawn trigger — handled by `m_activated` / activeObjects rollback path, not by snapshot.
- `SequenceTriggerState.hpp` — **[INTEREST]** Holds two `gd::unordered_map<int,float/int>` (binding_arm/SequenceTriggerState.hpp:14-16). Lives on a SequenceTriggerGameObject; if sim fires a sequence trigger these maps mutate. Restore-strategy: shallow copy of just the two maps before sim per affected object — cheap.
- `SetColorIDPopup.hpp` — UI/menu skip.
- `SetFolderPopup.hpp` — UI/menu skip.
- `SetGroupIDLayer.hpp` — UI/menu skip.
- `SetIDPopup.hpp` — UI/menu skip.
- `SetIDPopupDelegate.hpp` — UI/menu skip.
- `SetItemIDLayer.hpp` — UI/menu skip.
- `SetLevelOrderPopup.hpp` — UI/menu skip.
- `SetTargetIDLayer.hpp` — UI/menu skip.
- `SetTextPopup.hpp` — UI/menu skip.
- `SetTextPopupDelegate.hpp` — UI/menu skip.
- `SetupAdvFollowEditPhysicsPopup.hpp` — Editor UI, skip (subclass of SetupTriggerPopup).
- `SetupAdvFollowPopup.hpp` — Editor UI, skip.
- `SetupAdvFollowRetargetPopup.hpp` — Editor UI, skip.
- `SetupAnimationPopup.hpp` — Editor UI, skip.
- `SetupAnimSettingsPopup.hpp` — Editor UI, skip.
- `SetupAreaAnimTriggerPopup.hpp` — Editor UI, skip.
- `SetupAreaFadeTriggerPopup.hpp` — Editor UI, skip.
- `SetupAreaMoveTriggerPopup.hpp` — Editor UI, skip.
- `SetupAreaRotateTriggerPopup.hpp` — Editor UI, skip.
- `SetupAreaTintTriggerPopup.hpp` — Editor UI, skip.
- `SetupAreaTransformTriggerPopup.hpp` — Editor UI, skip.
- `SetupAreaTriggerPopup.hpp` — Editor UI, skip.
- `SetupArtSwitchPopup.hpp` — Editor UI, skip.
- `SetupAudioLineGuidePopup.hpp` — Editor UI, skip.
- `SetupAudioTriggerPopup.hpp` — Editor UI, skip.
- `SetupBGSpeedTrigger.hpp` — Editor UI, skip.
- `SetupCameraEdgePopup.hpp` — Editor UI, skip.
- `SetupCameraGuidePopup.hpp` — Editor UI, skip.
- `SetupCameraModePopup.hpp` — Editor UI, skip.
- `SetupCameraOffsetTrigger.hpp` — Editor UI, skip.
- `SetupCameraRotatePopup.hpp` — Editor UI, skip.
- `SetupCameraRotatePopup2.hpp` — Editor UI, skip.
- `SetupCheckpointPopup.hpp` — Editor UI, skip.
- `SetupCoinLayer.hpp` — Editor UI, skip.
- `SetupCollisionStateTriggerPopup.hpp` — Editor UI, skip.
- `SetupCollisionTriggerPopup.hpp` — Editor UI, skip.
- `SetupCountTriggerPopup.hpp` — Editor UI, skip.
- `SetupDashRingPopup.hpp` — Editor UI, skip.
- `SetupEndPopup.hpp` — Editor UI, skip.
- `SetupEnterEffectPopup.hpp` — Editor UI, skip.
- `SetupEnterTriggerPopup.hpp` — Editor UI, skip.
- `SetupEventLinkPopup.hpp` — Editor UI, skip.
- `SetupForceBlockPopup.hpp` — Editor UI, skip.
- `SetupGameplayOffsetPopup.hpp` — Editor UI, skip.
- `SetupGradientPopup.hpp` — Editor UI, skip.
- `SetupGravityModPopup.hpp` — Editor UI, skip (subclass SetupTriggerPopup; configures gravity mod field on object — runtime gravity is captured via `gravityRelated` already).
- `SetupGravityTriggerPopup.hpp` — Editor UI, skip (subclass SetupTriggerPopup, binding_arm/SetupGravityTriggerPopup.hpp:13). Confirmed: just the editor configurator, not the runtime trigger.
- `SetupInstantCollisionTriggerPopup.hpp` — Editor UI, skip.
- `SetupInstantCountPopup.hpp` — Editor UI, skip.
- `SetupInteractObjectPopup.hpp` — Editor UI, skip.
- `SetupItemCompareTriggerPopup.hpp` — Editor UI, skip.
- `SetupItemEditTriggerPopup.hpp` — Editor UI, skip.
- `SetupKeyframeAnimPopup.hpp` — Editor UI, skip.
- `SetupKeyframePopup.hpp` — Editor UI, skip.
- `SetupMGTrigger.hpp` — Editor UI, skip.
- `SetupMoveCommandPopup.hpp` — Editor UI, skip.
- `SetupObjectControlPopup.hpp` — Editor UI, skip.
- `SetupObjectOptions2Popup.hpp` — Editor UI, skip.
- `SetupObjectOptionsPopup.hpp` — Editor UI, skip.
- `SetupObjectTogglePopup.hpp` — Editor UI, skip.
- `SetupOpacityPopup.hpp` — Editor UI, skip.
- `SetupOptionsTriggerPopup.hpp` — Editor UI, skip.
- `SetupPersistentItemTriggerPopup.hpp` — Editor UI, skip.
- `SetupPickupTriggerPopup.hpp` — Editor UI, skip.
- `SetupPlatformerEndPopup.hpp` — Editor UI, skip.
- `SetupPlayerControlPopup.hpp` — Editor UI, skip.
- `SetupPortalPopup.hpp` — Editor UI, skip.
- `SetupPulsePopup.hpp` — Editor UI, skip.
- `SetupRandAdvTriggerPopup.hpp` — Editor UI, skip.
- `SetupRandTriggerPopup.hpp` — Editor UI, skip.
- `SetupResetTriggerPopup.hpp` — Editor UI, skip.
- `SetupReverbPopup.hpp` — Editor UI, skip.
- `SetupRotateCommandPopup.hpp` — Editor UI, skip.
- `SetupRotateGameplayPopup.hpp` — Editor UI, skip.
- `SetupRotatePopup.hpp` — Editor UI, skip.
- `SetupSequenceTriggerPopup.hpp` — Editor UI, skip.
- `SetupSFXEditPopup.hpp` — Editor UI, skip.
- `SetupSFXPopup.hpp` — Editor UI, skip.
- `SetupShaderEffectPopup.hpp` — Editor UI, skip.
- `SetupShakePopup.hpp` — Editor UI, skip.
- `SetupSmartBlockLayer.hpp` — Editor UI, skip.
- `SetupSmartTemplateLayer.hpp` — Editor UI, skip.
- `SetupSongTriggerPopup.hpp` — Editor UI, skip.
- `SetupSpawnParticlePopup.hpp` — Editor UI, skip (subclass SetupTriggerPopup, binding_arm/SetupSpawnParticlePopup.hpp:13). UI for editing m_offset/m_offsetVariance/m_rotation etc on SpawnParticleGameObject; particle ID plumbing not visible here, see SpawnParticleGameObject below.
- `SetupSpawnPopup.hpp` — Editor UI, skip.
- `SetupStaticCameraPopup.hpp` — Editor UI, skip.
- `SetupStopTriggerPopup.hpp` — Editor UI, skip.
- `SetupTeleportPopup.hpp` — Editor UI, skip (subclass SetupTriggerPopup, binding_arm/SetupTeleportPopup.hpp:13). Configures TeleportPortalObject fields; does not own runtime state.
- `SetupTimerControlTriggerPopup.hpp` — Editor UI, skip.
- `SetupTimerEventTriggerPopup.hpp` — Editor UI, skip.
- `SetupTimerTriggerPopup.hpp` — Editor UI, skip.
- `SetupTimeWarpPopup.hpp` — Editor UI, skip (subclass SetupTriggerPopup, binding_arm/SetupTimeWarpPopup.hpp:13). Confirms: time-warp triggers are EffectGameObject-derived; runtime state already in `timeWarp/queuedTimeWarp/timeWarpRelated`.
- `SetupTouchTogglePopup.hpp` — Editor UI, skip.
- `SetupTransformPopup.hpp` — Editor UI, skip.
- `SetupTriggerPopup.hpp` — **[INTEREST]** Base class for every Setup\*Popup (binding_arm/SetupTriggerPopup.hpp:16). Owns `m_gameObject` (EffectGameObject\*) + `m_gameObjects` array + page/group/tag dictionaries. Pure editor; useful only as a way to confirm "this is editor UI, not runtime" when scanning future files.
- `SetupZoomTriggerPopup.hpp` — Editor UI, skip.
- `SFXBrowser.hpp` — UI/menu skip.
- `SFXBrowserDelegate.hpp` — UI/menu skip.
- `SFXFolderObject.hpp` — UI/menu skip.
- `SFXInfoObject.hpp` — UI/menu skip.
- `SFXSearchResult.hpp` — UI/menu skip (search result cell).
- `SFXStateContainer.hpp` — Internal SFX runtime substruct (5 unk fields, binding_arm/SFXStateContainer.hpp:15-19). Audio-only, no gameplay state. Skip for sim purposes.
- `SFXTriggerGameObject.hpp` — SFX trigger object (sound path/ID/pitch/volume/loop/groupID, binding_arm/SFXTriggerGameObject.hpp:80-122). Audio side-effect only. Triggers a sound when activated. **Sim should suppress** — already covered if we suppress all `triggerObject` calls during sim, but if we allow triggers (for state changes), this one specifically should be skipped to prevent audio leak. **[FX-FLAG]**
- `SFXTriggerInstance.hpp` — Trivial pair (groupID1/groupID2/controlID/sfxTriggerGameObject\*, binding_arm/SFXTriggerInstance.hpp:14-18). Skip.
- `SFXTriggerState.hpp` — Per-SFX runtime state (sfxTriggerGameObject\*, several unk doubles/floats, m_processed bool, 3-array of SFXStateContainer, binding_arm/SFXTriggerState.hpp:15-29). Audio runtime; sim should keep `m_processed = false` so the SFX never fires, OR suppress at trigger entry.
- `ShaderGameObject.hpp` — **[FX-FLAG]** Shader trigger object (binding_arm/ShaderGameObject.hpp:13). Fields: m_speed/m_strength/m_outer/m_timeOff/m_waveWidth/m_targetX/m_targetY/m_fadeIn/m_fadeOut/m_screenOffsetX/Y/m_invert/m_inner/m_maxSize/m_flip/m_rotate/m_dual/m_useX/m_useY/m_snapGrid/m_hardEdges/m_disableAll/m_zLayerMin/m_zLayerMax/m_animate/m_relative/m_editorDisabled (binding_arm/ShaderGameObject.hpp:71-97). Pure visual; sim must skip `triggerObject` for these.
- `ShaderLayer.hpp` — **[FX-FLAG]** The shader rendering layer itself (CCLayer subclass, binding_arm/ShaderLayer.hpp:13). Owns m_shader/m_renderTexture/m_sprite/m_screenSize/m_targetTextureSize plus dozens of triggerXxx/setupXxx methods (Bulge, ChromaticGlitch, Glitch, Grayscale, HueShift, InvertColor, LensCircle, MotionBlur, Pinch, Pixelate, RadialBlur, Sepia, ShockLine, ShockWave, SplitScreen, ColorChange) (binding_arm/ShaderLayer.hpp:457-628). All triggerXxx are visible side effects with no gameplay impact. Sim must NOT call any trigger\* method here, and during sim we should ideally bypass the layer's `update(dt)` (binding_arm/ShaderLayer.hpp:61) so motion blur sample state doesn't drift. Has `m_savedCameraPosition` / `m_savedCameraRotation` (binding_arm/ShaderLayer.hpp:728-729) — these are read by the shader uniforms and could indirectly leak if camera changes are not restored. C2 reinforcement: confirms camera drift would propagate into shader uniforms.
- `ShardsPage.hpp` — UI/menu skip.
- `ShareCommentDelegate.hpp` — UI/menu skip.
- `ShareCommentLayer.hpp` — UI/menu skip.
- `ShareLevelLayer.hpp` — UI/menu skip.
- `ShareLevelSettingsLayer.hpp` — UI/menu skip.
- `ShareListLayer.hpp` — UI/menu skip.
- `SimpleObject.hpp` — Tiny CCObject with `m_color` (binding_arm/SimpleObject.hpp:13-43). Editor color picker palette entry. Skip.
- `SimplePlayer.hpp` — Player icon preview sprite (CCSprite, binding_arm/SimplePlayer.hpp:13). Used for icon select/UI display. Has m_firstLayer/m_secondLayer/m_birdDome/m_outlineSprite/m_robotSprite/m_spiderSprite (binding_arm/SimplePlayer.hpp:210-216). Not used in PlayLayer simulation. Skip.
- `SlideInLayer.hpp` — UI primitive (CCLayerColor with slide-in animation). Skip.
- `Slider.hpp` — UI primitive. Skip.
- `SliderDelegate.hpp` — UI primitive. Skip.
- `SliderThumb.hpp` — UI primitive. Skip.
- `SliderTouchLogic.hpp` — UI primitive. Skip.
- `SmartGameObject.hpp` — Editor smart-block prefab object (binding_arm/SmartGameObject.hpp:13). Has m_referenceOnly/m_baseFrame/m_smartFrame (binding_arm/SmartGameObject.hpp:71-73). Editor-only at runtime. Skip per Smart Block category.
- `SmartPrefabResult.hpp` — Editor prefab result struct. Skip per Smart Block category.
- `SmartTemplateCell.hpp` — UI/menu skip.
- `SongCell.hpp` — UI/menu skip.
- `SongChannelState.hpp` — Song channel runtime state: two SongTriggerGameObject\* and two doubles (binding_arm/SongChannelState.hpp:14-18). Audio-only. **Note**: `currentChannel` and `rotateChannel` are already in LayerStateSnapshot (Trajectory.cpp:52). Per-channel substate (this struct) is separate and lives somewhere on PlayLayer/m_audioState — could leak audio crossfades into reality but not gameplay.
- `SongInfoLayer.hpp` — UI/menu skip.
- `SongInfoObject.hpp` — Song metadata holder (id/name/artist/url/...). UI data, skip.
- `SongObject.hpp` — Tiny CCObject wrapping audioID (binding_arm/SongObject.hpp:13-43). Skip.
- `SongOptionsLayer.hpp` — UI/menu skip.
- `SongPlaybackDelegate.hpp` — UI/menu skip.
- `SongSelectNode.hpp` — UI/menu skip.
- `SongsLayer.hpp` — UI/menu skip.
- `SongTriggerGameObject.hpp` — Song trigger; inherits SFXTriggerGameObject (binding_arm/SongTriggerGameObject.hpp:13). Adds m_unk7a9/m_prep/m_loadPrep/m_songChannel (binding_arm/SongTriggerGameObject.hpp:62-65). When triggered, swaps the song on `m_songChannel`. Audio-only side effect. Sim must suppress; channel state already snapshotted.
- `SongTriggerState.hpp` — Trivial: songTriggerGameObject\* + double (binding_arm/SongTriggerState.hpp:14-15). Audio runtime, skip.
- `SoundStateContainer.hpp` — Audio fade points and offsets (binding_arm/SoundStateContainer.hpp:14-24). Audio-only, skip.
- `SpawnParticleGameObject.hpp` — **[INTEREST C1]** Particle-spawn trigger object (binding_arm/SpawnParticleGameObject.hpp:13). Inherits **EffectGameObject** (binding_arm/SpawnParticleGameObject.hpp:13). Own fields are *only* spawn parameters — `m_offset` / `m_offsetVariance` (CCPoint), `m_matchRotation` (bool), `m_rotation` / `m_rotationVariance` (float), `m_scale` / `m_scaleVariance` (float) (binding_arm/SpawnParticleGameObject.hpp:62-68). **There is no `m_particleID` here.** Particle identity must be sourced upstream — almost certainly from `EffectGameObject::m_particle` or via the `customSetup`/customObjectSetup vector parsing the level string. The fact that this class has no own particle handle means the particle system itself lives on the parent's m_particle slot. Confirms suppression strategy: hook `triggerObject` (inherited from EffectGameObject; not overridden in this header — so it's GJBaseGameLayer or EffectGameObject that does the spawning) and one-shot suppress any call when sim is active. Alternative: hook `GJBaseGameLayer::spawnParticle` or `GJBaseGameLayer::createParticleAt` which the trigger likely calls into.
- `SpawnTriggerAction.hpp` — **[INTEREST]** Per-frame in-flight action: m_finished/m_disabled/m_duration/m_deltaTime/m_targetGroupID/m_triggerUniqueID/m_controlID/m_spawnOrdered/m_gameObject\*/m_remapKeys (binding_arm/SpawnTriggerAction.hpp:60-69). These are queued in some array on GJBaseGameLayer (likely m_spawnObjects/m_pendingSpawnObjects). When sim fires a delayed Spawn trigger, an action is added; if not popped/canceled before end-of-sim, it'll fire in reality. **Restore strategy**: snapshot the actions array at sim start, restore at end, plus suppress trigger fan-out during sim entirely if possible.
- `SpawnTriggerGameObject.hpp` — Spawn trigger; inherits EffectGameObject (binding_arm/SpawnTriggerGameObject.hpp:14). Owns m_remapObjects/m_remapKey/m_remapKeys/m_currentDelay/m_spawnDelay/m_delayRange/m_resetRemap (binding_arm/SpawnTriggerGameObject.hpp:126-132). Triggers another group with a delay. m_currentDelay is mutable per-frame — would leak from sim into reality if the sim hits a Spawn trigger and we later resume. Same restore strategy as SpawnTriggerAction.
- `SpecialAnimGameObject.hpp` — **[INTEREST]** Animated special object (Spider/UFO portal frame anims). Inherits EnhancedGameObject (binding_arm/SpecialAnimGameObject.hpp:13). Has `updateSyncedAnimation(float totalTime, int frameIndex)` (binding_arm/SpecialAnimGameObject.hpp:88) and m_skipMainColorUpdate/m_skipSecondaryColorUpdate (binding_arm/SpecialAnimGameObject.hpp:98-99). If sim ticks animations, these will progress. Animation drift is cosmetic (not gameplay) but `m_animationID` from SavedSpecialObjectState should be restored if we snapshot.
- `SpriteAnimationManager.hpp` — Animation manager for animated sprites (binding_arm/SpriteAnimationManager.hpp:12). Owns priority/type/sound dicts plus current/next/queued animation strings. Cosmetic. Skip — but be aware that runAnimation/queueAnimation calls during sim would visually leak.
- `SpriteDescription.hpp` — Empty / data-only (likely just struct of string + frame). Skip.
- `SpritePartDelegate.hpp` — Delegate interface. Skip.
- `Standalones.hpp` — Empty translation unit (no class). Skip.
- `StarInfoPopup.hpp` — UI/menu skip.
- `StartPosObject.hpp` — **[INTEREST]** Start-position marker placed in editor (binding_arm/StartPosObject.hpp:13). Inherits EffectGameObject. Has only `m_startSettings: LevelSettingsObject*` (binding_arm/StartPosObject.hpp:89). Method `setSettings(LevelSettingsObject*)` (binding_arm/StartPosObject.hpp:88) and `loadSettingsFromString(gd::string)` (binding_arm/StartPosObject.hpp:79). When you start a level mid-way (Start Pos), the LevelSettingsObject on this start-pos overrides the level's defaults (gamemode, gravity, mini, dual, speed, etc.). Relevant to level setup: the autosolver needs to honour `m_startSettings` to know what mode/gravity/speed the player begins in. Read this when initialising the sim's player state at level start; if the active StartPos changes (rare, but possible via "Switch StartPos" triggers), re-pull.
- `StatsCell.hpp` — UI/menu skip.
- `StatsLayer.hpp` — UI/menu skip.
- `StatsObject.hpp` — Tiny key-value pair (key: const char\*, value: int, binding_arm/StatsObject.hpp:34-35). Stats record. Skip.
- `SupportLayer.hpp` — UI/menu skip.

## T

- `TableView.hpp` — UI primitive, skip.
- `TableViewCell.hpp` — UI primitive, skip.
- `TableViewCellDelegate.hpp` — UI primitive, skip.
- `TableViewDataSource.hpp` — UI primitive, skip.
- `TableViewDelegate.hpp` — UI primitive, skip.
- `TeleportPortalObject.hpp` — **[INTEREST C1, C3]** Critical. Located at binding_arm/TeleportPortalObject.hpp:13. Inherits **RingObject** (note: NOT EffectGameObject — this is a *ring*, so it activates on touch like jump rings, not on trigger). Class layout (binding_arm/TeleportPortalObject.hpp:161-179):
  - `TeleportPortalObject* m_orangePortal` — paired (orange) destination for blue portals.
  - `bool m_isYellowPortal` — flag identifying which colour it is. **Note despite the name "yellow" this is the boolean that distinguishes the two halves of a teleport pair.**
  - `float m_teleportYOffset` — vertical delta applied on teleport.
  - `bool m_teleportEase` — whether camera eases vs. snap.
  - `bool m_staticForceEnabled`, `float m_staticForce` — preserved velocity on exit.
  - `bool m_redirectForceEnabled`, `float m_redirectForceMod`, `float m_redirectForceMin/Max` — velocity redirection coefficients.
  - `bool m_saveOffset`, `bool m_ignoreX`, `bool m_ignoreY` — coordinate transform flags.
  - `int m_gravityMode` — gravity behaviour on exit (preserve / flip / set).
  - `bool m_staticForceAdditive` — force composition flag.
  - `bool m_instantCamera` — snap camera (vs. ease).
  - `bool m_snapGround` — snap to nearest ground tile on exit.
  - `bool m_redirectDash` — re-aim active dash on teleport.
  - `cocos2d::CCPoint m_teleportPosition` — computed exit position cache.
  Methods of note: `getTeleportXOff(CCNode*)` (binding_arm/TeleportPortalObject.hpp:133), `setStartPos(CCPoint)` (line 61), `setStartPosOverride(CCPoint)` (line 160), `setPositionOverride(CCPoint)` (line 151).
  **No `m_targetTeleport` field exists in the binding.** The pairing is via `m_orangePortal` (the partner pointer). The naming of "blue/orange teleport" matches GD's runtime: blue is the entry, orange is the exit. The position cache `m_teleportPosition` is read by the player's teleport logic.
  **Observations for autosolver:**
  - For trajectory divergence (C3): the redirect-force / static-force fields produce velocity changes that depend on the player's incoming dash and angle. If the sim copies the player's velocity but doesn't clone the dash/touch state correctly, the post-teleport velocity will diverge → silent desync.
  - The `m_instantCamera` and `m_snapGround` flags will mutate `gameState.m_cameraPosition` and the player Y on teleport. Already covered for camera by LayerStateSnapshot, but `m_snapGround` writes the player Y directly — sim must restore player Y from copyAttributes baseline, which we already do; verify it does NOT bleed onto the real player.
  - The teleport logic likely calls `gameState.m_lastActivatedPortal1/2 = this` — that is captured. Also likely `m_portalY` — captured.
  - `m_teleportPosition` is recomputed every teleport from `m_orangePortal`'s position, so it's not state we need to roll back, but a mid-sim change to the orange's position via a Move trigger would invalidate the cache. If sim runs a Move trigger that displaces the orange portal, real player will use stale `m_teleportPosition` — could explain trajectory divergence on levels with moving teleport pairs.
- `TextAlertPopup.hpp` — UI/menu skip.
- `TextArea.hpp` — UI primitive, skip.
- `TextAreaDelegate.hpp` — UI primitive, skip.
- `TextGameObject.hpp` — In-level text label object (binding_arm/TextGameObject.hpp:13). Inherits GameObject. Fields: m_text (gd::string), m_kerning (int) (binding_arm/TextGameObject.hpp:89-90). No collision logic of its own. Skip.
- `TextInputDelegate.hpp` — UI primitive, skip.
- `TextStyleSection.hpp` — UI primitive, skip.
- `TimerItem.hpp` — **[INTEREST]** Stateful timer record (binding_arm/TimerItem.hpp:14-26): m_itemID/m_time(double)/m_active/m_timeMod/m_ignoreTimeWarp/m_targetTime/m_stopTimeEnabled/m_targetGroupID/m_triggerUniqueID/m_controlID/m_remapKeys/m_disabled. Lives in the timers array on GJBaseGameLayer. **Each frame `m_time` advances**; if sim fires a TimerTrigger or just runs while timers exist, m_time advances and leaks. Restore strategy: snapshot the timers vector or snapshot `m_time` per timer at sim start.
- `TimerTriggerAction.hpp` — In-flight timer action (binding_arm/TimerTriggerAction.hpp:33-41): m_disabled/m_time/m_targetTime/m_targetGroupID/m_triggerUniqueID/m_controlID/m_itemID/m_multiActivate/m_remapKeys. Same concerns as SpawnTriggerAction; pendingActions arrays must be snapshotted.
- `TimerTriggerGameObject.hpp` — Timer trigger object; inherits EffectGameObject (binding_arm/TimerTriggerGameObject.hpp:13). Owns m_startTime/m_targetTime/m_stopTimeEnabled/m_dontOverride/m_ignoreTimeWarp/m_timeMod/m_startPaused/m_multiActivate/m_controlType (binding_arm/TimerTriggerGameObject.hpp:71-79). Trigger-only fields are read-only from the level data; the running state is in TimerItem. Suppress `triggerObject` during sim.
- `ToggleTriggerAction.hpp` — In-flight toggle action (binding_arm/ToggleTriggerAction.hpp:33-38): m_disabled/m_targetGroupID/m_activateGroup/m_triggerUniqueID/m_controlID/m_remapKeys. Toggles a group on/off. Group activation state must be snapshotted (probably m_groupStates or similar on GJBaseGameLayer). Out of scope of this header but flagged.
- `TopArtistsLayer.hpp` — UI/menu skip.
- `TOSPopup.hpp` — UI/menu skip.
- `TouchToggleAction.hpp` — Touch-toggle action (binding_arm/TouchToggleAction.hpp:33-41): m_disabled/m_targetGroupID/m_holdMode/m_touchTriggerType/m_touchTriggerControl/m_triggerUniqueID/m_controlID/m_dualMode/m_remapKeys. State change driven by player input; sim must NOT inject these from the simulated player or they'll leak.
- `TransformTriggerGameObject.hpp` — Transform trigger; inherits EffectGameObject (binding_arm/TransformTriggerGameObject.hpp:13). Transforms target group's scale/rotation. Fields m_objectScaleX/Y/m_property450/m_property451/m_onlyMove/m_divideX/m_divideY/m_relativeRotation/m_relativeScale (binding_arm/TransformTriggerGameObject.hpp:71-79). Applied via `triggerObject`; mutates per-object position/rotation/scale of the *target* group. If sim fires this, **objects in the level move/scale/rotate and the changes persist into reality** unless we restore each affected GameObject. This is a likely candidate for trajectory divergence (C3): triggers fire in sim → real geometry shifts. Restore strategy: snapshot all GameObjects in target group's positions/rotations/scales OR suppress trigger fan-out entirely during sim.
- `TriggerControlGameObject.hpp` — Trigger-control object (binding_arm/TriggerControlGameObject.hpp:13). Inherits EffectGameObject. Owns m_triggerControlFrame (gd::string) + m_customTriggerValue (GJActionCommand) (binding_arm/TriggerControlGameObject.hpp:80-81). Acts as a meta-trigger that controls other triggers via control-IDs. Same suppression treatment.
- `TriggerEffectDelegate.hpp` — Pure-virtual delegate interface (binding_arm/TriggerEffectDelegate.hpp:12). Methods `toggleGroupTriggered`, `spawnGroup`, `spawnObject` (binding_arm/TriggerEffectDelegate.hpp:23-41). PlayLayer / GJBaseGameLayer implements these — these are *the* fan-out methods that propagate trigger effects. **Hook target candidate**: routing all three through a sim-suppression check would cleanly block all trigger side effects without per-trigger work.
- `TutorialLayer.hpp` — UI/menu skip per Tutorial UI category.
- `TutorialPopup.hpp` — UI/menu skip per Tutorial UI category.

## U

- `UIButtonConfig.hpp` — UI/menu skip.
- `UILayer.hpp` — In-game HUD overlay (CCLayerColor, binding_arm/UILayer.hpp:12). Owns the pause button, jump-button visuals, dpad. Has `m_p1Jumping`/`m_p2Jumping`/`m_p1TouchId`/`m_p2TouchId`/`m_inPlatformer`/`m_dualMode`/`m_dpadType`/`m_editorMode` (binding_arm/UILayer.hpp:346-358). `isJumpButtonPressed()` (line 204) — the gameplay polls this to know if a jump is held. **Relevant for C6 (staircase failures):** if the autosolver injects jumps via `UILayer::isJumpButtonPressed` returning true, the buffered-jump-on-land logic depends on the *frame* that the press occurs. Worth verifying that the autosolver's input injection path matches PlayLayer's expectations exactly for the landing frame. `handleKeypress(key, down, timestamp)` (line 186) takes a timestamp — if our injection passes the wrong timestamp the buffered jump window may close early.
- `UIObjectSettingsPopup.hpp` — Editor UI for UISettingsGameObject, skip.
- `UIOptionsLayer.hpp` — UI/menu skip.
- `UIPOptionsLayer.hpp` — UI/menu skip.
- `UISaveLoadLayer.hpp` — UI/menu skip.
- `UISettingsGameObject.hpp` — UI-positioning trigger object (binding_arm/UISettingsGameObject.hpp:13). Inherits EffectGameObject. Fields m_xRef/m_yRef/m_xRelative/m_yRelative (binding_arm/UISettingsGameObject.hpp:62-65). Repositions UI elements; visual leak only. **[FX-FLAG]**
- `UndoObject.hpp` — Editor undo entry (binding_arm/UndoObject.hpp:13). Editor-only. Skip.
- `UpdateAccountSettingsPopup.hpp` — UI/menu skip.
- `UploadActionDelegate.hpp` — UI/menu skip.
- `UploadActionPopup.hpp` — UI/menu skip.
- `UploadListPopup.hpp` — UI/menu skip.
- `UploadMessageDelegate.hpp` — UI/menu skip.
- `UploadPopup.hpp` — UI/menu skip.
- `UploadPopupDelegate.hpp` — UI/menu skip.
- `URLCell.hpp` — UI/menu skip.
- `URLViewLayer.hpp` — UI/menu skip.
- `UserInfoDelegate.hpp` — UI/menu skip.
- `UserListDelegate.hpp` — UI/menu skip.

## V

- `VideoOptionsLayer.hpp` — UI/menu skip.

## W

- `WorldLevelPage.hpp` — UI/menu skip (world map page).
- `WorldSelectLayer.hpp` — UI/menu skip (world map screen).

---

## Summary of findings in this range

1. **C1 (portal particles)**: `SpawnParticleGameObject.hpp` has no `m_particleID` of its own — it inherits from EffectGameObject. The particle id and particle-system pointer must live on the parent. Hook target: the inherited `triggerObject` (or `GJBaseGameLayer`'s spawn-particle method) — single one-shot suppression for the duration of sim is the cleanest approach. The trigger config fields here (m_offset/m_offsetVariance/m_rotation/m_scale) are static level data, not runtime state.

2. **C1/C2 ALSO covered by `TriggerEffectDelegate`** (binding_arm/TriggerEffectDelegate.hpp:12) — implementing or hooking the three methods (`toggleGroupTriggered`, `spawnGroup`, `spawnObject`) would block all trigger fan-out at one chokepoint, eliminating particle fires, song/SFX changes, transform mutations, and shader triggers in a single shot.

3. **C2 (camera leak)**: `ShaderLayer` (binding_arm/ShaderLayer.hpp:13) has `m_savedCameraPosition`/`m_savedCameraRotation` (lines 728-729) and many trigger\* methods that read camera state. If sim's camera-state restoration is incomplete, shader uniforms drift. ShaderGameObject and ShockWave/ShockLine/etc. all funnel through here. Verify Trajectory.cpp restores enough of the camera to keep ShaderLayer.update consistent on the next real frame.

4. **C3 (trajectory divergence)**: Two prime suspects in this range:
   - `TeleportPortalObject` pair logic: m_orangePortal pointer + recomputed m_teleportPosition cache. If a Move trigger relocates the orange half during sim (and we don't restore), real player will use stale m_teleportPosition. Snapshot any object whose group could be moved by simulated Move/Transform triggers.
   - `TransformTriggerGameObject` (binding_arm/TransformTriggerGameObject.hpp:13): when fired, mutates target group objects' scale/rotation/position — leaks into reality unless suppressed or per-object snapshot taken.

5. **C4 (orb false-hit)**: `SavedActiveObjectState` (binding_arm/SavedActiveObjectState.hpp:13) is the per-object "already activated by P1/P2" record. The `m_activated.clear()` in TrajectorySimulator (Trajectory.cpp:198) addresses our internal tracker, but the real-player's `SavedActiveObjectState` records on PlayLayer must also be reset. If sim sets m_activatedByPlayer1=true on an orb, real player sees it as already used. Verify rollback path covers the m_activeObjects array of `SavedActiveObjectState`s.

6. **C5 (perf / spatial cull)**: No spatial-grid binding in this range. PlayLayer's broad-phase is in earlier bindings (probably `GJBaseGameLayer.hpp` / `LevelSettingsObject` not in S-Z). The trigger objects here all run via group-id dispatch (m_targetGroupID / m_remapKeys), not spatial — orthogonal to the cull problem. The collision narrow-phase is in PlayerObject / GameObject — also out of range.

7. **C6 (staircase)**: `UILayer::handleKeypress(key, down, timestamp)` (binding_arm/UILayer.hpp:186) and `isJumpButtonPressed(bool player1)` (line 213) — the *exact* moment the buffered-jump-on-land check reads the jump state matters. If autosolver injects the press one frame too late or via a different mechanism than UILayer's normal path, the land-frame check misses. Worth tracing whether PlayerObject's `m_isOnGround`/jump-buffer logic reads UILayer or its own internal pressed-flag.

8. **State-mutating triggers in this range** that need either suppression or per-object snapshot during sim (none currently in LayerStateSnapshot):
   - SequenceTriggerGameObject (writes its own m_sequenceState maps).
   - SpawnTriggerGameObject (writes m_currentDelay; queues SpawnTriggerActions).
   - TimerTriggerGameObject (writes/creates TimerItem records; m_time advances).
   - TransformTriggerGameObject (mutates target group geometry).
   - SongTriggerGameObject / SFXTriggerGameObject (audio side effects + `m_processed`).
   - ShaderGameObject / UISettingsGameObject (visual only — flag for FX-suppress, but they do mutate ShaderLayer state).
   - TriggerControlGameObject (meta-trigger, fan-out).

9. **Suggested architectural change** based on this range: introduce a `g_simSuppressTriggers` (or similar) flag checked in `PlayLayer/GJBaseGameLayer::toggleGroupTriggered`/`spawnGroup`/`spawnObject` (the TriggerEffectDelegate methods). With that flag set during sim, *all* trigger-driven mutations in this range stop at the front door. Particle, SFX, song, shader, transform, sequence, timer, spawn — all gated by one flag. This is strictly stronger than per-binding hooks and addresses C1, partial C2, partial C3 (the trigger-driven part of divergence), and the audio leak in one stroke.

10. **Not-in-range but referenced**: ParticleGameObject, EffectGameObject, RingObject, GJBaseGameLayer, PlayerObject, GameObject, LevelSettingsObject, EnhancedGameObject, ChanceTriggerGameObject — these are the parents/dependencies that own the actual runtime state most of the S-Z trigger headers add fields to. Survey of those should be cross-checked when designing the snapshot/suppression layer.
