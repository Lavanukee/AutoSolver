#pragma once

// FROZEN: pad logic is correct. Do not modify.
// Pads have no specific hooks — they work via shared infrastructure:
//   1. The collision filter in Hooks.cpp lets pads through to the sim's collision pass
//      (gated on TrajectorySimulator::wantsPads()).
//   2. The engine's bumpPlayer applies the impulse on the sim clone naturally.
//   3. The TrajEnhancedHook in Hooks.cpp suppresses persistent activation flag mutation.
// If pad behavior regresses, the cause is in shared code, not here.

#include <Geode/Enums.hpp>

namespace traj {

inline bool isPad(GameObjectType t) {
    using GO = GameObjectType;
    return t == GO::YellowJumpPad
        || t == GO::PinkJumpPad
        || t == GO::GravityPad
        || t == GO::RedJumpPad
        || t == GO::SpiderPad;
}

}
