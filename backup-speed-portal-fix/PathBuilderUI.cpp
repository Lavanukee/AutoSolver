#include "PathBuilder.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/CCMenuItemToggler.hpp>
#include <Geode/binding/ButtonSprite.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include <Geode/ui/ColorPickPopup.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>

using namespace geode::prelude;

namespace bot {

namespace {

constexpr float POPUP_W = 480.f;
constexpr float POPUP_H = 320.f;

// Inner card geometry.
constexpr float CONTENT_W      = 450.f;
constexpr float SCROLL_H       = 222.f;
constexpr float PATH_HEADER_H  = 28.f;
constexpr float GAMEMODE_H     = 22.f;
constexpr float STEP_H         = 26.f;
constexpr float ADD_STEP_H     = 24.f;
constexpr float CARD_PAD       = 6.f;
constexpr float CARD_GAP       = 6.f;

// All eight gamemode bits in display order.
constexpr uint8_t kGamemodeBits[8] = {
    GM_Cube, GM_Ship, GM_Ball, GM_Ufo, GM_Wave, GM_Robot, GM_Spider, GM_Swing
};
constexpr char const* kGamemodeShort[8] = {
    "Cube", "Ship", "Ball", "UFO", "Wave", "Robo", "Spid", "Swng"
};

char const* kindLabel(PathStep::Kind k) {
    switch (k) {
        case PathStep::Kind::Wait: return "Wait";
        case PathStep::Kind::Hold: return "Hold";
        case PathStep::Kind::Alt:  return "Alt";
    }
    return "?";
}

PathStep::Kind cycleKind(PathStep::Kind k) {
    switch (k) {
        case PathStep::Kind::Wait: return PathStep::Kind::Hold;
        case PathStep::Kind::Hold: return PathStep::Kind::Alt;
        case PathStep::Kind::Alt:  return PathStep::Kind::Wait;
    }
    return PathStep::Kind::Wait;
}

int parseIntOr(std::string const& s, int fallback) {
    if (s.empty()) return fallback;
    try { return std::stoi(s); } catch (...) { return fallback; }
}

class PathBuilderPopup : public Popup {
protected:
    ScrollLayer* m_scroll = nullptr;

    bool init() override {
        if (!Popup::init(POPUP_W, POPUP_H, "GJ_square01.png")) return false;
        this->setTitle("Path Builder");
        this->setID("path-builder-popup"_spr);

        // Scroll area for the path cards.
        auto scrollSize = CCSize{CONTENT_W, SCROLL_H};
        m_scroll = ScrollLayer::create(scrollSize);
        m_scroll->setAnchorPoint({0.f, 0.f});
        m_scroll->ignoreAnchorPointForPosition(false);
        m_scroll->setPosition({(POPUP_W - CONTENT_W) / 2.f, 36.f});
        m_mainLayer->addChild(m_scroll);

        // Bottom-row "Add Path" button.
        auto addPathSpr = ButtonSprite::create("+ Add Path", "bigFont.fnt", "GJ_button_01.png", 0.6f);
        addPathSpr->setScale(0.55f);
        auto* addPathBtn = CCMenuItemExt::createSpriteExtra(
            addPathSpr, [this](CCMenuItemSpriteExtra*) { this->onAddPath(); }
        );
        auto* btnMenu = CCMenu::create();
        btnMenu->setPosition(0.f, 0.f);
        btnMenu->addChild(addPathBtn);
        addPathBtn->setPosition(POPUP_W / 2.f, 18.f);
        m_mainLayer->addChild(btnMenu);

        rebuild(/*preserveScroll=*/false);
        return true;
    }

    void onClose(CCObject* sender) override {
        PathBuilder::get().save();
        Popup::onClose(sender);
    }

