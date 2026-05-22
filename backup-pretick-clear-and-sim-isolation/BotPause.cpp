#include "Bot.hpp"
#include "PathBuilder.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/PauseLayer.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

namespace {

void toggleBotEnabled() {
    bool now = !Mod::get()->getSettingValue<bool>("bot-enabled");
    Mod::get()->setSettingValue<bool>("bot-enabled", now);
    bot::Bot::get().setEnabled(now);
}

}

class $modify(BotPauseHook, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        bool enabled = Mod::get()->getSettingValue<bool>("bot-enabled");

        auto* btnSprite = ButtonSprite::create(
            enabled ? "Bot: ON" : "Bot: OFF",
            0, 0,
            "bigFont.fnt",
            enabled ? "GJ_button_01.png" : "GJ_button_06.png",
            30.f,
            0.6f
        );
        btnSprite->setScale(0.6f);

        auto* btn = CCMenuItemSpriteExtra::create(
            btnSprite, this,
            menu_selector(BotPauseHook::onBotToggle)
        );
        btn->setID("bot-toggle"_spr);

        // "Paths" button — opens the Path Builder popup.
        auto* pathsSprite = ButtonSprite::create(
            "Paths", 0, 0, "bigFont.fnt", "GJ_button_02.png", 30.f, 0.6f
        );
        pathsSprite->setScale(0.6f);
        auto* pathsBtn = CCMenuItemSpriteExtra::create(
            pathsSprite, this,
            menu_selector(BotPauseHook::onOpenPaths)
        );
        pathsBtn->setID("bot-paths"_spr);

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto* menu = CCMenu::create();
        menu->addChild(btn);
        menu->addChild(pathsBtn);
        // Bottom-right: Paths above the bot toggle.
        btn->setPosition({0.f, 0.f});
        pathsBtn->setPosition({0.f, 30.f});
        menu->setPosition({winSize.width - 50.f, 30.f});
        menu->setID("bot-toggle-menu"_spr);
        this->addChild(menu);
    }

    void onOpenPaths(CCObject*) {
        bot::openPathBuilderPopup();
    }

    void onBotToggle(CCObject* sender) {
        toggleBotEnabled();
        bool now = Mod::get()->getSettingValue<bool>("bot-enabled");
        if (auto* item = typeinfo_cast<CCMenuItemSpriteExtra*>(sender)) {
            if (auto* sprite = typeinfo_cast<ButtonSprite*>(item->getNormalImage())) {
                sprite->updateBGImage(now ? "GJ_button_01.png" : "GJ_button_06.png");
                for (auto* obj : CCArrayExt<CCNode*>(sprite->getChildren())) {
                    if (auto* l = typeinfo_cast<CCLabelBMFont*>(obj)) {
                        l->setString(now ? "Bot: ON" : "Bot: OFF");
                        break;
                    }
                }
            }
        }
    }
};

// F1 toggles bot in-game. Hard-coded for now; user can swap key easily.
class $modify(BotKeybindHook, PlayLayer) {
    void keyDown(cocos2d::enumKeyCodes key, double timestamp) {
        if (key == cocos2d::enumKeyCodes::KEY_F1) {
            toggleBotEnabled();
            return;
        }
        PlayLayer::keyDown(key, timestamp);
    }
};
