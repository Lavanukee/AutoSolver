#include "Telemetry.hpp"
#include "Trajectory.hpp"

using namespace geode::prelude;

namespace tel {

namespace {

// Rate-limit budget for the per-frame search log so the file doesn't blow up
// if the bot is searching every frame. Currently logs every Nth call.
constexpr int kBotSearchLogStride = 6;
int g_botSearchCounter = 0;

// Dedup latch for death telemetry. The engine calls destroyPlayer on every
// physics tick during the death animation (the player keeps "dying" until the
// level resets), which produces ~10-15 destroyPlayer fires per actual death
// — observed empirically: 14 death events at percent 0.00→0.06 within the
// same second, monotonically increasing x, all from one in-game death.
// Latch: log on the FIRST destroyPlayer after a reset/start, then suppress
// until the next reset clears the latch.
bool g_deathLatched = false;

// Cache the most recent real-player sample. Used to log the actual death
// position when the level resets — destroyPlayer in Hooks.cpp fires with
// false positives when anticheat-bypass mods are loaded (Eclipse Menu
// cancels the kill but the engine still calls destroyPlayer with the
// anticheat-spike pointer as cause), so we can't trust destroyPlayer to
// mean "player actually died". The level_reset event IS reliable — it
// fires only after the player has actually died — and the last real_pos
// sample before that reset is where the death happened.
float g_lastRealX = 0.f;
float g_lastRealY = 0.f;
float g_lastRealPercent = 0.f;

// Rate-limit budget for trigger fires. Levels with dense triggers can fire
// thousands per second; one log per fire would tank perf and bury the
// interesting events. Log every Nth fire of each kind, plus a per-stride
// counter so the harness can tell how many fired total.
constexpr int kTriggerLogStride = 30;
int g_triggerObjStride = 0;
int g_triggerActStride = 0;
int g_triggerObjTotal = 0;
int g_triggerActTotal = 0;

}

void levelStart(PlayLayer* pl) {
    if (!pl || !pl->m_level) return;
    auto const name = pl->m_level->m_levelName;
    int const id    = pl->m_level->m_levelID;
    int const attempts = pl->m_level->m_attempts;
    geode::log::info("[TEL] level_start id={} name=\"{}\" attempts={}",
                     id, std::string{name}, attempts);
    g_deathLatched = false;
}

void levelQuit() {
    geode::log::info("[TEL] level_quit");
}

void levelReset(int attempt) {
    // Authoritative death event: when level_reset fires during gameplay,
    // the player actually died at the last known position (cached in
    // g_lastRealX/Y/Percent by realPos). This sidesteps the spurious
    // destroyPlayer fires that anticheat-bypass mods produce — we only
    // trust a reset as a death signal. g_lastRealX > 5 filters out the
    // initial reset that fires at level entry (player at spawn, never
    // moved).
    if (g_lastRealX > 5.f) {
        geode::log::info("[TEL] real_death pos=({:.1f},{:.1f}) percent={:.2f} "
                         "trig_obj_total={} trig_act_total={}",
                         g_lastRealX, g_lastRealY, g_lastRealPercent,
                         g_triggerObjTotal, g_triggerActTotal);
    }
    geode::log::info("[TEL] level_reset attempt={}", attempt);
    g_triggerObjTotal = 0;
    g_triggerActTotal = 0;
    g_deathLatched = false;
    g_lastRealX = 0.f;
    g_lastRealY = 0.f;
    g_lastRealPercent = 0.f;
}

void win(float percent) {
    geode::log::info("[TEL] win percent={:.2f}", percent);
}

void death(PlayerObject* player, GameObject* causeObj, float percent) {
    auto& s = traj::TrajectorySimulator::get();
    if (s.isSimPlayer(player)) return;
    auto* pl = s.playLayer();
    bool const isRealP1 = pl && player == pl->m_player1;
    bool const isRealP2 = pl && player == pl->m_player2;
    if (!isRealP1 && !isRealP2) {
        geode::log::warn("[TEL] death_unknown_player player={} p1={} p2={} simp1={} simp2={}",
                         fmt::ptr(player),
                         fmt::ptr(pl ? pl->m_player1 : nullptr),
                         fmt::ptr(pl ? pl->m_player2 : nullptr),
                         fmt::ptr(s.simP1()), fmt::ptr(s.simP2()));
        return;
    }
    auto pos = player->getPosition();

    if (pos.x < 5.f) return;

    int const causeType = causeObj ? static_cast<int>(causeObj->m_objectType) : -1;
    bool const isAnticheat = pl && causeObj == pl->m_anticheatSpike;

    // Don't let an anticheat-spike fire (intercepted + cancelled by Eclipse
    // Menu's anti-anticheat) latch the dedup. Otherwise the REAL death's
    // destroyPlayer call gets suppressed and we lose the cause-object
    // identification we need to diagnose. Anticheat fires log unconditionally
    // (not dedup-counted) but do not consume the latch.
    if (!isAnticheat) {
        // Dedup: engine fires destroyPlayer every physics tick during the
        // death animation. Latch on the first NON-anticheat fire after a
        // reset/start; later fires in the same death are dropped.
        if (g_deathLatched) return;
        g_deathLatched = true;
    }
    geode::log::info("[TEL] death who={} percent={:.2f} player_pos=({:.1f},{:.1f}) "
                     "cause_obj_type={} anticheat={} trig_obj_total={} trig_act_total={}",
                     isRealP1 ? "real1" : "real2",
                     percent, pos.x, pos.y, causeType, isAnticheat ? 1 : 0,
                     g_triggerObjTotal, g_triggerActTotal);
}

void botSearch(int64_t frame, int candidateCount, int bestSurvived,
               int winnerIdx, bool diverged) {
    if (++g_botSearchCounter < kBotSearchLogStride) return;
    g_botSearchCounter = 0;
    geode::log::info("[TEL] bot_search frame={} candidates={} best_survived={} "
                     "winner_idx={} diverged={}",
                     frame, candidateCount, bestSurvived, winnerIdx, diverged ? 1 : 0);
}

namespace {
// Sample the real player position every Nth visual frame so we get a sparse
// movement trace without blowing up the log. ~6 → about 10 samples/sec at
// 60Hz, plenty for reconstructing the gameplay trajectory.
constexpr int kRealPosStride = 6;
int g_realPosStride = 0;
}

void realPos(PlayerObject* player, float percent) {
    if (!player) return;
    // Always update the cache — used by levelReset to log the actual death
    // position. The rate-limit only applies to the LOG line, not the cache.
    auto pos = player->getPosition();
    g_lastRealX = pos.x;
    g_lastRealY = pos.y;
    g_lastRealPercent = percent;
    if (++g_realPosStride < kRealPosStride) return;
    g_realPosStride = 0;
    geode::log::info("[TEL] real_pos pos=({:.1f},{:.1f}) yvel={:.2f} percent={:.2f}",
                     pos.x, pos.y, player->m_yVelocity, percent);
}

void triggerObject(int objectType, char const* who, float playerX, float triggerX) {
    ++g_triggerObjTotal;
    if (++g_triggerObjStride < kTriggerLogStride) return;
    g_triggerObjStride = 0;
    geode::log::info("[TEL] trigger_obj type={} who={} player_x={:.1f} trigger_x={:.1f} total={}",
                     objectType, who, playerX, triggerX, g_triggerObjTotal);
}

void triggerActivated(int objectType, char const* who, float xPosition) {
    ++g_triggerActTotal;
    if (++g_triggerActStride < kTriggerLogStride) return;
    g_triggerActStride = 0;
    geode::log::info("[TEL] trigger_act type={} who={} x_pos={:.1f} total={}",
                     objectType, who, xPosition, g_triggerActTotal);
}

}
