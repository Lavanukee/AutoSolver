#pragma once

// Orb classifier. Orb-specific hooks live in Orbs.cpp.
// The shared collision filter in Hooks.cpp gates orbs on wantsOrbs().

#include <Geode/Enums.hpp>

namespace traj {

inline bool isOrb(GameObjectType t) {
    using GO = GameObjectType;
    return t == GO::YellowJumpRing
        || t == GO::PinkJumpRing
        || t == GO::GravityRing
        || t == GO::GreenRing
        || t == GO::DropRing
        || t == GO::CustomRing
        || t == GO::DashRing
        || t == GO::GravityDashRing
        || t == GO::RedJumpRing
        || t == GO::SpiderOrb
        || t == GO::TeleportOrb;
}

}
