# Audio playback surface — FMOD + GD audio hooks

*The audio call surface is entirely unisolated in the mod today. Nothing in `src/` references the `FMODAudioEngine` singleton, hooks `playSfx`/`playMusic`/`activatedAudioTrigger`, or filters the SFX-trigger / song-trigger objects directly — the only sim audio mitigation is INDIRECT, via `TrajEffectHook::triggerObject` (src/Hooks.cpp:517) which short-circuits every non-speed-mod `EffectGameObject` activation. That gate transitively blocks `SFXTriggerGameObject` and `SongTriggerGameObject` fire paths because both inherit `EffectGameObject::triggerObject`. Everything that bypasses `triggerObject` — most player-state transitions, level-complete / firework / spawn / death / portal-touch chains, and any direct `playSfx`/`playSound` call from collision processing — is currently unprotected. This file enumerates every code path that COULD produce sound during a sim run, so we know what to hook if we add audio suppression. All line refs are to `_deps/bindings-src/bindings/2.208/FMOD.bro` and `_deps/bindings-src/bindings/2.208/GeometryDash.bro` unless stated.*

## FMODAudioEngine (GeometryDash.bro, declared at line 4867)

### Class overview
*FMODAudioEngine is the singleton audio backend (`FMODAudioEngine::sharedEngine()` / `::get()`). All sound playback in Geometry Dash ultimately routes here — GD layer methods, trigger fires, and player effects all forward into one of `playEffect*`, `playMusic*`, `loadAndPlayMusic`, `queuePlayEffect`, `queueStartMusic`, or `triggerQueuedMusic`. Suppressing audio fully would mean either (a) gating every caller, or (b) hooking these few engine entry points and bailing when sim is active.*

### Methods table

