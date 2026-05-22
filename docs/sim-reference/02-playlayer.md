# PlayLayer — sim-relevance audit

*PlayLayer is the level container PlayerObject runs inside. The mod's `TrajPlayLayerHook` in `src/Hooks.cpp` hooks `setupHasCompleted`, `resetLevel`, `onQuit`, `destroyPlayer`, `playEndAnimationToPos`, `playPlatformerEndAnimationToPos`, and `playGravityEffect`. PlayLayer also inherits from `GJBaseGameLayer` (audited separately in `03-gjbasegamelayer.md`) — PlayLayer-specific overrides/additions only here, not duplicated. `m_gameState` (GJGameState) lives on `GJBaseGameLayer`, not PlayLayer — most of the snapshot/restore in `LayerStateSnapshot` actually targets that inherited member (see `04-gjgamestate.md`); the only PlayLayer-declared field captured directly is `m_speedObjects`.*

## Summary stats

- Total methods declared on PlayLayer (not inherited): **130**
- Total fields declared on PlayLayer: **99**
- Methods needing isolation: **~38** (7 currently hooked, ~31 leak candidates)
- Fields needing snapshot/restore: **~13** (1 currently in LayerStateSnapshot: `m_speedObjects`; 12 not captured)

## Methods table

| Line | Name | Signature | Virtual? | Classification | Sim impact / notes |
|------|------|-----------|----------|----------------|--------------------|
| 14767 | PlayLayer | `PlayLayer()` | no | lifecycle | Constructor, not a hook surface during runPlan. |
| 14768 | ~PlayLayer | `~PlayLayer()` | yes (implicit) | lifecycle | Destructor; sim must not outlive PlayLayer (handled via `onPlayLayerQuit`). |
| 14770 | create | `static PlayLayer* create(GJGameLevel*, bool, bool)` | no (static) | lifecycle | Constructs new PlayLayer; ours runs `setupHasCompleted` afterward. |
| 14771 | get | `static PlayLayer* get()` | no (static) | read-only | Singleton accessor, harmless. |
| 14772 | scene | `static cocos2d::CCScene* scene(GJGameLevel*, bool, bool)` | no (static) | lifecycle | Constructs the CCScene wrapping a new PlayLayer. |
| 14774 | onEnterTransitionDidFinish | `void onEnterTransitionDidFinish()` | yes | lifecycle | Engine-side scene entry; pre-runPlan. |
| 14775 | onExit | `void onExit()` | yes | lifecycle | Engine-side scene exit; sim is torn down via `onPlayLayerQuit`. |
| 14776 | postUpdate | `void postUpdate(float dt)` | yes | engine-internal | Per-tick post-physics hook; sim does not call this path. |
| 14777 | checkForEnd | `void checkForEnd()` | yes | state-mutating-game | Detects level completion, fires `levelComplete()`. If sim hits this through normal updateCamera/postUpdate chain, would trigger the real win screen — currently NOT hooked. **LEAK CANDIDATE.** (Mitigated in practice because sim runs out-of-band via simulate(), not via postUpdate, but worth a defensive hook.) |
| 14779 | updateVerifyDamage | `void updateVerifyDamage()` | yes | state-mutating-game | Anti-cheat damage verification; suppressed indirectly because sim doesn't post-update. **Leak candidate** if sim ever invokes it. |
| 14780 | updateAttemptTime | `void updateAttemptTime(float attemptTime)` | yes | state-mutating-game | Increments attempt timer (`m_attemptTime`). **LEAK CANDIDATE.** |
| 14781 | updateVisibility | `void updateVisibility(float dt)` | yes | visual | Visibility culling. Read-only-ish but writes per-object opacity. Suppress for sim if it gets called. |
| 14782 | opacityForObject | `float opacityForObject(GameObject*)` | yes | read-only | Pure-function-ish; returns opacity. Harmless. |
| 14783 | updateColor | `void updateColor(cocos2d::ccColor3B&, float, int, bool, float, cocos2d::ccHSVValue&, int, bool, EffectGameObject*, int, int)` | yes | state-mutating-layer | Mutates the active color palette (`m_keyColors`, `m_keyOpacities`). Color-trigger sim path — **LEAK CANDIDATE.** Color palette persists past sim end → real player sees the color the sim crossed. |
| 14784 | activateEndTrigger | `void activateEndTrigger(int, bool, bool)` | yes | state-mutating-game | Fires level-end flow. **LEAK CANDIDATE — must suppress for sim** or sim crossing an end trigger triggers the real win screen. |
| 14785 | activatePlatformerEndTrigger | `void activatePlatformerEndTrigger(EndTriggerGameObject*, gd::vector<int> const&)` | yes | state-mutating-game | Platformer-mode level end; also sets `m_platformerEndTrigger`. **LEAK CANDIDATE.** |
| 14786 | toggleGlitter | `void toggleGlitter(bool)` | yes | visual | Toggles `m_glitterEnabled`. Visual + state. |
| 14787 | destroyPlayer | `void destroyPlayer(PlayerObject*, GameObject*)` | yes | state-mutating-game | **HOOKED** — TrajPlayLayerHook drops the call if the player is a sim player (except for `m_anticheatSpike`). Sim must not kill the real player. |
| 14788 | toggleGroundVisibility | `void toggleGroundVisibility(bool)` | yes | visual | Visibility of ground bar. Visual; mutates layer state. |
| 14789 | toggleMGVisibility | `void toggleMGVisibility(bool)` | yes | visual | Middleground visibility. |
| 14790 | toggleHideAttempts | `void toggleHideAttempts(bool)` | yes | visual | Attempt label visibility. |
| 14791 | timeForPos | `float timeForPos(cocos2d::CCPoint, int, int, bool, int)` | yes | read-only | Position → time conversion; pure. |
| 14792 | posForTime | `cocos2d::CCPoint posForTime(float)` | yes | read-only | Time → position conversion; pure. |
| 14793 | resetSPTriggered | `void resetSPTriggered()` | yes | state-mutating-layer | Resets song-pulse triggered flag. Sim song triggers are gated upstream. |
| 14794 | updateTimeWarp | `void updateTimeWarp(float)` | yes | state-mutating-layer | Updates `m_gameState.m_timeWarp` (covered by snapshot's `timeWarp/queuedTimeWarp`). Safe. |
| 14795 | playGravityEffect | `void playGravityEffect(bool)` | yes | visual | **HOOKED** — suppressed for sim. Particle/CCAction effects can't be cleanly unwound. |
| 14796 | manualUpdateObjectColors | `void manualUpdateObjectColors(GameObject*)` | yes | state-mutating-layer | Forces a color update for one object. Color-state side effects similar to `updateColor`. |
| 14797 | checkpointActivated | `void checkpointActivated(CheckpointGameObject*)` | yes | state-mutating-game | Activates a checkpoint, sets `m_activatedCheckpoint`. **LEAK CANDIDATE** — sim crossing a checkpoint would arm a checkpoint at sim's position for the real player. |
| 14798 | flipArt | `void flipArt(bool)` | yes | visual | Flip art on Y-flip trigger. CCAction-driven. **Leak candidate** if sim activates Y-flip and the action runs past sim. |
| 14799 | updateTimeLabel | `void updateTimeLabel(int, int, bool)` | yes | visual | Updates time label text. |
| 14800 | checkSnapshot | `void checkSnapshot()` | yes | engine-internal | Engine-side checkpoint snapshot system. Not called per-tick during sim. |
| 14801 | toggleProgressbar | `void toggleProgressbar()` | yes | visual | Progress bar visibility. |
| 14802 | toggleInfoLabel | `void toggleInfoLabel()` | yes | visual | Info label visibility. |
| 14803 | removeAllCheckpoints | `void removeAllCheckpoints()` | yes | state-mutating-game | Wipes practice checkpoints. **LEAK CANDIDATE** if called from sim. |
| 14804 | toggleMusicInPractice | `void toggleMusicInPractice()` | yes | audio | Practice-mode music toggle. |
| 14805 | currencyWillExit | `void currencyWillExit(CurrencyRewardLayer*)` | yes | engine-internal | CurrencyRewardDelegate callback. UI-driven. |
| 14806 | circleWaveWillBeRemoved | `void circleWaveWillBeRemoved(CCCircleWave*)` | yes | engine-internal | CCCircleWaveDelegate callback. Visual-side. |
| 14807 | dialogClosed | `void dialogClosed(DialogLayer*)` | yes | engine-internal | DialogDelegate callback. UI-driven. |
| 14809 | addCircle | `void addCircle(CCCircleWave*)` | no | visual | Append visual circle wave. **LEAK CANDIDATE** — sim-triggered circle wave is added to layer. |
| 14810 | addObject | `void addObject(GameObject*)` | no | state-mutating-layer | Adds object to level mid-play. Sim should never hit this path; level objects pre-exist. |
| 14811 | addToGroupOld | `void addToGroupOld(GameObject*)` | no | state-mutating-layer | Group bookkeeping; legacy. |
| 14812 | applyCustomEnterEffect | `void applyCustomEnterEffect(GameObject*, bool)` | no | visual | Object enter-effect animation. |
| 14813 | applyEnterEffect | `void applyEnterEffect(GameObject*, int, bool)` | no | visual | Object enter-effect animation. |
| 14814 | canPauseGame | `bool canPauseGame()` | no | read-only | Boolean getter. |
| 14815 | checkpointWithID | `CheckpointObject* checkpointWithID(int)` | no | read-only | Checkpoint lookup. |
| 14816 | colorObject | `void colorObject(int, cocos2d::ccColor3B)` | no | state-mutating-layer | Sets one object's color override. Color state similar to updateColor. |
| 14817 | commitJumps | `void commitJumps()` | no | persistent-stat | Flushes `m_uncommittedJumps` into total stats. **LEAK CANDIDATE** — if sim's jumps got counted toward player save. |
| 14818 | compareStateSnapshot | `void compareStateSnapshot()` | no | engine-internal | Save-restore diagnostic; not a runtime hook surface. |
| 14819 | createCheckpoint | `CheckpointObject* createCheckpoint()` | no | state-mutating-game | Creates a checkpoint at the player. **LEAK CANDIDATE** if sim crosses a checkpoint trigger. |
| 14820 | createObjectsFromSetupFinished | `void createObjectsFromSetupFinished()` | no | lifecycle | Called at end of level setup. |
| 14821 | delayedFullReset | `void delayedFullReset()` | no | lifecycle | Deferred reset. Not sim-relevant. |
| 14822 | delayedResetLevel | `void delayedResetLevel()` | no | lifecycle | Deferred reset. Not sim-relevant. |
| 14823 | fullReset | `void fullReset()` | no | lifecycle | Full-reset variant; covered by our resetLevel hook semantics (sim recomputes after reset). |
| 14824 | getCurrentPercent | `float getCurrentPercent()` | no | read-only | Percentage progress. |
| 14825 | getCurrentPercentInt | `int getCurrentPercentInt()` | no | read-only | Integer percentage. |
| 14826 | getEndPosition | `cocos2d::CCPoint getEndPosition()` | no | read-only | Level end position. |
| 14827 | getLastCheckpoint | `CheckpointObject* getLastCheckpoint()` | no | read-only | Last checkpoint accessor. |
| 14828 | getRelativeMod | `float getRelativeMod(cocos2d::CCPoint, float, float, float)` | no | read-only | Math helper for fade/opacity. |
| 14829 | getRelativeModNew | `float getRelativeModNew(cocos2d::CCPoint, float, float, bool, bool)` | no | read-only | Math helper variant. |
| 14830 | getTempMilliTime | `double getTempMilliTime()` | no | read-only | Time helper. |
| 14831 | gravityEffectFinished | `void gravityEffectFinished()` | no | state-mutating-layer | Decrements `m_activeGravityEffects`. **Leak candidate** if called for sim-fired gravity effect — but `playGravityEffect` is already suppressed for sim, so this should not fire from sim. |
| 14832 | incrementJumps | `void incrementJumps()` | no | persistent-stat | Increments `m_jumps` / `m_uncommittedJumps`. **LEAK CANDIDATE** — sim must not bump real jump stats. |
| 14833 | init | `bool init(GJGameLevel*, bool, bool)` | no | lifecycle | Constructor body. Sim init runs via `setupHasCompleted` after this. |
| 14834 | isGameplayActive | `bool isGameplayActive()` | no | read-only | Boolean getter. |
| 14835 | levelComplete | `void levelComplete()` | no | state-mutating-game | Awards completion (stars/orbs/diamonds), shows complete screen. **LEAK CANDIDATE — critical.** Sim must not trigger this. (Currently mitigated indirectly: sim doesn't drive `checkForEnd`/`postUpdate`, and the end animations are hooked.) |
| 14836 | loadActiveSaveObjects | `void loadActiveSaveObjects(...)` | no | engine-internal | Checkpoint save/restore. |
| 14837 | loadDefaultColors | `void loadDefaultColors()` | no | state-mutating-layer | Initial color setup. Lifecycle, not per-tick. |
| 14838 | loadDynamicSaveObjects | `void loadDynamicSaveObjects(...)` | no | engine-internal | Checkpoint save/restore. |
| 14839 | loadFromCheckpoint | `void loadFromCheckpoint(CheckpointObject*)` | no | state-mutating-game | Reverts to checkpoint state. Lifecycle-ish. |
| 14840 | loadLastCheckpoint | `CheckpointObject* loadLastCheckpoint()` | no | state-mutating-game | Reverts to last checkpoint. **Leak candidate** if sim invokes. |
| 14841 | markCheckpoint | `CheckpointObject* markCheckpoint()` | no | state-mutating-game | Drops a practice-mode checkpoint at player. **LEAK CANDIDATE** if sim presses Z (in practice mode). |
| 14842 | onQuit | `void onQuit()` | no | lifecycle | **HOOKED** — TrajPlayLayerHook calls `sim().onPlayLayerQuit()` before super so sim tears down its pointers before PlayLayer destruction. |
| 14843 | optimizeColorGroups | `void optimizeColorGroups()` | no | lifecycle | One-shot setup optimization. |
| 14844 | optimizeOpacityGroups | `void optimizeOpacityGroups()` | no | lifecycle | One-shot setup optimization. |
| 14845 | pauseGame | `void pauseGame(bool)` | no | state-mutating-layer | Pauses gameplay. **Leak candidate** if sim triggers a pause path (e.g. through a dialog trigger). |
| 14846 | playEndAnimationToPos | `void playEndAnimationToPos(cocos2d::CCPoint)` | no | visual / state-mutating-game | **HOOKED** — suppressed for sim. |
| 14847 | playPlatformerEndAnimationToPos | `void playPlatformerEndAnimationToPos(cocos2d::CCPoint, bool)` | no | visual / state-mutating-game | **HOOKED** — suppressed for sim. |
| 14848 | playReplay | `void playReplay(gd::string)` | no | engine-internal | Replay playback; not a sim path. |
| 14849 | prepareCreateObjectsFromSetup | `void prepareCreateObjectsFromSetup(gd::string&)` | no | lifecycle | One-shot setup. |
| 14850 | prepareMusic | `void prepareMusic(bool)` | no | audio | Audio setup. |
| 14851 | processCreateObjectsFromSetup | `void processCreateObjectsFromSetup()` | no | lifecycle | One-shot setup. |
| 14852 | processLoadedMoveActions | `void processLoadedMoveActions()` | no | lifecycle | Setup of pre-loaded move triggers. |
| 14853 | queueCheckpoint | `void queueCheckpoint()` | no | state-mutating-layer | Sets `m_tryPlaceCheckpoint`. **LEAK CANDIDATE** — would queue a real-player checkpoint at sim's location. |
| 14854 | removeAllObjects | `void removeAllObjects()` | no | lifecycle | Level teardown. |
| 14855 | removeCheckpoint | `void removeCheckpoint(bool)` | no | state-mutating-game | Pops a practice checkpoint. **LEAK CANDIDATE** if sim presses X. |
| 14856 | removeFromGroupOld | `void removeFromGroupOld(GameObject*)` | no | state-mutating-layer | Group bookkeeping; legacy. |
| 14857 | resetLevel | `void resetLevel()` | no | lifecycle | **HOOKED** — sim calls `onPlayLayerReset()` after super. |
| 14858 | resetLevelFromStart | `void resetLevelFromStart()` | no | lifecycle | Reset to spawn. |
| 14859 | resume | `void resume()` | no | state-mutating-layer | Unpause flow. |
| 14860 | resumeAndRestart | `void resumeAndRestart(bool)` | no | lifecycle | Unpause + reset. |
| 14861 | saveActiveSaveObjects | `void saveActiveSaveObjects(...)` | no | engine-internal | Checkpoint save. |
| 14862 | saveDynamicSaveObjects | `void saveDynamicSaveObjects(...)` | no | engine-internal | Checkpoint save. |
| 14863 | scanActiveSaveObjects | `void scanActiveSaveObjects()` | no | engine-internal | Checkpoint helper. |
| 14864 | scanDynamicSaveObjects | `void scanDynamicSaveObjects()` | no | engine-internal | Checkpoint helper. |
| 14865 | screenFlipObject | `void screenFlipObject(GameObject*)` | no | visual | Flip-trigger per-object animation. |
| 14866 | setDamageVerifiedIdx | `void setDamageVerifiedIdx(int)` | no | state-mutating-game | Anti-cheat damage idx. |
| 14867 | setupHasCompleted | `void setupHasCompleted()` | no | lifecycle | **HOOKED** — sim calls `onPlayLayerInit(this)` before super, then `setLevelReady(true)` after. |
| 14868 | shouldBlend | `bool shouldBlend(int)` | no | read-only | Color-blending predicate. |
| 14869 | showCompleteEffect | `void showCompleteEffect()` | no | visual / state-mutating-game | End-screen effects. **Leak candidate** if sim drives it (mitigated by end-anim hooks). |
| 14870 | showCompleteText | `void showCompleteText()` | no | visual / state-mutating-game | "Level complete" overlay text. |
| 14871 | showEndLayer | `void showEndLayer()` | no | state-mutating-game | Pushes end-screen scene. **LEAK CANDIDATE.** |
| 14872 | showHint | `void showHint()` | no | visual | Hint label show. |
| 14873 | showNewBest | `void showNewBest(bool, int, int, bool, bool, bool)` | no | persistent-stat | Saves new-best overlay + stats. **LEAK CANDIDATE — critical.** |
| 14874 | showRetryLayer | `void showRetryLayer()` | no | state-mutating-game | Retry overlay. |
| 14875 | showTwoPlayerGuide | `void showTwoPlayerGuide()` | no | visual | Two-player tutorial guide. |
| 14876 | spawnCircle | `void spawnCircle()` | no | visual | Spawns a CCCircleWave on death/end. **Leak candidate.** |
| 14877 | spawnFirework | `void spawnFirework()` | no | visual | End-screen firework. **Leak candidate** if invoked. |
| 14878 | startGame | `void startGame()` | no | lifecycle | First-play start; covered by setupHasCompleted ordering. |
| 14879 | startGameDelayed | `void startGameDelayed()` | no | lifecycle | Deferred start. |
| 14880 | startMusic | `void startMusic()` | no | audio | Begins music. |
| 14881 | startRecording | `void startRecording()` | no | engine-internal | Verifier replay record. |
| 14882 | startRecordingDelayed | `void startRecordingDelayed()` | no | engine-internal | Deferred record start. |
| 14883 | stopRecording | `void stopRecording()` | no | engine-internal | Stop verifier record. |
| 14884 | storeCheckpoint | `void storeCheckpoint(CheckpointObject*)` | no | state-mutating-game | Adds checkpoint to `m_checkpointArray`. **LEAK CANDIDATE** for sim checkpoint paths. |
| 14885 | takeStateSnapshot | `void takeStateSnapshot()` | no | engine-internal | Engine state-snapshot mechanism. |
| 14886 | toggleBGEffectVisibility | `void toggleBGEffectVisibility(bool)` | no | visual | Toggles `m_bgEffectDisabled`. |
| 14887 | toggleDebugDraw | `void toggleDebugDraw(bool)` | no | visual | Debug draw toggle. |
| 14888 | toggleGhostEffect | `void toggleGhostEffect(int)` | no | visual | Ghost-trail effect toggle. |
| 14889 | toggleIgnoreDamage | `void toggleIgnoreDamage(bool)` | no | state-mutating-game | Toggles `m_isIgnoreDamageEnabled`. **Leak candidate** — sim flipping this would make real player invincible. |
| 14890 | togglePracticeMode | `void togglePracticeMode(bool)` | no | state-mutating-game | Switches practice/normal. **Leak candidate.** |
| 14891 | tryStartRecord | `void tryStartRecord()` | no | engine-internal | Verifier helper. |
| 14892 | updateAttempts | `void updateAttempts()` | no | persistent-stat | Updates attempt counter + label, saves to GameStatsManager. **LEAK CANDIDATE.** |
| 14893 | updateEffectPositions | `void updateEffectPositions()` | no | visual | Repositions visual effects. |
| 14894 | updateInfoLabel | `void updateInfoLabel()` | no | visual | Info label text update. |
| 14895 | updateInvisibleBlock | `void updateInvisibleBlock(GameObject*, float, float, float, float, cocos2d::ccColor3B const&)` | no | visual | Invisible-block opacity update. |
| 14896 | updateProgressbar | `void updateProgressbar()` | no | visual | Progress bar position. |
| 14897 | updateScreenRotation | `void updateScreenRotation(int, bool, bool, float, int, float, int, int)` | no | state-mutating-layer | PlayLayer-side wrapper; the GJBaseGameLayer version is suppressed for sim in `TrajBaseLayerHook` — **note signature difference** (this is `int rotation`, base is `float rot`). Verify hooking the right one. |
| 14898 | updateTestModeLabel | `void updateTestModeLabel()` | no | visual | Test-mode label. |
| 14899 | updateTimeWarp | `void updateTimeWarp(EffectGameObject*, float)` | no | state-mutating-layer | Speed-mod helper; mutates `m_gameState.m_timeWarp` (snapshot covers it). |

## Fields table

| Line | Name | Type | Classification | Sim impact / notes |
|------|------|------|----------------|--------------------|
| 14901 | m_unk36c8 | `int` | unknown | Unknown PlayLayer scalar. Not in snapshot. |
| 14902 | m_unk36cc | `bool` | unknown | Unknown PlayLayer flag. |
| 14903 | m_unk36cd | `bool` | unknown | Unknown PlayLayer flag. |
| 14904 | m_unk36ce | `bool` | unknown | Unknown PlayLayer flag. |
| 14905 | m_unk36cf | `bool` | unknown | Unknown PlayLayer flag. |
| 14906 | m_damageVerified | `bool` | engine-internal | Anti-cheat damage-verification state. |
| 14907 | m_objectStrings | `gd::vector<gd::string>` | engine-internal | Per-object string blobs from level encoding. Read-only post-setup. |
| 14908 | m_coinArray | `cocos2d::CCArray*` | shared-container | Pickup coins array. **LEAK CANDIDATE** if sim collects/modifies; mitigated because coins are filtered out of sim's collision set (Hooks.cpp:176-177). |
| 14909 | m_passedIntegrity | `bool` | engine-internal | Anti-cheat integrity. |
| 14910 | m_objectsCreated | `int` | engine-internal | Setup counter. |
| 14911 | m_dynamicSaveObjects | `gd::vector<GameObject*>` | shared-container | Dynamic save-state list. Read post-setup; sim path doesn't touch normally. |
| 14912 | m_activeSaveObjects1 | `gd::vector<GameObject*>` | shared-container | Save-objects bucket 1. |
| 14913 | m_activeSaveObjects2 | `gd::vector<GameObject*>` | shared-container | Save-objects bucket 2. |
| 14914 | m_dynamicSaveObjects2 | `gd::vector<SavedObjectStateRef>` | shared-container | Dynamic save-state refs. |
| 14915 | m_unk3768 | `int` | unknown | Unknown scalar. |
| 14916 | m_platformerRestart | `bool` | layer-game-state | Platformer restart flag. Could change with sim end-trigger path; not in snapshot. |
| 14917 | m_unk376d | `bool` | unknown | Unknown flag. |
| 14918 | m_isIgnoreDamageEnabled | `bool` | layer-game-state | Damage-ignore flag (set by `toggleIgnoreDamage`). Sim shouldn't toggle but if it did the real player would become invincible. |
| 14919 | m_statusLabel | `cocos2d::CCLabelBMFont*` | ui-element | Status label. |
| 14920 | m_unk3778 | `float` | unknown | Unknown scalar. |
| 14921 | m_unk377c | `int` | unknown | Unknown scalar. |
| 14922 | m_unk3780 | `float` | unknown | Unknown scalar. |
| 14923 | m_unk3784 | `float` | unknown | Unknown scalar. |
| 14924 | m_unk3788 | `int` | unknown | Unknown scalar. |
| 14925 | m_unk378c | `int` | unknown | Unknown scalar. |
| 14926 | m_endChecked | `bool` | layer-game-state | "End checked" flag from `checkForEnd`. Not in snapshot — if sim path ever drives `checkForEnd` this would latch. |
| 14927 | m_endXPosition | `float` | level-data | Level end X. Immutable mid-play. |
| 14928 | m_currentCheckpoint | `CheckpointObject*` | layer-game-state | Active checkpoint pointer. Not in snapshot — sim hitting `createCheckpoint`/`storeCheckpoint`/`loadFromCheckpoint` would change this for the real player. **LEAK CANDIDATE.** |
| 14929 | m_checkpointArray | `cocos2d::CCArray*` | shared-container | Practice checkpoints array. Same risk class as `m_currentCheckpoint`. |
| 14930 | m_speedObjects | `cocos2d::CCArray*` | shared-container | **IN SNAPSHOT** — captured + restored pointer-set in LayerStateSnapshot::capture/restore. Sim-crossed speed-mod portal entries are stripped at run boundary. |
| 14931 | m_unk37b0 | `bool` | unknown | Unknown flag. |
| 14932 | m_unk37b1 | `bool` | unknown | Unknown flag. |
| 14933 | m_enterEffectPosition | `cocos2d::CCPoint` | layer-game-state | Position state for enter-effect spawning. Not in snapshot — sim crossing an enter-effect trigger would update this; only a minor cosmetic leak. |
| 14934 | m_unk37c0 | `cocos2d::CCArray*` | unknown | Unknown CCArray. **Possible leak surface** — being a container, sim writes could leak. |
| 14935 | m_isSilent | `bool` | audio | Silent-mode flag. |
| 14936 | m_unk37cc | `int` | unknown | Unknown scalar. |
| 14937 | m_circleWaveArray | `cocos2d::CCArray*` | shared-container | Active CCCircleWave instances. Sim-fired `addCircle` adds here and the wave persists on real screen — **LEAK CANDIDATE.** |
| 14938 | m_collectibles | `cocos2d::CCArray*` | shared-container | Collectible objects (pickup-coin-like). Coin filtering in `collisionCheckObjects` mitigates this, but any non-coin collectible path may leak. |
| 14939 | m_unk37e0 | `bool` | unknown | Unknown flag. |
| 14940 | m_pulseRodIndex | `int` | layer-game-state | Pulse-rod counter for visual triggers. Not in snapshot. |
| 14941 | m_maxObjectX | `float` | level-data | Furthest-right object X. Immutable mid-play. |
| 14942 | m_attemptLabel | `cocos2d::CCLabelBMFont*` | ui-element | Attempt label. |
| 14943 | m_percentageLabel | `cocos2d::CCLabelBMFont*` | ui-element | Percentage label. |
| 14944 | m_decimalPercentage | `bool` | ui-element | Decimal-format toggle for label. |
| 14945 | m_hintShown | `bool` | layer-game-state | "Hint already shown" flag. |
| 14946 | m_progressBar | `cocos2d::CCSprite*` | ui-element | Progress bar sprite. |
| 14947 | m_progressFill | `cocos2d::CCSprite*` | ui-element | Progress bar fill sprite. |
| 14948 | m_progressWidth | `float` | ui-element | Progress bar width. |
| 14949 | m_progressHeight | `float` | ui-element | Progress bar height. |
| 14950 | m_totalGravityEffects | `int` | layer-game-state | Counter for spawned gravity effects. Sim should never increment this — playGravityEffect is suppressed for sim. |
| 14951 | m_activeGravityEffects | `int` | layer-game-state | Active counter. Same. |
| 14952 | m_gravityEffectIndex | `int` | layer-game-state | Rotating index. Same. |
| 14953 | m_gravityEffects | `cocos2d::CCArray*` | shared-container | Pre-allocated gravity-effect pool. Same suppression-gate. |
| 14954 | m_doNot | `bool` | unknown | Curiously-named flag (likely an Unk). |
| 14955 | m_unk383c | `float` | unknown | Unknown scalar. |
| 14956 | m_skipAudioStep | `bool` | audio | Skip-audio-tick flag. |
| 14957 | m_blendingColors | `gd::unordered_set<int>` | layer-game-state | Set of color-IDs marked as blending. Mutated by `shouldBlend`/color triggers. **Not in snapshot** — sim crossing a color-blend trigger leaks this set. |
| 14958 | m_jumps | `int` | persistent-stat | Jump count for this attempt. **LEAK CANDIDATE** — `incrementJumps` would bump this; sim must not. |
| 14959 | m_hasJumped | `bool` | layer-game-state | Per-attempt has-jumped flag. |
| 14960 | m_uncommittedJumps | `int` | persistent-stat | Pending jump increments. **LEAK CANDIDATE.** |
| 14961 | m_showLeaderboardPercentage | `bool` | ui-element | Leaderboard display flag. |
| 14962 | m_hasCompletedLevel | `bool` | persistent-stat | Has-completed flag. **LEAK CANDIDATE** — sim must not set this. |
| 14963 | m_inResetDelay | `bool` | lifecycle | Mid-reset flag. |
| 14964 | m_lastAttemptPercent | `int` | persistent-stat | Last-attempt percent for save. |
| 14965 | m_endLayerStars | `bool` | persistent-stat | Star award flag. |
| 14966 | m_orbs | `int` | persistent-stat | Orbs earned this attempt. **LEAK CANDIDATE.** |
| 14967 | m_diamonds | `int` | persistent-stat | Diamonds earned this attempt. **LEAK CANDIDATE.** |
| 14968 | m_secretKey | `bool` | persistent-stat | Secret-key award flag. |
| 14969 | m_recordingStopped | `bool` | engine-internal | Verifier replay state. |
| 14970 | m_unk38b0 | `double` | unknown | Unknown timer. |
| 14971 | m_unk38b8 | `double` | unknown | Unknown timer. |
| 14972 | m_unk38c0 | `double` | unknown | Unknown timer. |
| 14973 | m_unk38c8 | `bool` | unknown | Unknown flag. |
| 14974 | m_unk38cc | `float` | unknown | Unknown scalar. |
| 14975 | m_unk38d0 | `int` | unknown | Unknown scalar. |
| 14976 | m_attemptTime | `double` | layer-game-state | Elapsed attempt time. Not in snapshot — sim doesn't tick it through `updateAttemptTime` normally, but `updateAttemptTime` is unhooked. |
| 14977 | m_bestAttemptTime | `double` | persistent-stat | Best attempt time saved. |
| 14978 | m_pauseTime | `double` | layer-game-state | Total paused duration. |
| 14979 | m_currentTime | `double` | layer-game-state | Current wall-clock time anchor. Not in snapshot. |
| 14980 | m_pauseDelta | `double` | layer-game-state | Pause delta accumulator. |
| 14981 | m_unk3900 | `float` | unknown | Unknown scalar. |
| 14982 | m_glitterEnabled | `bool` | layer-game-state | Glitter toggle (set by `toggleGlitter`). Not in snapshot. |
| 14983 | m_bgEffectDisabled | `bool` | layer-game-state | BG-effect disable flag. Not in snapshot. |
| 14984 | m_unk3906 | `bool` | unknown | Unknown flag. |
| 14985 | m_isPaused | `bool` | layer-game-state | Pause flag. |
| 14986 | m_disableGravityEffect | `bool` | layer-game-state | Disables `playGravityEffect`. |
| 14987 | m_infoLabel | `cocos2d::CCLabelBMFont*` | ui-element | Info label. |
| 14988 | m_unk3918 | `cocos2d::CCPoint` | unknown | Unknown point. |
| 14989 | m_unk3920 | `cocos2d::CCPoint` | unknown | Unknown point. |
| 14990 | m_colorKeyDict | `cocos2d::CCDictionary*` | shared-container | Color-key → channel map. Read mostly; mutation via color triggers. |
| 14991 | m_keyColors | `gd::vector<cocos2d::ccColor3B>` | shared-container | Current color palette per channel. **LEAK CANDIDATE** — sim crossing a color trigger writes here and the real player sees the color persist. Not in snapshot. |
| 14992 | m_keyOpacities | `gd::vector<float>` | shared-container | Current opacity per channel. Same leak class. |
| 14993 | m_keyPulses | `DynamicBitset` | shared-container | Pulse-state bitset per channel. |
| 14994 | m_nextColorKey | `int` | layer-game-state | Next color-key index. |
| 14995 | m_tryPlaceCheckpoint | `bool` | layer-game-state | Set by `queueCheckpoint`. **LEAK CANDIDATE** — sim path could arm a real checkpoint placement on next tick. |
| 14996 | m_activatedCheckpoint | `CheckpointGameObject*` | layer-game-state | Most-recently-activated checkpoint object. **LEAK CANDIDATE** if sim crosses a checkpoint. |
| 14997 | m_musicPrepared | `bool` | layer-game-state | Music-ready flag. |
| 14998 | m_endPosition | `cocos2d::CCPoint` | level-data | Cached end position. |
| 14999 | m_platformerEndTrigger | `EndTriggerGameObject*` | layer-game-state | Active platformer end trigger. **LEAK CANDIDATE** — sim crossing a platformer end trigger sets this; real player sees end. |

## Cross-reference with current isolation

### Methods hooked in src/Hooks.cpp (TrajPlayLayerHook)

| Hook | Semantics | Reason |
|------|-----------|--------|
| `setupHasCompleted` | lifecycle | sim().onPlayLayerInit(this) BEFORE super (so sim state is fresh before engine's setup-tick updateCamera); setLevelReady(true) AFTER super. |
| `resetLevel` | lifecycle | super, then sim().onPlayLayerReset(). |
| `onQuit` | lifecycle | sim().onPlayLayerQuit() BEFORE super (so sim drops pointers before destruction). |
| `destroyPlayer` | save-restore (suppress for sim player) | If `gameObject != m_anticheatSpike` and player is sim's player, drop the call → sim death tracked in sim, real player untouched. Anticheat-spike path always allowed so engine's anti-cheat integrity stays intact. |
| `playEndAnimationToPos` | suppress | `if (sim.isSimulating()) return;` — sim crossing end position would play the real end animation. |
| `playPlatformerEndAnimationToPos` | suppress | Same as above for platformer mode. |
| `playGravityEffect` | suppress | Particle/CCAction visual leak; flipGravity already passes noEffects=true for sim, but `playGravityEffect` can also be called directly via the engine's own path, so the most-derived virtual is hooked here. |

### Fields used by LayerStateSnapshot in src/Trajectory.cpp (lines 34-227)

Most snapshot targets are in `m_gameState` (a GJGameState — inherited from GJBaseGameLayer, audited in 04-gjgamestate.md). The only direct PlayLayer-declared field captured:

| Field | Capture | Restore |
|-------|---------|---------|
| `m_speedObjects` | Iterate `arr` and copy `CCObject*` pointers into `std::vector` | `removeAllObjects()` then re-add captured pointers. Strips entries the sim added via `addToSpeedObjects` while crossing speed-mod portals. |

Also captured: positions of inherited `m_groundLayer`/`m_groundLayer2`/`m_middleground` (these live on GJBaseGameLayer; restored with `stopAllActions()` to cancel sim-fired animate*Ground tweens).

### Leak candidates (state-mutating-* methods unhooked, or layer-game-state / shared-container fields unsnapshotted)

#### High-risk (would visibly desync real game)
- `levelComplete` (14835), `showEndLayer` (14871), `showNewBest` (14873), `activateEndTrigger` (14784), `activatePlatformerEndTrigger` (14785): if sim hits these during runPlan → real player sees the level-end / new-best screens for a sim trajectory. Currently mitigated indirectly because sim doesn't drive `postUpdate`/`checkForEnd` and the end animations are hooked, but no defensive gate on the level-end functions themselves.
- `checkForEnd` (14777): if sim ever triggers post-update path → end detection fires for sim trajectory.
- `markCheckpoint` (14841), `createCheckpoint` (14819), `storeCheckpoint` (14884), `checkpointActivated` (14797), `queueCheckpoint` (14853): if sim crosses a CheckpointGameObject → a real checkpoint gets armed at sim's position. The `m_currentCheckpoint`, `m_checkpointArray`, `m_tryPlaceCheckpoint`, `m_activatedCheckpoint` fields all carry the leak.
- `removeAllCheckpoints` (14803), `removeCheckpoint` (14855), `loadFromCheckpoint` (14839), `loadLastCheckpoint` (14840): inverse direction; sim-triggered checkpoint loads would teleport the real player.
- `toggleIgnoreDamage` (14889), `togglePracticeMode` (14890): if sim flips either → real player becomes invincible / mode flips.

#### Stat-corruption (persistent save leak)
- `incrementJumps` (14832), `commitJumps` (14817): if sim hits these → real `m_jumps` / `m_uncommittedJumps` bumped → next `commitJumps` writes inflated jump count to player save.
- `updateAttempts` (14892), `updateAttemptTime` (14780): if sim drives → attempt counter / time saved with sim runs.
- `m_orbs`, `m_diamonds`, `m_secretKey`, `m_endLayerStars`, `m_hasCompletedLevel`, `m_lastAttemptPercent`, `m_bestAttemptTime` (14962-14967, 14977): all persistent-stat fields. If any state-mutating-game function above leaks into them, the save file is wrong.

#### Visual / state leaks
- `updateColor` (14783), `manualUpdateObjectColors` (14796), `colorObject` (14816), and the `m_keyColors` / `m_keyOpacities` / `m_keyPulses` / `m_colorKeyDict` / `m_blendingColors` fields: if sim crosses a color trigger → real player's color palette flips for the rest of the attempt. Currently NO snapshot of color palette state. Color triggers are presumably gated upstream (Hooks.cpp `triggerObject` path), but no defense-in-depth at this layer.
- `toggleGlitter`, `toggleGroundVisibility`, `toggleMGVisibility`, `toggleHideAttempts`, `toggleProgressbar`, `toggleInfoLabel`, `toggleBGEffectVisibility`, `toggleDebugDraw`, `toggleGhostEffect` (14786-14790, 14801-14802, 14886-14888): if sim hits any → visual toggles flip and the snapshot's POD-prefix doesn't cover them (they live in PlayLayer, not GJGameState). Cosmetic but visible.
- `flipArt` (14798): if sim crosses Y-flip → flip CCAction starts on real art.
- `addCircle` (14809), `spawnCircle` (14876), `spawnFirework` (14877): visual additions. `addCircle` writes to `m_circleWaveArray`. Currently no suppression.
- `pauseGame` (14845): if sim-driven → real game pauses.
- `playReplay` (14848): not a sim path but conceptually dangerous.
- `setDamageVerifiedIdx` (14866): anti-cheat state; sim should not touch.

#### Unsnapshotted layer fields with leak potential if any of the above unhooked methods fires from sim
- `m_currentCheckpoint`, `m_checkpointArray`, `m_activatedCheckpoint`, `m_tryPlaceCheckpoint` — checkpoint paths.
- `m_keyColors`, `m_keyOpacities`, `m_keyPulses`, `m_blendingColors` — color palette.
- `m_circleWaveArray`, `m_gravityEffects` (and counters), `m_coinArray`, `m_collectibles` — shared visual/pickup arrays.
- `m_jumps`, `m_uncommittedJumps`, `m_hasJumped`, `m_attemptTime`, `m_currentTime`, `m_pauseTime`, `m_pauseDelta`, `m_isPaused`, `m_inResetDelay`, `m_lastAttemptPercent`, `m_orbs`, `m_diamonds`, `m_secretKey`, `m_hasCompletedLevel`, `m_endLayerStars`, `m_bestAttemptTime` — stats/timing.
- `m_endChecked`, `m_endXPosition` (immutable), `m_platformerEndTrigger`, `m_endPosition` (mostly immutable) — end-trigger state.
- `m_platformerRestart`, `m_isIgnoreDamageEnabled`, `m_pulseRodIndex`, `m_glitterEnabled`, `m_bgEffectDisabled`, `m_disableGravityEffect`, `m_musicPrepared`, `m_nextColorKey`, `m_decimalPercentage`, `m_showLeaderboardPercentage` — misc layer flags.
- `m_unk37c0` (CCArray*) — unknown but a shared container.
- `m_doNot` — confirmed-unknown, name suggests engine-internal guard.

## Caveats

- `m_gameState` is GJGameState, but it is **declared on GJBaseGameLayer** (line 7717), not on PlayLayer. The prompt asked to list `m_gameState` on PlayLayer; the bindings disagree. Per the "Don't re-list inherited members" rule, it is intentionally omitted from the fields table — see `03-gjbasegamelayer.md` for the declaration and `04-gjgamestate.md` for the contents. The `LayerStateSnapshot` capture goes through `pl->m_gameState` syntactically (the inherited member is visible from a PlayLayer*), but the field itself is GJBaseGameLayer's.
- `m_player1` / `m_player2` are GJBaseGameLayer fields (line 7876-7877), not PlayLayer's. Same for `m_anticheatSpike` (line 8118), `m_groundLayer` / `m_groundLayer2` / `m_middleground` (lines 7927-7929), and `m_objectLayer` referenced in `createSimPlayer`. All audited in `03-gjbasegamelayer.md`.
- `updateScreenRotation` appears twice in the layer hierarchy: on PlayLayer (line 14897, signature `int rotation`) and on GJBaseGameLayer (different signature `float rot`). The `TrajBaseLayerHook` suppresses the GJBaseGameLayer version. The PlayLayer-specific version (non-virtual `int` overload) is **not** hooked — verify whether the engine ever calls the PlayLayer overload during sim runs; if so it would leak.
- `updateTimeWarp` likewise appears in two forms: the virtual `void updateTimeWarp(float)` at 14794 and the non-virtual `void updateTimeWarp(EffectGameObject*, float)` at 14899. Both ultimately write to `m_gameState.m_timeWarp`, which is covered by the snapshot, so leakage is bounded.
- Several method names look innocuous but classify as state-mutating-game or persistent-stat: `incrementJumps`, `commitJumps`, `queueCheckpoint`, `markCheckpoint`, `storeCheckpoint`, `createCheckpoint`, `updateAttempts`, `updateAttemptTime`, `setDamageVerifiedIdx`, `toggleIgnoreDamage`, `togglePracticeMode`. The bot must not invoke any of these from a sim-driven code path.
- The 99-field count includes the many `m_unkXXXX` placeholders. Several of those (e.g. `m_unk37c0` CCArray*, `m_unk3918` / `m_unk3920` CCPoints, `m_doNot`) may be hidden state-mutation surfaces — every "unknown" field that's a CCArray*, CCDictionary*, or a CCPoint/float that triggers/portals reference is a potential leak vector that the audit cannot definitively classify until decompilation identifies its role.
