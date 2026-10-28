#pragma once
#include "../../../Engine/Aircraft/AircraftAIController.h"
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<Engine::AircraftAIParameters>);
static_assert(offsetof(Engine::AircraftAIParameters, directionChangeSeconds) == 0 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, altitudeChangeSeconds) == 1 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, lowAltitude) == 2 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, highAltitude) == 3 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, pitchLookAheadDistance) == 4 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, turnGain) == 5 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, pitchGain) == 6 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, speedMatchGain) == 7 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, highGManeuverIntervalSeconds) == 8 * sizeof(float));
static_assert(offsetof(Engine::AircraftAIParameters, highGManeuverDurationSeconds) == 9 * sizeof(float));
static_assert(sizeof(Engine::AircraftAIParameters) == 10 * sizeof(float));