    // Rebuild the whole content layer. By default preserves the scroll
    // position so editing a button doesn't jerk the user back to the top.
    // Pass false on first build / when the user explicitly wants to reset.
    void rebuild(bool preserveScroll = true) {
        float oldPosY = m_scroll->m_contentLayer->getPositionY();

        m_scroll->m_contentLayer->removeAllChildren();

        auto& paths = PathBuilder::get().paths();

        // Compute total content height first (cards stacked top-down).
        float totalH = CARD_PAD;
        for (auto const& p : paths) {
            totalH += cardHeight(p) + CARD_GAP;
        }
        totalH += CARD_PAD;
        if (totalH < SCROLL_H) totalH = SCROLL_H;

        m_scroll->m_contentLayer->setContentSize({CONTENT_W, totalH});

        // Stack cards from the top of the content layer down.
        float y = totalH - CARD_PAD;
        for (size_t i = 0; i < paths.size(); ++i) {
            float h = cardHeight(paths[i]);
            y -= h;
            auto* card = makeCard(static_cast<int>(i));
            card->setPosition({0.f, y});
            m_scroll->m_contentLayer->addChild(card);
            y -= CARD_GAP;
        }

        if (preserveScroll) {
            // Clamp the prior position to the new valid range. Valid posY is
            // [scrollH - contentH, 0] when content is taller than the
            // viewport; otherwise top-align.
            float minPos = SCROLL_H - totalH;
            if (minPos > 0.f) minPos = 0.f;
            float newPos = std::clamp(oldPosY, minPos, 0.f);
            m_scroll->m_contentLayer->setPositionY(newPos);
        } else {
            m_scroll->scrollToTop();
        }
    }

    static float cardHeight(TestPath const& p) {
        return PATH_HEADER_H
             + GAMEMODE_H
             + STEP_H * static_cast<float>(p.steps.size())
             + ADD_STEP_H
             + 4.f;
    }

