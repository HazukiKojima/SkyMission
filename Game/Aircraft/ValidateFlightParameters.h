#pragma once
#include "AircraftFlightParameters.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include <numbers>

namespace Game {
	void ValidateFlightParameters(const AircraftFlightParameters& p);
} // namespace Game
