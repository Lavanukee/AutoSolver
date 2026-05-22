#include "BotViz.hpp"
#include "Bot.hpp"

using namespace geode::prelude;

namespace bot {

namespace {
constexpr float DOT_RADIUS    = 4.f;
constexpr float LINE_THICK    = 0.6f;
constexpr float PATH_THICK    = 0.9f;
constexpr float TRACE_THICK   = 0.55f; // candidate-path traces (drawn under best path)
constexpr float LINE_HALF_HEIGHT = 600.f; // tall enough to span any reasonable camera Y range
constexpr cocos2d::ccColor4F COLOR_PATH    = {1.00f, 0.60f, 0.10f, 1.0f}; // orange — followed best path (normal gravity)
constexpr cocos2d::ccColor4F COLOR_PATH_UD = {1.00f, 0.40f, 0.85f, 1.0f}; // pink   — followed best path (gravity-flipped)
constexpr cocos2d::ccColor4F COLOR_HOLD    = {1.00f, 0.60f, 0.10f, 1.0f}; // orange — hold start (matches path)
constexpr cocos2d::ccColor4F COLOR_RELEASE = {0.20f, 0.55f, 1.00f, 1.0f}; // blue   — release start
}

BotViz& BotViz::get() {
    static BotViz s;
    return s;
}

void BotViz::ensureNode() {
    if (m_node || !m_pl) return;
    m_node = CCDrawNode::create();
    m_node->setID("bot-viz-node"_spr);
    m_node->retain();
    auto* dbg = m_pl->m_debugDrawNode;
    if (dbg && dbg->getParent()) {
        dbg->getParent()->addChild(m_node);
        m_node->setZOrder(dbg->getZOrder() + 1);
    } else {
        m_pl->addChild(m_node);
    }
}

void BotViz::onPlayLayerInit(PlayLayer* pl) {
    // Drop any orphaned-but-retained node from a prior level whose onQuit
    // didn't fire (Eclipse mod chain, scene shortcuts, crashes). Without
    // this, ensureNode would skip recreation and we'd reparent into a freed
    // PlayLayer on the first render.
    if (m_node) {
        m_node->removeFromParent();
        m_node->release();
        m_node = nullptr;
    }
    m_pl = pl;
    ensureNode();
}

void BotViz::onPlayLayerQuit() {
    if (m_node) {
        m_node->removeFromParent();
        m_node->release();
        m_node = nullptr;
    }
    m_pl = nullptr;
}

void BotViz::clear() {
    if (m_node) m_node->clear();
}

void BotViz::render() {
    if (!m_pl) return;
    ensureNode();
    if (!m_node) return;
    m_node->clear();

    auto& bot = Bot::get();
    if (!bot.enabled()) return;
    // Master viz gate (toggled via X keybind / "show-bot-trajectory" setting).
    // Best-path overlay AND candidate traces both off when this is false; the
    // bot itself keeps running in the background — only rendering is muted.
    if (!bot.showBotTraj()) return;

    // Per-candidate traces drawn first, in each path's author-chosen color.
    // These are this-frame's predicted trajectories for every enabled,
    // gamemode-matching path — the orange best-path overlay (below) sits
    // on top so the followed plan stays visually dominant.
    for (auto const& tv : bot.candidateTraces()) {
        cocos2d::ccColor4F c {
            tv.color.r / 255.f, tv.color.g / 255.f, tv.color.b / 255.f, 1.f
        };
        auto drawTrace = [&](std::vector<cocos2d::CCPoint> const& s) {
            if (s.size() < 2) return;
            for (size_t i = 1; i < s.size(); ++i) {
                m_node->drawSegment(s[i - 1], s[i], TRACE_THICK, c);
            }
        };
        drawTrace(tv.samples);
        drawTrace(tv.samples2);
    }

    auto const& bp = bot.bestPath();
    if (bp.empty() || bp.samples.empty()) return;

    // Continuous orange/pink line traces the bot's currently-followed best
    // path for each player. Orange in normal-gravity ticks, pink when sim
    // had m_isUpsideDown=true (gravity-flipped — typically ball-mode after
    // snapping to a ceiling). Lets us SEE where sim believes the player
    // has gone upside-down, which on Bloodbath Ball is the diagnostic for
    // the "sim snaps to ceiling that real can't" bug class. Drawn ALWAYS
    // while the bot is enabled, independent of the dot/line debug-viz.
    auto drawPath = [&](std::vector<cocos2d::CCPoint> const& s,
                        std::vector<uint8_t> const& ud) {
        if (s.size() < 2) return;
        for (size_t i = 1; i < s.size(); ++i) {
            // Segment is colored by the gravity state AT ITS END tick
            // (post-tick state of plan[i-1]). Index i indexes into the
            // [0..size()-1] samples array — same dimension as upsideDown.
            bool const flipped = i < ud.size() && ud[i] != 0;
            auto const& c = flipped ? COLOR_PATH_UD : COLOR_PATH;
            m_node->drawSegment(s[i - 1], s[i], PATH_THICK, c);
        }
    };
    drawPath(bp.samples,  bp.samplesUpsideDown);
    drawPath(bp.samples2, bp.samples2UpsideDown);

    if (bot.debugViz() == DebugViz::Off) return;
    if (bp.samples.size() < bp.plan.size() + 1) return;

    bool isLines = bot.debugViz() == DebugViz::Lines;

    auto markAt = [&](cocos2d::CCPoint pos, cocos2d::ccColor4F color) {
        if (isLines) {
            cocos2d::CCPoint top    {pos.x, pos.y + LINE_HALF_HEIGHT};
            cocos2d::CCPoint bottom {pos.x, pos.y - LINE_HALF_HEIGHT};
            m_node->drawSegment(bottom, top, LINE_THICK, color);
        } else {
            m_node->drawDot(pos, DOT_RADIUS, color);
        }
    };

    // Mark every transition: input at frame i differs from input at frame i-1.
    // Color by what the bot is switching INTO at that frame. In dual mode,
    // mark the same transition on both player paths.
    bool haveP2 = bp.samples2.size() >= bp.plan.size() + 1;
    for (size_t i = 1; i < bp.plan.size(); ++i) {
        if (bp.plan[i] == bp.plan[i - 1]) continue;
        cocos2d::ccColor4F color = bp.plan[i] ? COLOR_HOLD : COLOR_RELEASE;
        markAt(bp.samples[i], color);
        if (haveP2) markAt(bp.samples2[i], color);
    }
}

}