    CCNode* makeCard(int pathIdx) {
        auto& paths = PathBuilder::get().paths();
        auto& p = paths[pathIdx];

        float h = cardHeight(p);
        auto* card = CCNode::create();
        card->setContentSize({CONTENT_W, h});
        card->setAnchorPoint({0.f, 0.f});

        // Card background.
        auto* bg = extension::CCScale9Sprite::create("square02b_001.png");
        bg->setColor({30, 30, 50});
        bg->setOpacity(160);
        bg->setContentSize({CONTENT_W, h});
        bg->setAnchorPoint({0.f, 0.f});
        bg->setPosition({0.f, 0.f});
        card->addChild(bg);

        // Header: [enabled toggle] [name input] [color swatch] [× delete path]
        auto* headerMenu = CCMenu::create();
        headerMenu->setPosition(0.f, 0.f);
        card->addChild(headerMenu);

        float headerY = h - PATH_HEADER_H / 2.f - 2.f;

        auto* enableToggle = CCMenuItemExt::createTogglerWithStandardSprites(
            0.5f,
            [pathIdx](CCMenuItemToggler* t) {
                auto& ps = PathBuilder::get().paths();
                if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
                ps[pathIdx].enabled = !t->isToggled();
                PathBuilder::get().save();
            }
        );
        enableToggle->toggle(p.enabled);
        enableToggle->setPosition(15.f, headerY);
        headerMenu->addChild(enableToggle);

        auto* nameInput = TextInput::create(200.f, "Path Name", "bigFont.fnt");
        nameInput->setMaxCharCount(24);
        // Filter to safe characters: alphanumerics, space, dash, underscore.
        // (Excludes our serializer delimiters | , : \n.)
        nameInput->setFilter("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -_");
        nameInput->setString(p.name);
        nameInput->setCallback([pathIdx](std::string const& s) {
            auto& ps = PathBuilder::get().paths();
            if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
            ps[pathIdx].name = s.empty() ? "Path" : s;
            PathBuilder::get().save();
        });
        nameInput->setPosition({140.f, headerY});
        nameInput->setScale(0.85f);
        card->addChild(nameInput);

        // Color swatch — small tinted square that opens ColorPickPopup.
        auto* swatch = CCSprite::create("square02_001.png");
        if (swatch) {
            swatch->setColor({p.colorR, p.colorG, p.colorB});
            swatch->setScaleX(20.f / swatch->getContentSize().width);
            swatch->setScaleY(20.f / swatch->getContentSize().height);
            auto* swatchBtn = CCMenuItemExt::createSpriteExtra(
                swatch,
                [this, pathIdx](CCMenuItemSpriteExtra*) { this->onOpenColorPicker(pathIdx); }
            );
            swatchBtn->setPosition(CONTENT_W - 50.f, headerY);
            headerMenu->addChild(swatchBtn);
        }

        auto* delPathBtn = CCMenuItemExt::createSpriteExtraWithFrameName(
            "GJ_deleteIcon_001.png", 0.55f,
            [this, pathIdx](CCMenuItemSpriteExtra*) { this->onDeletePath(pathIdx); }
        );
        delPathBtn->setPosition(CONTENT_W - 18.f, headerY);
        headerMenu->addChild(delPathBtn);

        // Gamemode toggle row (8 small buttons).
        float gmY = h - PATH_HEADER_H - GAMEMODE_H / 2.f;
        auto* gmMenu = CCMenu::create();
        gmMenu->setPosition(0.f, 0.f);
        card->addChild(gmMenu);

        // 8 buttons spread across CONTENT_W minus left/right margins.
        float gmStartX = 18.f;
        float gmEndX   = CONTENT_W - 18.f;
        float gmStep   = (gmEndX - gmStartX) / 7.f;
        for (int i = 0; i < 8; ++i) {
            uint8_t bit  = kGamemodeBits[i];
            bool    on   = (p.gamemodeMask & bit) != 0;
            char const* spriteFile = on ? "GJ_button_01.png" : "GJ_button_05.png";
            auto* spr = ButtonSprite::create(kGamemodeShort[i], "bigFont.fnt", spriteFile, 0.55f);
            spr->setScale(0.42f);
            auto* btn = CCMenuItemExt::createSpriteExtra(
                spr,
                [this, pathIdx, bit](CCMenuItemSpriteExtra*) {
                    auto& ps = PathBuilder::get().paths();
                    if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
                    ps[pathIdx].gamemodeMask ^= bit;
                    PathBuilder::get().save();
                    this->rebuild();
                }
            );
            btn->setPosition(gmStartX + gmStep * i, gmY);
            gmMenu->addChild(btn);
        }

        // Steps stacked under the gamemode row.
        float stepBaseY = h - PATH_HEADER_H - GAMEMODE_H;
        for (size_t s = 0; s < p.steps.size(); ++s) {
            float sy = stepBaseY - STEP_H * (s + 1) + STEP_H / 2.f;
            buildStepRow(card, pathIdx, static_cast<int>(s), sy);
        }

        // "+ Add step" button at the bottom of the card.
        auto* addStepMenu = CCMenu::create();
        addStepMenu->setPosition(0.f, 0.f);
        card->addChild(addStepMenu);
        auto* addStepSpr = ButtonSprite::create(
            "+ Step", "bigFont.fnt", "GJ_button_05.png", 0.6f
        );
        addStepSpr->setScale(0.45f);
        auto* addStepBtn = CCMenuItemExt::createSpriteExtra(
            addStepSpr,
            [this, pathIdx](CCMenuItemSpriteExtra*) { this->onAddStep(pathIdx); }
        );
        addStepBtn->setPosition(40.f, ADD_STEP_H / 2.f + 2.f);
        addStepMenu->addChild(addStepBtn);

        return card;
    }