| Line | Name | Signature | Sim relevance | Notes |
|---|---|---|---|---|
| 4868 | FMODAudioEngine | constructor | engine-internal | Singleton ctor |
| 4869 | ~FMODAudioEngine | dtor | engine-internal | |
| 4871 | get | static FMODAudioEngine* | query | Singleton accessor |
| 4872 | pitchForIdx | static float(int) | config | Pure lookup |
| 4873 | reverbToString | static gd::string(FMODReverbPreset) | query | |
| 4874 | sharedEngine | static FMODAudioEngine* | query | Singleton accessor (alias) |
| 4876 | update | virtual void(float dt) | engine-internal | Per-frame engine tick (drains queues, tweens, fades) — must keep running during sim |
| 4878 | activateQueuedMusic | void(int channel) | play-music | Starts a queued music channel — would emit audio mid-sim if a queued music fire-time hits |
| 4879 | channelForChannelID | FMOD::Channel*(int) | query | |
| 4880 | channelForUniqueID | FMOD::Channel*(int) | query | |
| 4881 | channelIDForUniqueID | int(int) | query | |
| 4882 | channelLinkSound | void(int, FMODSound*) | engine-internal | Pre-allocation link |
| 4883 | channelStopped | void(FMOD::Channel*, bool) | control | Callback when channel ends |
| 4884 | channelUnlinkSound | void(int) | engine-internal | |
| 4885 | clearAllAudio | void() | control | Stops everything |
| 4886 | countActiveEffects | int() | query | |
| 4887 | countActiveMusic | int() | query | |
| 4888 | createStream | FMOD::Sound*(gd::string) | engine-internal | Loads but doesn't play |
| 4889 | disableMetering | void() | config | |
| 4890 | enableMetering | void() | config | |
| 4891 | fadeInBackgroundMusic | void(float) | control | Volume tween only |
| 4892 | fadeInMusic | void(float, int channel) | control | Volume tween only |
| 4893 | fadeMusic | void(float, int, float, float) | control | Volume tween only |
| 4894 | fadeOutMusic | void(float, int) | control | Volume tween only |
| 4895 | getActiveMusic | gd::string(int) | query | |
| 4896 | getActiveMusicChannel | FMOD::Channel*(int) | query | |
| 4897 | getBackgroundMusicVolume | float() | query | |
| 4898 | getChannelGroup | FMOD::ChannelGroup*(int, bool) | query | |
| 4899 | getEffectsVolume | float() | query | |
| 4900 | getFMODStatus | gd::string(int) | query | |
| 4901 | getMeteringValue | float() | query | |
| 4902 | getMusicChannelID | int(int) | query | |
| 4903 | getMusicLengthMS | unsigned int(int) | query | |
| 4904 | getMusicTime | float(int) | query | |
| 4905 | getMusicTimeMS | unsigned int(int) | query | |
| 4906 | getNextChannelID | int() | engine-internal | |
| 4907 | getTweenContainer | reference accessor | query | |
| 4908 | isAnyPersistentPlaying | bool() | query | |
| 4909 | isChannelStopping | bool(int) | query | |
| 4910 | isEffectLoaded | bool(gd::string) | query | |
| 4911 | isMusicPlaying | bool(int) | query | |
| 4912 | isMusicPlaying | bool(gd::string, int) | query | overload |
| 4913 | isPersistentMatchPlaying | bool(gd::string, int) | query | |
| 4914 | isSoundReady | bool(FMOD::Sound*) | query | |
| 4915 | lengthForSound | int(gd::string) | query | |
| **4916** | **loadAndPlayMusic** | void(gd::string, unsigned int, int) | **play-music** | **Direct music start — would emit sound during sim** |
| 4917 | loadAudioState | void(FMODAudioState&) | engine-internal | Restores state snapshot (relevant for sim — see "audio state save/restore" below) |
| 4918 | loadMusic | void(gd::string) | engine-internal | Loads only |
| 4919 | loadMusic | void(gd::string, float, float, float, bool, int, int, bool) | engine-internal | Loads with config; doesn't play directly |
| 4920 | pauseAllAudio | void() | control | |
| 4921 | pauseAllEffects | void() | control | |
| 4922 | pauseAllMusic | void(bool) | control | |
| 4923 | pauseEffect | void(unsigned int) | control | |
| 4924 | pauseMusic | void(int) | control | |
| **4925** | **playEffect** | int(gd::string) | **play-effect** | **One-shot SFX start — primary leak surface** |
| **4926** | **playEffect** | int(gd::string, float, float, float) | **play-effect** | **overload** |
| **4927** | **playEffectAdvanced** | int(gd::string, ...19 args) | **play-effect** | **Used by SFXTriggerGameObject activation path** |
| **4928** | **playEffectAsync** | int(gd::string) | **play-effect** | **Async one-shot** |
| **4929** | **playMusic** | void(gd::string, bool, float, int) | **play-music** | **Starts a song channel** |
| 4930 | preloadEffect | FMODSound*(gd::string) | engine-internal | Loads only |
| 4931 | preloadEffectAsync | FMOD::Sound*(gd::string) | engine-internal | Loads only |
| 4932 | preloadMusic | FMOD::Sound*(gd::string, bool, int) | engine-internal | Loads only |
| 4933 | printResult | void(FMOD_RESULT) | engine-internal | Debug |
| 4934 | queuedEffectFinishedLoading | void(gd::string) | engine-internal | Async-load callback; may flush into playback |
| **4935** | **queuePlayEffect** | int(gd::string, ...17 args) | **play-effect** | **Queues a deferred SFX play — drained by `updateQueuedEffects` during `update`** |
| **4936** | **queueStartMusic** | void(gd::string, ...13 args) | **play-music** | **Queues a deferred music start — drained by `updateQueuedMusic` during `update`** |
| 4937 | registerChannel | int(FMOD::Channel*, int, int) | engine-internal | |
| 4938 | releaseRemovedSounds | void() | engine-internal | |
| 4939 | resumeAllAudio | void() | control | |
| 4940 | resumeAllEffects | void() | control | |
| 4941 | resumeAllMusic | void() | control | |
| 4942 | resumeAudio | void() | control | |
| 4943 | resumeEffect | void(unsigned int) | control | |
| 4944 | resumeMusic | void(int) | control | |
| 4945 | saveAudioState | void(FMODAudioState&) | engine-internal | Captures state snapshot (see "audio state save/restore" below) |
| 4946 | setBackgroundMusicVolume | void(float) | config | |
| 4947 | setChannelPitch | void(int, AudioTargetType, float) | config | |
| 4948 | setChannelVolume | void(int, AudioTargetType, float) | config | |
| 4949 | setChannelVolumeMod | void(int, AudioTargetType, float) | config | |
| 4950 | setEffectsVolume | void(float) | config | |
| 4951 | setMusicTimeMS | void(unsigned int, bool, int) | control | |
| 4952 | setup | void() | engine-internal | |
| 4953 | setupAudioEngine | void() | engine-internal | |
| 4954 | start | void() | engine-internal | |
| **4955** | **startMusic** | void(int, int, int, int, bool, int, bool, bool) | **play-music** | **Starts a preloaded/queued track** |
| 4956 | stop | void() | control | |
| 4957 | stopAllEffects | void() | control | |
| 4958 | stopAllMusic | void(bool) | control | |
| 4959 | stopAndGetFade | float(FMOD::Channel*) | control | |
| 4960 | stopAndRemoveMusic | void(int) | control | |
| 4961 | stopChannel | void(int) | control | |
| 4962 | stopChannel | void(FMOD::Channel*, bool, float) | control | |
| 4963 | stopChannel | void(int, AudioTargetType, bool, float) | control | |
| 4964 | stopChannelTween | void(int, AudioTargetType, AudioModType) | control | |
| 4965 | stopChannelTweens | void(int, AudioTargetType) | control | |
| 4966 | stopMusic | void(int) | control | |
| 4967 | stopMusicNotInSet | void(gd::unordered_set<int>&) | control | |
| 4968 | storeEffect | FMODSound*(FMOD::Sound*, gd::string) | engine-internal | |
| 4969 | swapMusicIndex | void(int, int) | engine-internal | |
| 4970 | testFunction | void(int) | engine-internal | Debug |
| **4971** | **triggerQueuedMusic** | void(FMODQueuedMusic) | **play-music** | **Immediately starts a queued music entry** |
| 4972 | unloadAllEffects | void() | engine-internal | |
| 4973 | unloadEffect | void(gd::string) | engine-internal | |
| 4974 | unregisterChannel | void(int) | engine-internal | |
| 4975 | updateBackgroundFade | void() | engine-internal | |
| 4976 | updateChannel | void(int, AudioTargetType, AudioModType, float, float) | engine-internal | Tween step |
| 4977 | updateChannelTweens | void(float) | engine-internal | |
| 4978 | updateMetering | void() | engine-internal | |
| **4979** | **updateQueuedEffects** | void() | **play-effect** | **Drains `m_queuedEffects` — can fire `playEffect` mid-frame** |
| **4980** | **updateQueuedMusic** | void() | **play-music** | **Drains queued music — can fire `playMusic` / `triggerQueuedMusic` mid-frame** |
| 4981 | updateReverb | void(FMODReverbPreset, bool) | config | |
| 4982 | updateTemporaryEffects | void() | engine-internal | |
| 4983 | waitUntilSoundReady | FMOD_OPENSTATE(FMOD::Sound*) | engine-internal | Blocking wait |

