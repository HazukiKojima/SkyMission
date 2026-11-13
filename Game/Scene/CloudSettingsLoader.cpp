#include "CloudSettingsLoader.h"
#include <cmath>
#include <stdexcept>
namespace Game {
	Engine::CloudSettings LoadCloudSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) return {};
		try {
			const float enabled = reader.ResolveFloat(path, "enabled");
			if (enabled != 0 && enabled != 1) throw std::invalid_argument("enabled must be 0 or 1");
			if (!enabled) return {};
			Engine::CloudSettings settings;
			settings.enabled = true;
			settings.bottom = reader.ResolveFloat(path, "bottom");
			settings.top = reader.ResolveFloat(path, "top");
			settings.coverage = reader.ResolveFloat(path, "coverage");
			settings.density = reader.ResolveFloat(path, "density");
			settings.scale = reader.ResolveFloat(path, "scale");
			settings.maxDistance = reader.ResolveFloat(path, "maxDistance");
			const auto wind = reader.ResolveFloatArray(path, "wind");
			if (wind.size() != 2) throw std::invalid_argument("wind must contain X/Z velocity");
			settings.wind = {wind[0], wind[1]};
			const auto integer = [&](const char* key, unsigned low, unsigned high) {
				float value = reader.ResolveFloat(path, key);
				if (!std::isfinite(value) || value < float(low) || value > float(high) ||
					std::floor(value) != value)
					throw std::invalid_argument(std::string(key) + " must be an integer within range");
				return static_cast<unsigned>(value);
			};
			settings.steps = integer("steps", 16, 64);
			settings.seed = integer("seed", 0, 16777215);
			Engine::ValidateCloudSettings(settings);
			return settings;
		} catch (const std::exception& error) {
			throw std::runtime_error(path.string() + ": " + error.what());
		}
	}
} // namespace Game