    void buildStepRow(CCNode* card, int pathIdx, int stepIdx, float y) {
        auto& paths = PathBuilder::get().paths();
        auto& step = paths[pathIdx].steps[stepIdx];

        auto* menu = CCMenu::create();
        menu->setPosition(0.f, 0.f);
        card->addChild(menu);

        // Type cycler — clicking cycles Wait → Hold → Alt → Wait.
        auto* typeSpr = ButtonSprite::create(
            kindLabel(step.kind), "bigFont.fnt", "GJ_button_04.png", 0.6f
        );
        typeSpr->setScale(0.5f);
        auto* typeBtn = CCMenuItemExt::createSpriteExtra(
            typeSpr,
            [this, pathIdx, stepIdx](CCMenuItemSpriteExtra*) {
                auto& ps = PathBuilder::get().paths();
                if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
                if (stepIdx < 0 || stepIdx >= (int)ps[pathIdx].steps.size()) return;
                auto& st = ps[pathIdx].steps[stepIdx];
                st.kind = cycleKind(st.kind);
                PathBuilder::get().save();
                this->rebuild();
            }
        );
        typeBtn->setPosition(45.f, y);
        menu->addChild(typeBtn);

        // Inputs depend on kind.
        if (step.kind == PathStep::Kind::Alt) {
            float startX = 95.f;
            auto* labelHold = CCLabelBMFont::create("H", "bigFont.fnt");
            labelHold->setScale(0.4f);
            labelHold->setPosition(startX, y);
            card->addChild(labelHold);
            auto* holdInput = TextInput::create(48.f, "10", "bigFont.fnt");
            holdInput->setCommonFilter(CommonFilter::Uint);
            holdInput->setMaxCharCount(4);
            holdInput->setString(std::to_string(step.altHold));
            holdInput->setCallback([pathIdx, stepIdx](std::string const& s) {
                auto& ps = PathBuilder::get().paths();
                if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
                if (stepIdx < 0 || stepIdx >= (int)ps[pathIdx].steps.size()) return;
                ps[pathIdx].steps[stepIdx].altHold = std::max(1, parseIntOr(s, 1));
                PathBuilder::get().save();
            });
            holdInput->setPosition({startX + 40.f, y});
            holdInput->setScale(0.85f);
            card->addChild(holdInput);

            auto* labelWait = CCLabelBMFont::create("W", "bigFont.fnt");
            labelWait->setScale(0.4f);
            labelWait->setPosition(startX + 80.f, y);
            card->addChild(labelWait);
            auto* waitInput = TextInput::create(48.f, "10", "bigFont.fnt");
            waitInput->setCommonFilter(CommonFilter::Uint);
            waitInput->setMaxCharCount(4);
            waitInput->setString(std::to_string(step.altWait));
            waitInput->setCallback([pathIdx, stepIdx](std::string const& s) {
                auto& ps = PathBuilder::get().paths();
                if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
                if (stepIdx < 0 || stepIdx >= (int)ps[pathIdx].steps.size()) return;
                ps[pathIdx].steps[stepIdx].altWait = std::max(1, parseIntOr(s, 1));
                PathBuilder::get().save();
            });
            waitInput->setPosition({startX + 120.f, y});
            waitInput->setScale(0.85f);
            card->addChild(waitInput);

            auto* labelCount = CCLabelBMFont::create("x", "bigFont.fnt");
            labelCount->setScale(0.4f);
            labelCount->setPosition(startX + 160.f, y);
            card->addChild(labelCount);
            auto* countInput = TextInput::create(48.f, "5", "bigFont.fnt");
            countInput->setCommonFilter(CommonFilter::Uint);
            countInput->setMaxCharCount(4);
            countInput->setString(std::to_string(step.altCount));
            countInput->setCallback([pathIdx, stepIdx](std::string const& s) {
                auto& ps = PathBuilder::get().paths();
                if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
                if (stepIdx < 0 || stepIdx >= (int)ps[pathIdx].steps.size()) return;
                ps[pathIdx].steps[stepIdx].altCount = std::max(1, parseIntOr(s, 1));
                PathBuilder::get().save();
            });
            countInput->setPosition({startX + 200.f, y});
            countInput->setScale(0.85f);
            card->addChild(countInput);
        } else {
            float startX = 100.f;
            auto* labelN = CCLabelBMFont::create("frames", "bigFont.fnt");
            labelN->setScale(0.4f);
            labelN->setPosition(startX, y);
            card->addChild(labelN);
            auto* nInput = TextInput::create(60.f, "10", "bigFont.fnt");
            nInput->setCommonFilter(CommonFilter::Uint);
            nInput->setMaxCharCount(5);
            nInput->setString(std::to_string(step.frames));
            nInput->setCallback([pathIdx, stepIdx](std::string const& s) {
                auto& ps = PathBuilder::get().paths();
                if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
                if (stepIdx < 0 || stepIdx >= (int)ps[pathIdx].steps.size()) return;
                ps[pathIdx].steps[stepIdx].frames = std::max(1, parseIntOr(s, 1));
                PathBuilder::get().save();
            });
            nInput->setPosition({startX + 60.f, y});
            nInput->setScale(0.85f);
            card->addChild(nInput);
        }

        // Delete-step button on the right.
        auto* delStepBtn = CCMenuItemExt::createSpriteExtraWithFrameName(
            "GJ_deleteIcon_001.png", 0.45f,
            [this, pathIdx, stepIdx](CCMenuItemSpriteExtra*) {
                this->onDeleteStep(pathIdx, stepIdx);
            }
        );
        delStepBtn->setPosition(CONTENT_W - 18.f, y);
        menu->addChild(delStepBtn);
    }