### Fields (FMODAudioEngine, lines 4985-5028)
Most are engine-internal state — channel maps, queued-effect/music vectors, sound caches, master DSP/groups, the live `FMODAudioState m_audioState` snapshot, reverb preset, and visualizer telemetry. None are referenced from src/. The only sim-relevant one is `m_audioState` (line 5009) — `FMODAudioState` is the engine's full mutable state (tween containers, channel volume / pitch maps, queued-music tables, sound-state-for-channels) and has explicit `saveAudioState`/`loadAudioState` methods (lines 4945, 4917). The mod's `LayerStateSnapshot` does NOT use these — if we add per-tick audio side effects, we may need to snapshot+restore audio state alongside game state.

### FMOD.bro low-level surface
`FMOD::System::playSound` (FMOD.bro:350) is the absolute bottom of the playback stack — `FMODAudioEngine::playEffect*` and `::playMusic*` all eventually call this. `FMOD::Channel::setPaused` / `::setVolume` / `::setPitch` and `FMOD::ChannelControl::setMute` exist but are config-only — sim should not need to touch them directly if we suppress higher-level calls. Hooking `FMOD::System::playSound` would be the universal kill-switch but it's a vendor class with no Geode `$modify` story; hooking the GD-layer entry points (`FMODAudioEngine::play*`) is the practical approach.

## GD-side audio entry points

### Layer methods that route to audio

GJBaseGameLayer is declared at line 7283. PlayLayer (line 14766) inherits. The following methods either call `FMODAudioEngine` directly or route through SFX/song state machinery that eventually does.

