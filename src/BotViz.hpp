#pragma once

#include <Geode/Geode.hpp>

namespace bot {

class BotViz {
public:
    static BotViz& get();

    void onPlayLayerInit(PlayLayer* pl);
    void onPlayLayerQuit();

    void clear();
    // Re-render markers from Bot::bestPath() according to the current viz mode.
    void render();

private:
    BotViz() = default;
    BotViz(const BotViz&) = delete;
    BotViz& operator=(const BotViz&) = delete;

    void ensureNode();

    PlayLayer*           m_pl   = nullptr;
    cocos2d::CCDrawNode* m_node = nullptr;
};

}
