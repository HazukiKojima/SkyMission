#pragma once
#include "../../../Engine/Aircraft/AircraftFlightParameters.h"
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<Engine::AircraftFlightParameters>);
static_assert(offsetof(Engine::AircraftFlightParameters, initialSpeed) == 0 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, minimumSpeed) == 1 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, maximumSpeed) == 2 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, acceleration) == 3 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, drag) == 4 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, maximumBankRadians) == 5 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, pitchRateRadians) == 6 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, turnRateRadians) == 7 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, yawRateRadians) == 8 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, rollLevelRate) == 9 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, controlResponse) == 10 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, rollInputResponse) == 11 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, minimumTurnFactor) == 12 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, maximumTurnFactor) == 13 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, highGTurnMultiplier) == 14 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, highGDrag) == 15 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, gravity) == 16 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, stallSpeed) == 17 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, liftReferenceSpeed) == 18 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, maximumLiftFactor) == 19 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, stallAngleRadians) == 20 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, criticalAngleRadians) == 21 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, turnDrag) == 22 * sizeof(float));
static_assert(offsetof(Engine::AircraftFlightParameters, flightPathResponse) == 23 * sizeof(float));
static_assert(sizeof(Engine::AircraftFlightParameters) == 24 * sizeof(float));
