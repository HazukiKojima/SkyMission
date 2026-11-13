#pragma once
#include "../../../Game/Aircraft/AircraftFlightParameters.h"
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<Game::AircraftFlightParameters>);
static_assert(offsetof(Game::AircraftFlightParameters, initialSpeed) == 0 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, minimumSpeed) == 1 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, maximumSpeed) == 2 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, acceleration) == 3 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, drag) == 4 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, maximumBankRadians) == 5 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, pitchRateRadians) == 6 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, turnRateRadians) == 7 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, yawRateRadians) == 8 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, rollLevelRate) == 9 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, controlResponse) == 10 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, rollInputResponse) == 11 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, minimumTurnFactor) == 12 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, maximumTurnFactor) == 13 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, highGTurnMultiplier) == 14 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, highGDrag) == 15 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, gravity) == 16 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, stallSpeed) == 17 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, liftReferenceSpeed) == 18 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, maximumLiftFactor) == 19 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, stallAngleRadians) == 20 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, criticalAngleRadians) == 21 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, turnDrag) == 22 * sizeof(float));
static_assert(offsetof(Game::AircraftFlightParameters, flightPathResponse) == 23 * sizeof(float));
static_assert(sizeof(Game::AircraftFlightParameters) == 24 * sizeof(float));
