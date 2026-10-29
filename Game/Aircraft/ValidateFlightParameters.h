#pragma once
#include "AircraftFlightParameters.h"
#include <cmath>
#include <stdexcept>
#include <string>
#include <numbers>

namespace Game {
	inline void ValidateFlightParameters(const AircraftFlightParameters& p) {
		const auto finiteNonnegative = [](float value, const char* name) {
			if (!std::isfinite(value) || value < 0.0f)
				throw std::invalid_argument(std::string("Invalid flight parameter: ") + name);
		};
		finiteNonnegative(p.initialSpeed, "initialSpeed");
		finiteNonnegative(p.minimumSpeed, "minimumSpeed");
		finiteNonnegative(p.maximumSpeed, "maximumSpeed");
		finiteNonnegative(p.acceleration, "acceleration");
		finiteNonnegative(p.drag, "drag");
		finiteNonnegative(p.maximumBankRadians, "maximumBankRadians");
		finiteNonnegative(p.pitchRateRadians, "pitchRateRadians");
		finiteNonnegative(p.turnRateRadians, "turnRateRadians");
		finiteNonnegative(p.yawRateRadians, "yawRateRadians");
		finiteNonnegative(p.rollLevelRate, "rollLevelRate");
		finiteNonnegative(p.controlResponse, "controlResponse");
		finiteNonnegative(p.rollInputResponse, "rollInputResponse");
		finiteNonnegative(p.minimumTurnFactor, "minimumTurnFactor");
		finiteNonnegative(p.maximumTurnFactor, "maximumTurnFactor");
		finiteNonnegative(p.highGTurnMultiplier, "highGTurnMultiplier");
		finiteNonnegative(p.highGDrag, "highGDrag");
		finiteNonnegative(p.gravity, "gravity");
		finiteNonnegative(p.stallSpeed, "stallSpeed");
		finiteNonnegative(p.liftReferenceSpeed, "liftReferenceSpeed");
		finiteNonnegative(p.maximumLiftFactor, "maximumLiftFactor");
		finiteNonnegative(p.stallAngleRadians, "stallAngleRadians");
		finiteNonnegative(p.criticalAngleRadians, "criticalAngleRadians");
		finiteNonnegative(p.turnDrag, "turnDrag");
		finiteNonnegative(p.flightPathResponse, "flightPathResponse");
		// 離陸用にminimumSpeed未満の初速を許し、maximumSpeed超過は拒否する
		if (p.minimumSpeed <= 0.0f || p.maximumSpeed <= p.minimumSpeed || p.initialSpeed > p.maximumSpeed ||
			p.liftReferenceSpeed <= 0.0f || p.stallSpeed > p.maximumSpeed ||
			p.criticalAngleRadians <= p.stallAngleRadians ||
			p.criticalAngleRadians > std::numbers::pi_v<float> ||
			p.maximumBankRadians > std::numbers::pi_v<float> || p.maximumTurnFactor < p.minimumTurnFactor ||
			p.minimumTurnFactor <= 0.0f || p.highGTurnMultiplier < 1.0f || p.maximumLiftFactor <= 0.0f) {
			throw std::invalid_argument("Flight parameter ranges are inconsistent.");
		}
	}
} // namespace Game
