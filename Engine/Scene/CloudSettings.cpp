#include "CloudSettings.h"
#include <cmath>
#include <stdexcept>
namespace Engine {
	void ValidateCloudSettings(const CloudSettings& settings) {
		if (!settings.enabled) return;
		const auto range = [](float x, float low, float high) {
			return std::isfinite(x) && x >= low && x <= high;
		};
		if (!range(settings.bottom, 500, 15000) || !range(settings.top, settings.bottom + 300, 20000) ||
			!range(settings.coverage, 0, 1) || !range(settings.density, 0, 0.02f) ||
			!range(settings.scale, 1000, 20000) || !range(settings.wind.x, -100, 100) ||
			!range(settings.wind.y, -100, 100) || !range(settings.maxDistance, 5000, 80000) ||
			settings.steps < 16 || settings.steps > 64 || settings.seed > 16777215)
			throw std::invalid_argument(
				"Cloud layer, coverage, density, scale, wind, distance, steps or seed is invalid");
	}
} // namespace Engine
