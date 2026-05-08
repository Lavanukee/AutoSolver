#include "Trajectory.hpp"
#include "Bot.hpp"
#include "PathBuilder.hpp"

#include <Geode/Geode.hpp>
#include <Geode/loader/SettingV3.hpp>

using namespace geode::prelude;

namespace {

bot::DebugViz parseDebugViz(std::string_view s) {
    if (s == "dots")  return bot::DebugViz::Dots;
    if (s == "lines") return bot::DebugViz::Lines;
    return bot::DebugViz::Off;
}

}

$execute {
    auto& sim = traj::TrajectorySimulator::get();
    auto& bot = bot::Bot::get();
    auto* mod = Mod::get();

    sim.setShowTrajectory(mod->getSettingValue<bool>("show-trajectory"));
    sim.setIterations(static_cast<int>(mod->getSettingValue<int64_t>("trajectory-iterations")));
    sim.setPadsEnabled(mod->getSettingValue<bool>("pads"));
    sim.setOrbsEnabled(mod->getSettingValue<bool>("orbs"));
    sim.setPortalsEnabled(mod->getSettingValue<bool>("portals"));
    bot.setEnabled(mod->getSettingValue<bool>("bot-enabled"));
    bot.setShowBotTraj(mod->getSettingValue<bool>("show-bot-trajectory"));
    bot.setDebugViz(parseDebugViz(mod->getSettingValue<std::string>("debug-visuals")));
    bot.setDivergenceThreshold(static_cast<float>(mod->getSettingValue<double>("bot-divergence-threshold")));
    bot.setSearchInterval(static_cast<int>(mod->getSettingValue<int64_t>("bot-search-interval")));
    bot::PathBuilder::get().load();

    listenForSettingChanges<bool>("show-trajectory", [](bool v) {
        traj::TrajectorySimulator::get().setShowTrajectory(v);
    });
    listenForSettingChanges<int64_t>("trajectory-iterations", [](int64_t v) {
        traj::TrajectorySimulator::get().setIterations(static_cast<int>(v));
    });
    listenForSettingChanges<bool>("pads", [](bool v) {
        traj::TrajectorySimulator::get().setPadsEnabled(v);
    });
    listenForSettingChanges<bool>("orbs", [](bool v) {
        traj::TrajectorySimulator::get().setOrbsEnabled(v);
    });
    listenForSettingChanges<bool>("portals", [](bool v) {
        traj::TrajectorySimulator::get().setPortalsEnabled(v);
    });
    listenForSettingChanges<bool>("bot-enabled", [](bool v) {
        bot::Bot::get().setEnabled(v);
    });
    listenForSettingChanges<bool>("show-bot-trajectory", [](bool v) {
        bot::Bot::get().setShowBotTraj(v);
    });
    listenForSettingChanges<std::string>("debug-visuals", [](std::string v) {
        bot::Bot::get().setDebugViz(parseDebugViz(v));
    });
    listenForSettingChanges<double>("bot-divergence-threshold", [](double v) {
        bot::Bot::get().setDivergenceThreshold(static_cast<float>(v));
    });
    listenForSettingChanges<int64_t>("bot-search-interval", [](int64_t v) {
        bot::Bot::get().setSearchInterval(static_cast<int>(v));
    });
}
