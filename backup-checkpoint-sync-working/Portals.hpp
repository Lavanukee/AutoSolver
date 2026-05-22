#pragma once

// FROZEN: portal logic is correct. Do not modify.
// Portal-specific hook logic lives in Portals.cpp.
// The shared collision filter in Hooks.cpp gates portals on wantsPortals().

#include <Geode/Enums.hpp>

namespace traj {

inline bool isPortal(GameObjectType t) {
    using GO = GameObjectType;
    return t == GO::InverseGravityPortal
        || t == GO::NormalGravityPortal
        || t == GO::ShipPortal
        || t == GO::CubePortal
        || t == GO::InverseMirrorPortal
        || t == GO::NormalMirrorPortal
        || t == GO::BallPortal
        || t == GO::RegularSizePortal
        || t == GO::MiniSizePortal
        || t == GO::UfoPortal
        || t == GO::DualPortal
        || t == GO::SoloPortal
        || t == GO::WavePortal
        || t == GO::RobotPortal
        || t == GO::TeleportPortal
        || t == GO::SpiderPortal
        || t == GO::SwingPortal
        || t == GO::GravityTogglePortal;
}

}
