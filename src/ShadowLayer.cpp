#include "ShadowLayer.hpp"

using namespace geode::prelude;

namespace bot {

ShadowLayer& ShadowLayer::get() {
    static ShadowLayer instance;
    return instance;
}

void ShadowLayer::ensureCreated(PlayLayer* realPL) {
    if (m_shadow || !realPL || !realPL->m_level) return;

    // Save the real PlayLayer pointer BEFORE shadow construction.
    // PlayLayer::init writes GameManager::sharedState()->m_playLayer = self
    // unconditionally, so without this save+restore the engine would see
    // the SHADOW as the active PlayLayer on every subsequent PlayLayer::get()
    // call — which is most of the engine's PlayLayer-aware code paths.
    auto* gm = GameManager::sharedState();
    auto* savedPL = gm ? gm->m_playLayer : nullptr;

    m_inShadowConstruction = true;
    geode::log::info("[shadow] constructing for level id={} name=\"{}\"",
                     realPL->m_level->m_levelID,
                     std::string{realPL->m_level->m_levelName});

    // PlayLayer::create signature (bindings line 14926):
    //   create(GJGameLevel* level, bool useReplay, bool dontCreateObjects)
    //
    // DEBUG: passing dontCreateObjects=true to narrow down whether the
    // bot regression we saw with shadow alive is caused by object
    // instantiation or by something else in PlayLayer init. If bot
    // behavior matches the no-shadow baseline with this flag set,
    // we know the object-instantiation phase is mutating shared
    // state we need to find and protect against.
    m_shadow = PlayLayer::create(realPL->m_level, false, /*dontCreateObjects=*/true);

    if (m_shadow) {
        m_shadow->retain();  // keep it alive — we own it post-removal.
        // PlayLayer::create may add the shadow into the current scene
        // graph (or some other shared parent — auto-attachment happens
        // inside cocos2d node lifecycle). If left attached, shadow's
        // children (especially m_player1 sprite + level objects) render
        // alongside the real scene and visibly leak into the user's
        // gameplay. Detach immediately. The retain above keeps shadow
        // alive after removeFromParent drops the parent's reference.
        if (auto* parent = m_shadow->getParent()) {
            geode::log::info("[shadow] detaching from parent (ptr={})", fmt::ptr(parent));
            m_shadow->removeFromParent();
        }
        // Make sure shadow doesn't tick from CCDirector. setVisible(false)
        // gates rendering even if some other path re-parents it later.
        m_shadow->setVisible(false);
        geode::log::info("[shadow] created, ptr={} (real ptr={})",
                         fmt::ptr(m_shadow), fmt::ptr(realPL));
    } else {
        geode::log::warn("[shadow] PlayLayer::create returned null");
    }

    // Restore m_playLayer to real — atomic swap pattern. Anything that
    // queries PlayLayer::get() after this point gets the real one back.
    if (gm) gm->m_playLayer = savedPL;
    m_inShadowConstruction = false;
}

void ShadowLayer::onRealLevelReset() {
    // For now, don't tear down on reset — the level is the same, so the
    // shadow should still be valid. If reset behavior turns out to cause
    // problems (e.g. shadow's internal state drifts after a few resets),
    // we'll re-create here.
}

void ShadowLayer::onRealLevelQuit() {
    if (!m_shadow) return;
    geode::log::info("[shadow] tearing down on level quit");
    m_shadow->release();
    m_shadow = nullptr;
}

}