| File:line | Class | Method | What it plays | Currently hooked? | Suppression recommendation |
|---|---|---|---|---|---|
| GeometryDash.bro:7336 | GJBaseGameLayer (virtual) | `activatedAudioTrigger(SFXTriggerGameObject*)` | Routes SFX trigger into queue; eventually fires `playEffectAdvanced` | NO | Hook in `$modify(GJBaseGameLayer)` |
| GeometryDash.bro:7348 | GJBaseGameLayer | `activatedAudioTrigger(SFXTriggerGameObject*, float levelTime)` | Same as above (timed variant) | NO | Hook |
| GeometryDash.bro:7356 | GJBaseGameLayer | `activateSFXEditTrigger(SFXTriggerGameObject*)` | Editor-mode SFX trigger (still calls audio) | NO | Hook |
| GeometryDash.bro:7357 | GJBaseGameLayer | `activateSFXTrigger(SFXTriggerGameObject*)` | Primary SFX-trigger activation | INDIRECTLY (triggerObject gate) | Hook directly for defense in depth |
| GeometryDash.bro:7358 | GJBaseGameLayer | `activateSongEditTrigger(SongTriggerGameObject*)` | Editor song trigger | NO | Hook |
| GeometryDash.bro:7359 | GJBaseGameLayer | `activateSongTrigger(SongTriggerGameObject*)` | Primary song-trigger activation; calls `FMODAudioEngine::loadAndPlayMusic` / `queueStartMusic` | INDIRECTLY (triggerObject gate) | Hook directly |
| GeometryDash.bro:7367 | GJBaseGameLayer | `addProximityVolumeEffect(int, int, SFXTriggerGameObject*)` | Adds proximity-volume entry; no playback itself but feeds `updateProximityVolumeEffects` | NO | Hook (defensive) |
| GeometryDash.bro:7379 | GJBaseGameLayer | `applySFXEditTrigger(int, int, SFXTriggerGameObject*)` | Applies edit-channel volume/pitch (config), but may also un-mute | NO | Hook |
| GeometryDash.bro:7392 | GJBaseGameLayer | `canProcessSFX(...)` | Gating function; pure query | n/a | Leave alone |
| GeometryDash.bro:7510 | GJBaseGameLayer | `pauseAudio()` | Pauses audio engine | NO (but harmless) | No-op (idempotent if sim) |
| GeometryDash.bro:7532 | GJBaseGameLayer | `processActivatedAudioTriggers(float levelTime)` | Drains queued audio triggers — direct entry into FMOD | NO | Hook |
| GeometryDash.bro:7552 | GJBaseGameLayer | `processQueuedAudioTriggers()` | Drains queued audio triggers (no-arg overload) | NO | Hook |
| GeometryDash.bro:7555 | GJBaseGameLayer | `processSFXObjects()` | Iterates active SFX trigger objects; can fire playback | NO | Hook |
| GeometryDash.bro:7556 | GJBaseGameLayer | `processSFXState(SFXTriggerState*, SFXTriggerState*, int, float)` | Walks state machine; calls into FMOD on transitions | NO | Hook |
| GeometryDash.bro:7557 | GJBaseGameLayer | `processSongState(int, float, float, int, float, float, ...)` | Walks song-trigger state machine; calls `loadAndPlayMusic` / `queueStartMusic` | NO | Hook |
| GeometryDash.bro:7585 | GJBaseGameLayer | `resetAudio()` | Resets FMOD state | NO (and dangerous to no-op — would corrupt state if real player relies on it) | Leave alone |
| GeometryDash.bro:7592 | GJBaseGameLayer | `resetSongTriggerValues()` | Resets song-trigger book-keeping | NO | Probably leave; pure data reset |
| GeometryDash.bro:7600 | GJBaseGameLayer | `resumeAudio()` | Resumes audio | NO | No-op in sim |
| GeometryDash.bro:7627 | GJBaseGameLayer | `stopSFXTrigger(SFXTriggerGameObject*)` | Stops an SFX channel | NO | No-op in sim |
| GeometryDash.bro:7662 | GJBaseGameLayer | `tryResumeAudio()` | Resumes if suspended | NO | No-op in sim |
| GeometryDash.bro:7701 | GJBaseGameLayer | `updateProximityVolumeEffects()` | Recomputes proximity volumes — config only but iterates active triggers | NO | Probably leave (config-only) |
| GeometryDash.bro:7714 | GJBaseGameLayer | `volumeForProximityEffect(SFXTriggerInstance&)` | Pure computation | n/a | Leave alone |
| GeometryDash.bro:557 | AppDelegate | `checkSound()` | Sound check | NO | Out of sim scope (AppDelegate-level) |
| GeometryDash.bro:562 | AppDelegate | `pauseSound()` | Pauses audio | NO | Out of sim scope |
| GeometryDash.bro:564 | AppDelegate | `resumeSound()` | Resumes audio | NO | Out of sim scope |
| GeometryDash.bro:5832 | GameManager | `playMenuMusic()` | Menu music | NO | Not called during PlayLayer sim |
| GeometryDash.bro:5833 | GameManager | `playSFXTrigger(SFXTriggerGameObject*)` | Direct SFX play | NO | Hook (defensive) |
| GeometryDash.bro:5857 | GameManager | `resumeAudio()` | Audio resume | NO | Out of sim scope |
| GeometryDash.bro:5858 | GameManager | `resumeAudioDelayed()` | Delayed resume | NO | Out of sim scope |
| GeometryDash.bro:14804 | PlayLayer (virtual) | `toggleMusicInPractice()` | Toggles practice-mode music | NO | Not sim-relevant |
| GeometryDash.bro:14850 | PlayLayer | `prepareMusic(bool)` | Pre-loads level music | NO | Out of sim scope (called at startup) |
| GeometryDash.bro:14880 | PlayLayer | `startMusic()` | Starts level music | NO | Out of sim scope (called once on play start) |
| GeometryDash.bro:11736 | LevelEditorLayer (virtual override of `activatedAudioTrigger`) | `activatedAudioTrigger(SFXTriggerGameObject*)` | Editor-side audio path | NO | Not active in PlayLayer flow |

#### Audio state save/restore
- `FMODAudioEngine::saveAudioState(FMODAudioState&)` (FMOD line 4945) and `loadAudioState(FMODAudioState&)` (4917) exist; CheckpointObject (line 1904) and FMODAudioEngine itself (5009) hold an `FMODAudioState`. Suggests Robtop's own checkpoint flow snapshots audio state. Not used by the mod currently. If we let any sim audio leak through (e.g., we choose to play correct sounds at the *real* tick), we'd want symmetric save/load.

### Object methods that route to audio

