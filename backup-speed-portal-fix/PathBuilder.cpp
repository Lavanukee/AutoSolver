#include "PathBuilder.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <sstream>

using namespace geode::prelude;

namespace bot {

namespace {

constexpr char const* SAVE_KEY = "test-paths";

// Format (one path per line):
//   name|enabled|step,step,step|gamemodeMask|R,G,B
// Steps:
//   W:<frames>
//   H:<frames>
//   A:<altHold>:<altWait>:<altCount>
// Names cannot contain | , : \n  — UI filters input to be safe.
// Older saves (3 fields) deserialize with default mask + grey color.

std::vector<std::string> splitOn(std::string const& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

std::string stepToString(PathStep const& st) {
    switch (st.kind) {
        case PathStep::Kind::Wait: return fmt::format("W:{}", std::max(1, st.frames));
        case PathStep::Kind::Hold: return fmt::format("H:{}", std::max(1, st.frames));
        case PathStep::Kind::Alt:  return fmt::format("A:{}:{}:{}",
            std::max(1, st.altHold), std::max(1, st.altWait), std::max(1, st.altCount));
    }
    return "W:1";
}

bool parseStep(std::string const& s, PathStep& out) {
    if (s.empty()) return false;
    auto parts = splitOn(s, ':');
    if (parts.empty()) return false;
    auto toi = [](std::string const& v) {
        try { return std::stoi(v); } catch (...) { return 1; }
    };
    if (parts[0] == "W" && parts.size() >= 2) {
        out.kind   = PathStep::Kind::Wait;
        out.frames = std::max(1, toi(parts[1]));
        return true;
    }
    if (parts[0] == "H" && parts.size() >= 2) {
        out.kind   = PathStep::Kind::Hold;
        out.frames = std::max(1, toi(parts[1]));
        return true;
    }
    if (parts[0] == "A" && parts.size() >= 4) {
        out.kind     = PathStep::Kind::Alt;
        out.altHold  = std::max(1, toi(parts[1]));
        out.altWait  = std::max(1, toi(parts[2]));
        out.altCount = std::max(1, toi(parts[3]));
        return true;
    }
    return false;
}

int parseIntOrDef(std::string const& s, int fallback) {
    if (s.empty()) return fallback;
    try { return std::stoi(s); } catch (...) { return fallback; }
}

}

char const* gamemodeLabel(uint8_t bit) {
    switch (bit) {
        case GM_Cube:   return "Cube";
        case GM_Ship:   return "Ship";
        case GM_Ball:   return "Ball";
        case GM_Ufo:    return "UFO";
        case GM_Wave:   return "Wave";
        case GM_Robot:  return "Robot";
        case GM_Spider: return "Spider";
        case GM_Swing:  return "Swing";
    }
    return "?";
}

uint8_t currentGamemodeBit(PlayerObject* p) {
    if (!p) return GM_Cube;
    if (p->m_isShip)   return GM_Ship;
    if (p->m_isBall)   return GM_Ball;
    if (p->m_isBird)   return GM_Ufo;
    if (p->m_isDart)   return GM_Wave;
    if (p->m_isRobot)  return GM_Robot;
    if (p->m_isSpider) return GM_Spider;
    if (p->m_isSwing)  return GM_Swing;
    return GM_Cube;
}

PathBuilder& PathBuilder::get() {
    static PathBuilder s;
    return s;
}

void PathBuilder::load() {
    auto* mod = Mod::get();
    std::string raw = mod->getSavedValue<std::string>(SAVE_KEY, "");
    if (raw.empty()) {
        m_paths = defaults();
        save();
        return;
    }
    m_paths = deserialize(raw);
    if (m_paths.empty()) {
        m_paths = defaults();
        save();
        return;
    }
    // Append any newly-added preset paths that the existing save doesn't
    // already have by name. Lets users pick up new presets without losing
    // their custom paths. A user who renames a preset will see a fresh copy
    // appended on next load — they can delete it again.
    std::vector<TestPath> defs = defaults();
    bool changed = false;
    for (auto& d : defs) {
        bool found = false;
        for (auto const& p : m_paths) {
            if (p.name == d.name) { found = true; break; }
        }
        if (!found) {
            m_paths.push_back(std::move(d));
            changed = true;
        }
    }
    if (changed) save();
}

void PathBuilder::save() {
    Mod::get()->setSavedValue<std::string>(SAVE_KEY, serialize(m_paths));
}

std::vector<TestPath> PathBuilder::defaults() {
    std::vector<TestPath> out;

    // Gamemode-specific presets ship disabled by default — running 5+
    // candidates per frame in robot/ship mode caused noticeable lag on
    // dense levels. Users enable individually in the path builder when
    // they want to test that pattern.
    auto holdN = [](char const* name, uint8_t mask, int n) {
        TestPath p;
        p.name         = name;
        p.enabled      = false;
        p.gamemodeMask = mask;
        // Grey by default; user re-colors via the picker.
        p.colorR = p.colorG = p.colorB = 160;
        p.steps.push_back({PathStep::Kind::Hold, n, 10, 10, 5});
        p.steps.push_back({PathStep::Kind::Wait, 1000, 10, 10, 5});
        return p;
    };

    auto altHW = [](char const* name, uint8_t mask, int hold, int wait) {
        TestPath p;
        p.name         = name;
        p.enabled      = false;
        p.gamemodeMask = mask;
        p.colorR = p.colorG = p.colorB = 160;
        p.steps.push_back({PathStep::Kind::Alt, 1, hold, wait, 100});
        return p;
    };

    // Generic fallbacks (work in every gamemode).
    TestPath waitHold;
    waitHold.name         = "Wait then Hold";
    waitHold.enabled      = true;
    waitHold.gamemodeMask = GM_All;
    waitHold.colorR = waitHold.colorG = waitHold.colorB = 160;
    waitHold.steps.push_back({PathStep::Kind::Wait, 1, 10, 10, 5});
    waitHold.steps.push_back({PathStep::Kind::Hold, 1000, 10, 10, 5});
    out.push_back(std::move(waitHold));

    TestPath allRelease;
    allRelease.name         = "All Release";
    allRelease.enabled      = true;
    allRelease.gamemodeMask = GM_All;
    allRelease.colorR = allRelease.colorG = allRelease.colorB = 160;
    allRelease.steps.push_back({PathStep::Kind::Wait, 1000, 10, 10, 5});
    out.push_back(std::move(allRelease));

    // Robot-only "hold N then release" presets.
    out.push_back(holdN("Robot Hold 20", GM_Robot, 20));
    out.push_back(holdN("Robot Hold 30", GM_Robot, 30));
    out.push_back(holdN("Robot Hold 40", GM_Robot, 40));
    out.push_back(holdN("Robot Hold 50", GM_Robot, 50));
    out.push_back(holdN("Robot Hold 80", GM_Robot, 80));

    // Ship-only alternating presets.
    out.push_back(altHW("Ship Alt 20/15", GM_Ship, 20, 15));
    out.push_back(altHW("Ship Alt 15/20", GM_Ship, 15, 20));

    return out;
}

std::string PathBuilder::serialize(std::vector<TestPath> const& paths) {
    std::ostringstream out;
    bool firstLine = true;
    for (auto const& p : paths) {
        if (!firstLine) out << '\n';
        firstLine = false;
        out << p.name << '|' << (p.enabled ? '1' : '0') << '|';
        bool firstStep = true;
        for (auto const& st : p.steps) {
            if (!firstStep) out << ',';
            firstStep = false;
            out << stepToString(st);
        }
        out << '|' << static_cast<int>(p.gamemodeMask)
            << '|' << static_cast<int>(p.colorR)
            << ',' << static_cast<int>(p.colorG)
            << ',' << static_cast<int>(p.colorB);
    }
    return out.str();
}

std::vector<TestPath> PathBuilder::deserialize(std::string const& s) {
    std::vector<TestPath> out;
    auto lines = splitOn(s, '\n');
    for (auto& line : lines) {
        if (line.empty()) continue;
        auto parts = splitOn(line, '|');
        if (parts.size() < 2) continue;
        TestPath p;
        p.name    = parts[0];
        p.enabled = !parts[1].empty() && parts[1][0] == '1';
        if (parts.size() >= 3 && !parts[2].empty()) {
            auto stepStrs = splitOn(parts[2], ',');
            for (auto& ss : stepStrs) {
                PathStep st;
                if (parseStep(ss, st)) p.steps.push_back(st);
            }
        }
        if (parts.size() >= 4 && !parts[3].empty()) {
            int mask = parseIntOrDef(parts[3], static_cast<int>(GM_All));
            p.gamemodeMask = static_cast<uint8_t>(std::clamp(mask, 0, 255));
        }
        if (parts.size() >= 5 && !parts[4].empty()) {
            auto rgb = splitOn(parts[4], ',');
            if (rgb.size() == 3) {
                p.colorR = static_cast<uint8_t>(std::clamp(parseIntOrDef(rgb[0], 160), 0, 255));
                p.colorG = static_cast<uint8_t>(std::clamp(parseIntOrDef(rgb[1], 160), 0, 255));
                p.colorB = static_cast<uint8_t>(std::clamp(parseIntOrDef(rgb[2], 160), 0, 255));
            }
        }
        if (p.name.empty()) p.name = "Path";
        out.push_back(std::move(p));
    }
    return out;
}

Plan PathBuilder::expand(TestPath const& path, int maxFrames) const {
    Plan out;
    if (maxFrames <= 0) return out;
    out.reserve(maxFrames);

    auto push = [&](bool v, int n) {
        for (int i = 0; i < n && (int)out.size() < maxFrames; ++i) {
            out.push_back(v);
        }
    };

    bool lastVal = false;
    for (auto const& st : path.steps) {
        switch (st.kind) {
            case PathStep::Kind::Wait:
                push(false, std::max(1, st.frames));
                lastVal = false;
                break;
            case PathStep::Kind::Hold:
                push(true, std::max(1, st.frames));
                lastVal = true;
                break;
            case PathStep::Kind::Alt: {
                int hold  = std::max(1, st.altHold);
                int wait  = std::max(1, st.altWait);
                int count = std::max(1, st.altCount);
                for (int i = 0; i < count && (int)out.size() < maxFrames; ++i) {
                    push(true,  hold);
                    push(false, wait);
                }
                lastVal = false;
                break;
            }
        }
        if ((int)out.size() >= maxFrames) break;
    }
    // Pad to horizon with the final value so "ends in Hold" keeps holding,
    // "ends in Wait" keeps releasing.
    while ((int)out.size() < maxFrames) out.push_back(lastVal);
    return out;
}

std::vector<Candidate> PathBuilder::generateCandidates(int maxFrames, uint8_t gamemodeBit) const {
    std::vector<Candidate> out;
    for (auto const& p : m_paths) {
        if (!p.enabled || p.steps.empty()) continue;
        if ((p.gamemodeMask & gamemodeBit) == 0) continue;
        Plan plan = expand(p, maxFrames);
        if (plan.empty()) continue;
        Candidate c;
        c.plan  = std::move(plan);
        c.color = cocos2d::ccColor3B{ p.colorR, p.colorG, p.colorB };
        c.name  = p.name;
        out.push_back(std::move(c));
    }
    if (out.empty()) {
        // Safety net: nothing matches the current gamemode. Fall back to
        // two generic plans so the bot has SOMETHING to test.
        Plan waitHold(maxFrames, true);
        if (!waitHold.empty()) waitHold[0] = false;
        Plan allRelease(maxFrames, false);

        Candidate a; a.plan = std::move(waitHold);   a.name = "Wait then Hold (fallback)";
        Candidate b; b.plan = std::move(allRelease); b.name = "All Release (fallback)";
        out.push_back(std::move(a));
        out.push_back(std::move(b));
    }
    return out;
}

}
