#pragma once

#include "Bot.hpp"

#include <string>
#include <vector>
#include <cstdint>

namespace bot {

// Gamemode bit positions used in TestPath::gamemodeMask. A path is considered
// for the bot's candidate set only when the player's current gamemode bit is
// set. Default mask 0xFF = all enabled.
enum GamemodeBit : uint8_t {
    GM_Cube   = 1u << 0,
    GM_Ship   = 1u << 1,
    GM_Ball   = 1u << 2,
    GM_Ufo    = 1u << 3,
    GM_Wave   = 1u << 4,
    GM_Robot  = 1u << 5,
    GM_Spider = 1u << 6,
    GM_Swing  = 1u << 7,
    GM_All    = 0xFF,
};

// Map an enabled-bit to a human label (e.g. for UI buttons).
char const* gamemodeLabel(uint8_t bit);

// Reads the player's current vehicle flags and returns the corresponding bit.
// Falls back to GM_Cube if the player is null or no flag is set.
uint8_t currentGamemodeBit(PlayerObject* p);

// One step in a user-built test path. Three kinds:
//   Wait : `frames` frames with the button NOT held
//   Hold : `frames` frames WITH the button held
//   Alt  : `altCount` iterations of (Hold altHold, Wait altWait)
struct PathStep {
    enum class Kind : uint8_t { Wait, Hold, Alt };
    Kind     kind     = Kind::Wait;
    int32_t  frames   = 1;     // used by Wait/Hold
    int32_t  altHold  = 10;    // Alt-only
    int32_t  altWait  = 10;    // Alt-only
    int32_t  altCount = 5;     // Alt-only
};

struct TestPath {
    std::string           name;
    bool                  enabled       = true;
    uint8_t               gamemodeMask  = GM_All;
    uint8_t               colorR        = 160;
    uint8_t               colorG        = 160;
    uint8_t               colorB        = 160;
    std::vector<PathStep> steps;
};

// What BotSearch consumes for its per-frame candidate set.
struct Candidate {
    Plan                plan;
    cocos2d::ccColor3B  color { 160, 160, 160 };
    std::string         name;
};

// Singleton. Owns the user's authored paths, persists them to mod saved
// values, and produces candidates for BotSearch.
class PathBuilder {
public:
    static PathBuilder& get();

    // Read/write the persisted "test-paths" saved value. load() seeds
    // defaults if no value has been saved yet.
    void load();
    void save();

    std::vector<TestPath>&       paths()       { return m_paths; }
    std::vector<TestPath> const& paths() const { return m_paths; }

    // Expand a single path's steps into a Plan capped at maxFrames.
    // If the expanded sequence is shorter than maxFrames, pads with the
    // step list's last output bool (so "...Hold:50" keeps holding to the
    // horizon; "...Wait:30" keeps releasing).
    Plan expand(TestPath const& path, int maxFrames) const;

    // Build the bot's candidate set: one Candidate per enabled, non-empty
    // path whose gamemodeMask matches `gamemodeBit`. If nothing matches,
    // falls back to two safe defaults (wait-then-hold + all-release) so
    // the bot still does something even before any paths are authored.
    std::vector<Candidate> generateCandidates(int maxFrames, uint8_t gamemodeBit) const;

private:
    PathBuilder() = default;
    PathBuilder(PathBuilder const&) = delete;
    PathBuilder& operator=(PathBuilder const&) = delete;

    static std::string serialize(std::vector<TestPath> const& paths);
    static std::vector<TestPath> deserialize(std::string const& s);
    static std::vector<TestPath> defaults();

    std::vector<TestPath> m_paths;
};

// Opens the Path Builder popup. Defined in PathBuilderUI.cpp so it can
// pull in cocos UI headers without leaking them into Bot.hpp consumers.
void openPathBuilderPopup();

}