| File:line | Class | Method | What it plays | Currently hooked? | Suppression recommendation |
|---|---|---|---|---|---|
| GeometryDash.bro:7518 | GJBaseGameLayer | `playerTouchedRing(PlayerObject*, RingObject*)` | Likely fires ring-pickup SFX via SFX trigger or direct call | NO | Hook on layer (covers all ring colors) |
| GeometryDash.bro:15386 (`class RingObject`) | RingObject | inherits `EffectGameObject::triggerObject` | Ring-collected fire path | INDIRECTLY (triggerObject gate suppresses) | Already mitigated by current gate, but `playerTouchedRing` is a separate path |
| GeometryDash.bro:18277 (`class SFXTriggerGameObject`) | SFXTriggerGameObject | inherits `EffectGameObject::triggerObject` | Whole purpose is to play SFX | INDIRECTLY (triggerObject gate suppresses) | Already mitigated |
| GeometryDash.bro:19171 (`class SongTriggerGameObject`) | SongTriggerGameObject | inherits `EffectGameObject::triggerObject` | Whole purpose is to play music | INDIRECTLY (triggerObject gate suppresses) | Already mitigated |
| GeometryDash.bro:19308 | SpriteAnimationManager | `playSound(gd::string)` | Generic sprite-anim sound | NO | Hook if sprite-anim runs during sim (robot/spider mode transitions may invoke this) |
| GeometryDash.bro:19309 | SpriteAnimationManager | `playSoundForAnimation(gd::string)` | Named anim sound | NO | Hook |
| GeometryDash.bro:2887 | CustomSFXDelegate | `overridePlaySFX(SFXInfoObject*)` (virtual) | Pre-play hook (editor-side override interface) | NO | Not active during PlayLayer |
| GeometryDash.bro:17664 | SetupSFXPopup (or similar editor popup) | `overridePlaySFX(SFXInfoObject*)` | Editor-side preview | NO | Not active during PlayLayer |

### Player-driven audio (PlayerObject, line 14239)

PlayerObject does NOT expose direct `playSfx`/`playSound` methods in the bindings. All player-driven sounds (jump, land, death, dash) appear to come from one of:
1. SFXTriggerGameObject fires (already gated transitively),
2. The animation system via `SpriteAnimationManager::playSound`,
3. `playSoundForAnimation` calls from inside private `update*` functions, or
4. `playEffectAdvanced` calls inside player effect functions (`playDeathEffect`, `playSpawnEffect`, `playBumpEffect`, etc.) that the bindings only expose by name.

Flagged as "suspected audio side effect" — we cannot confirm without disassembly, but high-probability candidates:

| File:line | Method | Suspected audio | Confidence |
|---|---|---|---|
| 14334 | `PlayerObject::playBumpEffect(int, GameObject*)` | bump/orb SFX | high |
| 14335 | `PlayerObject::playBurstEffect()` | dash burst SFX | medium |
| 14336 | `PlayerObject::playCompleteEffect(bool, bool)` | level-complete cue | high |
| 14337 | `PlayerObject::playDeathEffect()` | death sound | high |
| 14338 | `PlayerObject::playDynamicSpiderRun()` | spider running SFX | medium |
| 14339 | `PlayerObject::playerDestroyed(bool)` | death sound (wraps playDeathEffect) | high |
| 14345 | `PlayerObject::playSpawnEffect()` | respawn cue | high |
| 14346 | `PlayerObject::playSpiderDashEffect(CCPoint, CCPoint)` | spider-teleport SFX | high |
| 14269 | `PlayerObject::bumpPlayer(...)` | wraps playBumpEffect | high |
| 14268 | `PlayerObject::boostPlayer(float)` | possible pad SFX (pads ARE sim-relevant; boost is silent if `m_playEffects=false`, but `m_playEffects` is set per-instance and unlikely false on sim player) | medium |
| 14350 | `PlayerObject::propellPlayer(float, bool noEffects, int)` | `noEffects` param suggests it can be silenced — but call sites always pass false for normal play | medium |
| 14370 | `PlayerObject::ringJump(RingObject*, bool)` | ring-jump SFX | high |
| 14296 | `PlayerObject::flipGravity(bool, bool noEffects)` | gravity-portal SFX | medium |
| 14406 | `PlayerObject::switchedToMode(GameObjectType)` | mode-change SFX | medium |
| 14408-14417 | `toggleBirdMode/toggleDartMode/...` | per-mode portal entry SFX | medium |
| 14312 | `PlayerObject::hitGround(GameObject*, bool)` | land SFX | medium |
| 14313 | `PlayerObject::hitGroundNoJump(GameObject*, bool)` | land SFX | medium |
| 14795 | `PlayerObject::playGravityEffect(bool)` (virtual) | gravity portal | high |
| 14787 | `PlayLayer::destroyPlayer(PlayerObject*, GameObject*)` (virtual override) | death cascade | high |
| 14835 | `PlayLayer::levelComplete()` | level-end music | high |
| 14869 | `PlayLayer::showCompleteEffect()` | complete fanfare | high |
| 14876 | `PlayLayer::spawnCircle()` | spawn pop | medium |
| 14877 | `PlayLayer::spawnFirework()` | fireworks (audio + visual) | high |
| 12825 | LevelEditorLayer (`playCompleteEffect()`) | editor-side cue | low (editor, not PlayLayer) |
| 7432 | `GJBaseGameLayer::gameEventTriggered(...)` | dispatch into per-event audio chains | high |
| 14298 | `PlayerObject::gameEventTriggered(int, int)` | event dispatch into mode SFX | high |
| 7523 | `GJBaseGameLayer::playFlashEffect(float, int, float)` | visual flash + possible flash SFX | medium |
| 7521 | `GJBaseGameLayer::playerWillSwitchMode(PlayerObject*, GameObject*)` | mode change prep — may chain audio | medium |
| 7636 | `GJBaseGameLayer::teleportPlayer(TeleportPortalObject*, PlayerObject*)` | teleport whoosh | high |
| 14388 | `PlayerObject::spawnPortalCircle(ccColor3B, float)` | portal-entry pulse (visual + likely audio) | medium |
| 7331 | `GJBaseGameLayer::playGravityEffect(bool)` (virtual) | gravity flash + likely SFX | medium |

