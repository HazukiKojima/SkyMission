#include "OpeningCinematicSettings.h"
#include "Engine/Resources/JsonAssetReader.h"
#include <cmath>
#include <stdexcept>

namespace Game {
	void OpeningCinematicSettings::Validate() const {
		const auto positive = [](float value, const char* key) {
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument(std::string(key) + " must be positive and finite");
		};
		positive(durationSeconds, "durationSeconds");
		positive(fadeOutStartSeconds, "fadeOutStartSeconds");
		positive(cutSeconds, "cutSeconds");
		positive(fadeInStartSeconds, "fadeInStartSeconds");
		positive(fadeInSeconds, "fadeInSeconds");
		if (!(fadeOutStartSeconds < cutSeconds && cutSeconds <= fadeInStartSeconds &&
				fadeInStartSeconds + fadeInSeconds <= durationSeconds))
			throw std::invalid_argument("fadeOutStartSeconds < cutSeconds <= fadeInStartSeconds; "
										"fadeInStartSeconds + fadeInSeconds <= durationSeconds required");
		positive(formationSpacingMeters, "formationSpacingMeters");
		positive(formationDepthMeters, "formationDepthMeters");
		positive(flightSpeedMetersPerSecond, "flightSpeedMetersPerSecond");
		positive(propellerRadiansPerSecond, "propellerRadiansPerSecond");
		positive(verticalFovDegrees, "verticalFovDegrees");
		if (verticalFovDegrees >= 120) throw std::invalid_argument("verticalFovDegrees must be below 120");
		for (const auto offset : {cameraStartOffsetMeters, cameraEndOffsetMeters, lookAtOffsetMeters})
			for (float value : {offset.x, offset.y, offset.z})
				if (!std::isfinite(value)) throw std::invalid_argument("camera offsets must be finite");
		// 注視点との重なりや真上からの視線によるLookAtの特異点を防ぐ
		if (cameraStartOffsetMeters.z <= lookAtOffsetMeters.z + 1 ||
			cameraEndOffsetMeters.z <= lookAtOffsetMeters.z + 1)
			throw std::invalid_argument("cameraStartOffsetMeters.z and cameraEndOffsetMeters.z must be ahead "
										"of lookAtOffsetMeters.z");
		if (title.empty() || subtitle.empty() || title.size() > 192 || subtitle.size() > 192)
			throw std::invalid_argument("title and subtitle must contain 1..192 UTF-8 bytes");
	}

	OpeningCinematicSettings LoadOpeningCinematicSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		OpeningCinematicSettings result;
		if (!std::filesystem::exists(path)) return result;
		try {
			const auto number = [&](const char* key, float& target) {
				if (auto values = reader.TryResolveFloatArray(path, key)) {
					if (values->size() != 1)
						throw std::invalid_argument(std::string(key) + " must be a number");
					target = values->front();
				}
			};
			number("durationSeconds", result.durationSeconds);
			number("fadeOutStartSeconds", result.fadeOutStartSeconds);
			number("cutSeconds", result.cutSeconds);
			number("fadeInStartSeconds", result.fadeInStartSeconds);
			number("fadeInSeconds", result.fadeInSeconds);
			number("formationSpacingMeters", result.formationSpacingMeters);
			number("formationDepthMeters", result.formationDepthMeters);
			number("flightSpeedMetersPerSecond", result.flightSpeedMetersPerSecond);
			number("propellerRadiansPerSecond", result.propellerRadiansPerSecond);
			number("verticalFovDegrees", result.verticalFovDegrees);
			const auto vector = [&](const char* key, DirectX::XMFLOAT3& target) {
				if (auto values = reader.TryResolveFloatArray(path, key)) {
					if (values->size() != 3)
						throw std::invalid_argument(std::string(key) + " must contain 3 numbers");
					target = {(*values)[0], (*values)[1], (*values)[2]};
				}
			};
			vector("cameraStartOffsetMeters", result.cameraStartOffsetMeters);
			vector("cameraEndOffsetMeters", result.cameraEndOffsetMeters);
			vector("lookAtOffsetMeters", result.lookAtOffsetMeters);
			if (auto value = reader.TryResolveString(path, "title")) result.title = *value;
			if (auto value = reader.TryResolveString(path, "subtitle")) result.subtitle = *value;
			result.Validate();
			return result;
		} catch (const std::exception& error) {
			throw std::runtime_error(path.string() + ": " + error.what());
		}
	}
} // namespace Game
