#include "Bot.hpp"
#include "BotViz.hpp"

namespace bot {

Bot& Bot::get() {
    static Bot s;
    return s;
}

void Bot::onPlayLayerInit(PlayLayer* pl) {
    m_pl         = pl;
    m_frame      = 0;
    m_injecting  = false;
    m_lastHeld   = false;
    m_inSearch   = false;
    m_levelReady = false;
    m_best       = BestPath{};
    m_candidateTraces.clear();
    BotViz::get().onPlayLayerInit(pl);
}

void Bot::onPlayLayerReset() {
    m_frame     = 0;
    m_injecting = false;
    m_lastHeld  = false;
    m_inSearch  = false;
    m_best      = BestPath{};
    m_candidateTraces.clear();
    BotViz::get().clear();
}

void Bot::onPlayLayerQuit() {
    BotViz::get().onPlayLayerQuit();
    m_pl         = nullptr;
    m_frame      = 0;
    m_injecting  = false;
    m_lastHeld   = false;
    m_inSearch   = false;
    m_levelReady = false;
    m_best       = BestPath{};
    m_candidateTraces.clear();
}

void Bot::setEnabled(bool e) {
    if (m_enabled == e) return;
    m_enabled = e;
    if (!e) BotViz::get().clear();
}

void Bot::setDebugViz(DebugViz v) {
    m_viz = v;
    if (v == DebugViz::Off) BotViz::get().clear();
}

bool Bot::inputForCurrentFrame() const {
    if (!m_enabled) return false;
    if (m_best.empty()) return m_lastHeld;

    auto const& plan = m_best.plan;
    int64_t const startF = m_best.planStart;
    int64_t const F      = m_frame;

    // Walk every transition (including the implicit "start at plan[0]" at
    // index 0). Apply each one's offset depending on the value it switches
    // INTO (hold vs. release). The transition with the latest shifted frame
    // ≤ F wins.
    bool found = false;
    bool state = false;
    int64_t latestShifted = INT64_MIN;
    auto consider = [&](size_t i, bool val) {
        int offset = val ? m_holdOffset : m_releaseOffset;
        int64_t shiftedF = startF + static_cast<int64_t>(i) + offset;
        if (F >= shiftedF && shiftedF >= latestShifted) {
            found = true;
            state = val;
            latestShifted = shiftedF;
        }
    };

    if (!plan.empty()) consider(0, plan[0]);
    for (size_t i = 1; i < plan.size(); ++i) {
        if (plan[i] == plan[i - 1]) continue;
        consider(i, plan[i]);
    }

    return found ? state : m_lastHeld;
}

void Bot::commitBest(BestPath&& nb) {
    m_best = std::move(nb);
}

}