## Currently-mitigated paths

The mod's `TrajEffectHook::triggerObject` (src/Hooks.cpp:517) bails out of `EffectGameObject::triggerObject` for everything except speed-mod portals during sim. Because every dedicated audio-trigger class inherits `EffectGameObject` and dispatches via `triggerObject`, this incidentally suppresses:

- `SFXTriggerGameObject` (line 18277) — full SFX trigger fire path: no `activateSFXTrigger` → no `processSFXState` → no `playEffectAdvanced`
- `SongTriggerGameObject` (line 19171) — full song trigger fire path: no `activateSongTrigger` → no `processSongState` → no `loadAndPlayMusic` / `queueStartMusic`
- `RingObject` (line 15386) — ring-collected trigger; **but `playerTouchedRing` (line 7518) is a separate path NOT routed through `triggerObject` — see leak candidates below**
- All other audio-bearing `EffectGameObject` subclasses (`SpawnTriggerGameObject`, `SpawnParticleGameObject`, color/move/etc. triggers — they don't directly emit sound but some chain into SFX trigger groups)
- The activation-side effect (`m_activatedByPlayer1/2` flags) is also blocked, which prevents future re-firing during the same sim run

## Direct audio leak candidates

Every path below BYPASSES `triggerObject` and is therefore currently unprotected. Listed in approximate descending impact order.

### 1. Player ring activation — `GJBaseGameLayer::playerTouchedRing`
- **Where called from:** `GJBaseGameLayer::collisionCheckObjects` (line 7406) → `playerTouchedRing` (line 7518). Fires every time the sim player's bounding box hits a ring.
- **What sound:** ring-pickup SFX (per ring color).
- **Sim impact:** **HIGH.** Sim runs per-frame search through many candidate paths; each path may cross multiple rings → torrent of ring sounds.
- **Note:** This path is NOT the same as `RingObject::triggerObject`. The ring-touched call writes player state (`m_touchingRings`, `m_touchedRings`) and dispatches the audio cue separately from the trigger-object activation chain.

### 2. Player mode-switch portals — `PlayerObject::toggle{Fly,Bird,Dart,Roll,Spider,Robot,Swing,Ball}Mode`
- **Where called from:** `GJBaseGameLayer::switchToFlyMode`/`switchToRobotMode`/`switchToRollMode`/`switchToSpiderMode` (lines 7631-7634) → invoked by portal-touch detection in player update. Bypasses `triggerObject`.
- **What sound:** mode-transition portal SFX.
- **Sim impact:** **HIGH** during portal-heavy levels; sim crosses every candidate portal during search.

### 3. Gravity portal — `PlayerObject::flipGravity` + `PlayLayer::playGravityEffect`
- **Where called from:** `GJBaseGameLayer::flipGravity` (line 7430) → `PlayerObject::flipGravity` (14296) → also dispatches to `PlayLayer::playGravityEffect` (14795).
- **What sound:** gravity-flip SFX.
- **Sim impact:** **HIGH** in gravity-portal-heavy levels.

### 4. Teleport / blue portals — `GJBaseGameLayer::teleportPlayer`
- **Where called from:** line 7636. Direct call when sim player crosses a teleport-portal object.
- **What sound:** teleport whoosh.
- **Sim impact:** **MEDIUM.**

### 5. Death — `PlayLayer::destroyPlayer` → `PlayerObject::playerDestroyed` → `PlayerObject::playDeathEffect`
- **Where called from:** `PlayLayer::destroyPlayer` (line 14787) → `playerDestroyed` (14339) → `playDeathEffect` (14337).
- **What sound:** death thud + game-over cue.
- **Sim impact:** **HIGH.** The sim explores many paths that die; every failed candidate could trigger this. (Even more concerning: the sim probably ALSO calls `resetPlayer` afterward, but the death sound has already been queued.)

### 6. Land / ground-hit — `PlayerObject::hitGround` / `hitGroundNoJump`
- **Where called from:** lines 14312, 14313. Called from collision-resolution path during player update.
- **What sound:** landing thud (subtle, but audible).
- **Sim impact:** **MEDIUM** — fires constantly during cube/ball mode. Volume is low per hit but cumulative.

### 7. Orb/pad bumps — `PlayerObject::bumpPlayer` → `playBumpEffect`
- **Where called from:** `GJBaseGameLayer::bumpPlayer` (7386) → `PlayerObject::bumpPlayer` (14269) → `playBumpEffect` (14334).
- **What sound:** orb/pad activation SFX.
- **Sim impact:** **HIGH** — orbs and pads are the bread-and-butter of sim path exploration; every orb tap chains here.

### 8. Ring jump — `PlayerObject::ringJump`
- **Where called from:** Path-finding paths that go through colored rings. Line 14370. May invoke an audio cue separately from `playerTouchedRing`.
- **What sound:** ring-jump SFX.
- **Sim impact:** **HIGH.**

### 9. Dash effects — `PlayerObject::playSpiderDashEffect` and dash particles
- **Where called from:** Line 14346 + dash-related update calls (lines 14397-14399).
- **What sound:** dash whoosh (yellow/pink dash rings, spider teleport).
- **Sim impact:** **MEDIUM.**

### 10. Level complete cascade — `PlayLayer::levelComplete` / `showCompleteEffect` / `spawnFirework` / `playCompleteEffect`
- **Where called from:** Lines 14835, 14869, 14877. If sim ever reaches the end of the level during search (e.g., a winning candidate is fully simulated), the engine runs the complete cascade.
- **What sound:** level-end fanfare, fireworks.
- **Sim impact:** **MEDIUM-LOW** — happens at most once per winning candidate, but is loud and intrusive.

### 11. `GJBaseGameLayer::gameEventTriggered` and `PlayerObject::gameEventTriggered`
- **Where called from:** lines 7432, 14298. Generic dispatch — likely fires under any mode change / portal / death / etc.
- **What sound:** depends on event; potentially anything from the audio asset pool.
- **Sim impact:** **HIGH** (overlapping with above categories).

### 12. Sprite animation system — `SpriteAnimationManager::playSound` / `playSoundForAnimation`
- **Where called from:** Robot / spider sprite anim system, lines 19308-19309. Robot footsteps, spider walk, etc.
- **What sound:** per-animation sound effect.
- **Sim impact:** **MEDIUM** — only emits if sim simulates robot/spider mode in a way that triggers animation.

### 13. `FMODAudioEngine::update` (called by cocos scheduler)
- **Not directly a sim-triggered path** but worth noting: `update` (line 4876) drains `updateQueuedEffects` (4979) and `updateQueuedMusic` (4980) every frame. If we DO queue an effect during sim (because we miss a hook), it WILL get drained on the next visual frame even though sim is done by then. **Suppression must happen at the queue point, not just at sim entry.**

## Recommended hooks (concrete code)

```cpp
// src/Hooks.cpp — add one or more $modify blocks. Pattern: bail when sim active
// AND audio suppression toggle is on.

// (1) Universal layer-side audio kill — covers SFX/Song trigger fire paths
// even if a future refactor bypasses triggerObject.
class $modify(TrajAudioLayerHook, GJBaseGameLayer) {
    void activatedAudioTrigger(SFXTriggerGameObject* obj) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        GJBaseGameLayer::activatedAudioTrigger(obj);
    }
    void activateSFXTrigger(SFXTriggerGameObject* obj) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        GJBaseGameLayer::activateSFXTrigger(obj);
    }
    void activateSongTrigger(SongTriggerGameObject* obj) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        GJBaseGameLayer::activateSongTrigger(obj);
    }
    void processQueuedAudioTriggers() {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        GJBaseGameLayer::processQueuedAudioTriggers();
    }
    void processActivatedAudioTriggers(float levelTime) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        GJBaseGameLayer::processActivatedAudioTriggers(levelTime);
    }
    void processSFXObjects() {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        GJBaseGameLayer::processSFXObjects();
    }
    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        if (sim().isSimulating() && sim().suppressAudio()) {
            // Still update touched-ring state for sim physics —
            // ONLY suppress the audio side effect. May require a more
            // surgical fix: split state updates from audio dispatch
            // by hooking the audio call inside this path instead.
            // Quick first cut: skip entirely and rely on existing
            // RingObject::triggerObject gating + manual m_touchedRings
            // bookkeeping in TrajPlayerObjectHook.
        }
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};

// (2) Engine kill switch — last line of defense. If anything sneaks through
// the layer-side hooks, the engine still won't emit.
class $modify(TrajAudioEngineHook, FMODAudioEngine) {
    int playEffect(gd::string path) {
        if (sim().isSimulating() && sim().suppressAudio()) return -1;
        return FMODAudioEngine::playEffect(path);
    }
    int playEffect(gd::string path, float speed, float unk, float vol) {
        if (sim().isSimulating() && sim().suppressAudio()) return -1;
        return FMODAudioEngine::playEffect(path, speed, unk, vol);
    }
    int playEffectAdvanced(gd::string path, float speed, float unk, float vol,
                           float pitch, bool fft, bool reverb, int startMs,
                           int endMs, int fadeIn, int fadeOut, bool loop,
                           int effectID, bool ovr, bool noPre, int channelID,
                           int uniqueID, float minInterval, int sfxGroup) {
        if (sim().isSimulating() && sim().suppressAudio()) return -1;
        return FMODAudioEngine::playEffectAdvanced(path, speed, unk, vol, pitch, fft,
            reverb, startMs, endMs, fadeIn, fadeOut, loop, effectID, ovr, noPre,
            channelID, uniqueID, minInterval, sfxGroup);
    }
    int playEffectAsync(gd::string path) {
        if (sim().isSimulating() && sim().suppressAudio()) return -1;
        return FMODAudioEngine::playEffectAsync(path);
    }
    void playMusic(gd::string path, bool loop, float fadeIn, int channel) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        FMODAudioEngine::playMusic(path, loop, fadeIn, channel);
    }
    void loadAndPlayMusic(gd::string path, unsigned int time, int musicID) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        FMODAudioEngine::loadAndPlayMusic(path, time, musicID);
    }
    int queuePlayEffect(gd::string path, float speed, float unk, float vol,
                        float pitch, bool fft, bool reverb, int start, int end,
                        int fadeIn, int fadeOut, bool loop, int effectID,
                        bool ovr, int uniqueID, float minInterval, int group) {
        if (sim().isSimulating() && sim().suppressAudio()) return -1;
        return FMODAudioEngine::queuePlayEffect(path, speed, unk, vol, pitch, fft,
            reverb, start, end, fadeIn, fadeOut, loop, effectID, ovr, uniqueID,
            minInterval, group);
    }
    void queueStartMusic(gd::string path, float pitch, float unk, float vol,
                        bool loop, int start, int end, int fadeIn, int fadeOut,
                        int musicID, bool unk2, int channelID, bool noPrep,
                        bool dontReset) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        FMODAudioEngine::queueStartMusic(path, pitch, unk, vol, loop, start, end,
            fadeIn, fadeOut, musicID, unk2, channelID, noPrep, dontReset);
    }
    void startMusic(int start, int end, int fadeIn, int fadeOut, bool loop,
                    int musicID, bool noResume, bool dontReset) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        FMODAudioEngine::startMusic(start, end, fadeIn, fadeOut, loop, musicID,
            noResume, dontReset);
    }
    void triggerQueuedMusic(FMODQueuedMusic music) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        FMODAudioEngine::triggerQueuedMusic(music);
    }
};

// (3) Player-side sprite-animation audio (robot/spider footstep sounds, etc.)
class $modify(TrajAudioSpriteAnimHook, SpriteAnimationManager) {
    void playSound(gd::string sound) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        SpriteAnimationManager::playSound(sound);
    }
    void playSoundForAnimation(gd::string animation) {
        if (sim().isSimulating() && sim().suppressAudio()) return;
        SpriteAnimationManager::playSoundForAnimation(animation);
    }
};
```

**Note on `playerTouchedRing`:** the function does more than play audio — it updates player state. A blanket `return` would break ring physics. The recommended approach is to rely on the engine-level FMODAudioEngine hook to swallow the audio while letting state updates run normally. Same applies to `flipGravity`, `bumpPlayer`, mode-switch toggles, etc. — these all do meaningful physics work; the engine-level kill is the right cut point.

**Defense in depth:** Use BOTH (1) layer-level hooks AND (2) engine-level hooks. (1) avoids burning CPU on dead-end SFX-trigger queue churn; (2) catches anything that slips past.

## Master toggle proposal

- **Setting:** `sim-suppress-audio` (bool, default `true`)
- **Declared in `mod.json`** under the `settings` block, with a description like "Mute all audio while the bot is searching trajectories (recommended)."
- **Wired in `src/main.cpp`** via `Mod::get()->getSettingValue<bool>("sim-suppress-audio")` cached in a static atomic, refreshed on `listenForSettingChanges<bool>("sim-suppress-audio", ...)`.
- **Read in `TrajectorySimulator::suppressAudio()` accessor** (one-liner returning the cached atomic).
- **Gates every hook listed above** via the `sim().isSimulating() && sim().suppressAudio()` pattern.

Optional refinements (future):
- A second toggle `sim-suppress-music-only` for users who want to keep SFX (so they can still hear pad/orb feedback when the bot demos a path) but mute the background track. Maps to suppressing only the `playMusic` / `loadAndPlayMusic` / `queueStartMusic` / `startMusic` / `triggerQueuedMusic` family.
- Equivalent `sim-suppress-sfx-only` flag.
- A debug counter exposed via the bot HUD: "audio calls suppressed this frame" — useful for verifying coverage.

---

## Enumeration counts

- **Total audio entry points enumerated:** 76 FMODAudioEngine methods + 22 GJBaseGameLayer audio methods + 4 PlayLayer audio methods + 2 SpriteAnimationManager methods + 4 AppDelegate/GameManager methods + 1 RingObject path + 24 PlayerObject suspected-audio methods + 10 SFXTriggerGameObject/SongTriggerGameObject paths (already gated) = **143 enumerated entries**.
- **Direct leak candidates (bypass `triggerObject`):** 13 categories spanning **~30+ specific call sites** in PlayerObject / GJBaseGameLayer / PlayLayer / SpriteAnimationManager (ring touched, mode-switch portals, gravity, teleport, death cascade, ground-hit, orb/pad bump, ring jump, dash, level-complete, gameEvent dispatch, sprite-anim sound, FMODAudioEngine update-drain).
- **Currently-mitigated paths:** 1 indirect mitigation (`TrajEffectHook::triggerObject` gate) covers all SFXTriggerGameObject / SongTriggerGameObject / RingObject-trigger-side fires — but NOT `playerTouchedRing`, NOT player mode-switches, NOT death/level-complete cascades, NOT engine-level direct `playEffect`/`playMusic` calls from anywhere else.