    void onAddPath() {
        auto& paths = PathBuilder::get().paths();
        TestPath p;
        p.name         = "Path " + std::to_string(paths.size() + 1);
        p.enabled      = true;
        p.gamemodeMask = GM_All;
        p.colorR = p.colorG = p.colorB = 160;
        // Seed one Wait step so the user has something to edit.
        p.steps.push_back({PathStep::Kind::Wait, 10, 10, 10, 5});
        paths.push_back(std::move(p));
        PathBuilder::get().save();
        rebuild();
    }

    void onDeletePath(int idx) {
        auto& paths = PathBuilder::get().paths();
        if (idx < 0 || idx >= (int)paths.size()) return;
        paths.erase(paths.begin() + idx);
        PathBuilder::get().save();
        rebuild();
    }

    void onAddStep(int pathIdx) {
        auto& paths = PathBuilder::get().paths();
        if (pathIdx < 0 || pathIdx >= (int)paths.size()) return;
        paths[pathIdx].steps.push_back({PathStep::Kind::Wait, 10, 10, 10, 5});
        PathBuilder::get().save();
        rebuild();
    }

    void onDeleteStep(int pathIdx, int stepIdx) {
        auto& paths = PathBuilder::get().paths();
        if (pathIdx < 0 || pathIdx >= (int)paths.size()) return;
        if (stepIdx < 0 || stepIdx >= (int)paths[pathIdx].steps.size()) return;
        paths[pathIdx].steps.erase(paths[pathIdx].steps.begin() + stepIdx);
        PathBuilder::get().save();
        rebuild();
    }

    void onOpenColorPicker(int pathIdx) {
        auto& paths = PathBuilder::get().paths();
        if (pathIdx < 0 || pathIdx >= (int)paths.size()) return;
        auto& p = paths[pathIdx];

        auto* picker = ColorPickPopup::create(ccColor3B{p.colorR, p.colorG, p.colorB});
        if (!picker) return;
        picker->setCallback([this, pathIdx](ccColor4B const& col) {
            auto& ps = PathBuilder::get().paths();
            if (pathIdx < 0 || pathIdx >= (int)ps.size()) return;
            ps[pathIdx].colorR = col.r;
            ps[pathIdx].colorG = col.g;
            ps[pathIdx].colorB = col.b;
            PathBuilder::get().save();
            this->rebuild();
        });
        picker->show();
    }

public:
    static PathBuilderPopup* create() {
        auto* ret = new PathBuilderPopup();
        if (ret && ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

}

void openPathBuilderPopup() {
    if (auto* p = PathBuilderPopup::create()) p->show();
}

}
