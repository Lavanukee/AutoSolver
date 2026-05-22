#pragma once

// Lightweight telemetry. Writes structured single-line records to the Geode
// log with a "[TEL]" prefix so the test harness can grep them out.
//
// Format convention: `[TEL] event key=value key2="value with space" ...`
// Keep values primitive; values with spaces use quoted strings. The harness
// parses ad-hoc — no schema enforcement at the mod side.
//
// Lifecycle events fire from hook sites in Hooks.cpp / BotSearch.cpp; the
// helpers below are small wrappers around geode::log::info so the call sites
// stay readable.

#include <Geode/Geode.hpp>

namespace tel {

void levelStart(PlayLayer* pl);
void levelQuit();
void levelReset(int attempt);
void win(float percent);
void death(PlayerObject* player, GameObject* causeObj, float percent);

// Bot search summary — one line per visual frame the bot actually searches.
void botSearch(int64_t frame, int candidateCount, int bestSurvived,
               int winnerIdx, bool diverged);

// Real player position sampled per visual frame (rate-limited inside).
// Lets us reconstruct the actual gameplay trace from telemetry: where the
// real player went, when, on each attempt.
void realPos(PlayerObject* player, float percent);

// Trigger fires — wired in TrajEffectHook in Hooks.cpp. `who` is a short tag
// identifying which player fired the trigger ("sim1"/"sim2"/"real1"/"real2").
void triggerObject(int objectType, char const* who, float playerX, float triggerX);
void triggerActivated(int objectType, char const* who, float xPosition);

}
