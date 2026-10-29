#pragma once
#include "../../../Game/Aircraft/AircraftAIController.h"
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<Game::AircraftAIParameters>);
static_assert(offsetof(Game::AircraftAIParameters, directionChangeSeconds) == 0 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, altitudeChangeSeconds) == 1 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, lowAltitude) == 2 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, highAltitude) == 3 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, pitchLookAheadDistance) == 4 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, turnGain) == 5 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, pitchGain) == 6 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, speedMatchGain) == 7 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, highGManeuverIntervalSeconds) == 8 * sizeof(float));
static_assert(offsetof(Game::AircraftAIParameters, highGManeuverDurationSeconds) == 9 * sizeof(float));
static_assert(sizeof(Game::AircraftAIParameters) == 10 * sizeof(float));
