#pragma once

// Trigger classifier. Triggers are EffectGameObject-derived gameplay objects
// that fire side effects (color, move, toggle, spawn, pulse, alpha, etc.) when
// the player crosses them. The shared collision filter in Hooks.cpp gates them
// on wantsTriggers(); the EffectGameObject hook in Hooks.cpp gates the actual
// firing during sim on the same flag (speed-mod portals stay special-cased and
// fire regardless because sim physics needs the speed change).
//
// Object-type mapping: in GD, the vast majority of triggers report
// GameObjectType::Modifier. A handful of "enter effect" objects (entrance
// animations / screen effects) report EnterEffectObject. We deliberately do
// NOT include GameObjectType::Special — that category bundles text labels,
// shaders, and other non-trigger decorations, so gating it would suppress
// objects that have no bearing on physics.

#include <Geode/Enums.hpp>

namespace traj {

inline bool isTrigger(GameObjectType t) {
    using GO = GameObjectType;
    return t == GO::Modifier
        || t == GO::EnterEffectObject;
}

}
