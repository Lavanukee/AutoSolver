#include "Orbs.hpp"
#include "Trajectory.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/RingObject.hpp>

using namespace geode::prelude;

namespace {
inline traj::TrajectorySimulator& sim() { return traj::TrajectorySimulator::get(); }
}

class $modify(TrajOrbsLayerHook, GJBaseGameLayer) {
    void playerTouchedRing(PlayerObject* player, RingObject* ring) {
        auto& s = sim();
        if (s.isSimulating() && !s.isSimPlayer(player)) return;
        GJBaseGameLayer::playerTouchedRing(player, ring);
    }
};

class $modify(TrajRingHook, RingObject) {
    void triggerActivated(float xPosition) {
        if (sim().isSimulating()) return;
        RingObject::triggerActivated(xPosition);
    }

    void powerOnObject(int state) {
        if (sim().isSimulating()) return;
        RingObject::powerOnObject(state);
    }

    void spawnCircle() {
        if (sim().isSimulating()) return;
        RingObject::spawnCircle();
    }
};
