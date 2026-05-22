#pragma once

// Shadow PlayLayer: a second, isolated PlayLayer instance running alongside
// the real one. The goal is to use the engine's OWN code paths to simulate
// candidate plans, eliminating the entire class of state-sync bugs we've
// been hitting (button-hold-duration drift, portal/mode-transition
// divergence, hitbox precision, move-trigger application). The shadow IS
// the engine — same physics, same trigger system, same everything.
//
// Architecture:
//
//   - Constructed once per level via PlayLayer::create(level, false, false).
//     The construction temporarily clobbers GameManager::m_playLayer (which
//     PlayLayer::init does), so we save and restore that pointer around it.
//   - NOT added to any scene tree, so cocos2d's director doesn't render or
//     auto-tick it. We manually call shadow->update(dt) for each sim tick.
//   - Before each manual tick, we swap GameManager::m_playLayer to shadow
//     so engine code (PlayLayer::get callers) sees shadow. After the tick,
//     we swap back to real. Atomic swap pattern — no engine re-entry can
//     observe the wrong pointer in between.
//
// Caveats:
//
//   - Each shadow tick runs the full engine path (~2-10ms vs the old sim's
//     ~500µs). That means runPlan's candidate count budget per visual
//     frame drops correspondingly. Mitigation will come from search-batch
//     amortization (sync once per visual frame, run many plans per sync).
//   - CCActions started on shadow's CCNodes tick from the global director.
//     We rely on the existing TrajBaseLayerHook suppressions (shakeCamera,
//     spawnParticle, animatePortalY, etc.) to keep side effects out of
//     the real scene tree. Those hooks gate on `inShadowTick()` plus the
//     existing isSimulating() — both suppress.

#include <Geode/Geode.hpp>

namespace bot {

class ShadowLayer {
public:
    static ShadowLayer& get();

    // Lazy construction. Called from Bot::runSearch the first time the
    // bot wants to score a plan — by then the real PlayLayer is fully
    // initialized and we're safe to create a sibling. No-op if already
    // alive for this level.
    void ensureCreated(PlayLayer* realPL);

    // Level lifecycle hooks. Called from TrajPlayLayerHook in Hooks.cpp
    // when the real level resets / quits, so we tear down the shadow
    // alongside it.
    void onRealLevelReset();
    void onRealLevelQuit();

    PlayLayer* shadow() const { return m_shadow; }
    bool       alive()  const { return m_shadow != nullptr; }

    // True while we're inside a manual shadow->update tick. Used by the
    // GJBaseGameLayer / PlayLayer hooks to suppress side effects (camera
    // shakes, particle spawns, sound) that would leak from shadow into
    // the real scene tree via shared cocos2d state.
    bool inShadowTick() const { return m_inShadowTick; }

    // True while we're inside PlayLayer::create for the shadow. Hooks
    // use this to skip mod-specific setup that should only happen for
    // the real PlayLayer (e.g. registering it as the sim's playLayer).
    bool inShadowConstruction() const { return m_inShadowConstruction; }

private:
    ShadowLayer() = default;
    ShadowLayer(const ShadowLayer&) = delete;

    PlayLayer* m_shadow{nullptr};
    bool       m_inShadowTick{false};
    bool       m_inShadowConstruction{false};
};

}
