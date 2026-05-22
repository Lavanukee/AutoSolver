// FROZEN: portal logic is correct. Do not modify.
// Hooks here protect the real player from sim-driven portal side effects.
// Portal physics on the sim still runs — these only block global mutations
// (dual mode toggle, camera limits) that would leak from sim into the real game.

#include "Trajectory.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace {
inline traj::TrajectorySimulator& sim() { return traj::TrajectorySimulator::get(); }
}

class $modify(TrajPortalsLayerHook, GJBaseGameLayer) {
    void toggleDualMode(GameObject* obj, bool dual, PlayerObject* player, bool noEffects) {
        if (sim().isSimPlayer(player)) return;
        GJBaseGameLayer::toggleDualMode(obj, dual, player, noEffects);
    }

    void checkCameraLimitAfterTeleport(PlayerObject* player, float yOffset) {
        if (sim().isSimPlayer(player)) return;
        GJBaseGameLayer::checkCameraLimitAfterTeleport(player, yOffset);
    }
};
