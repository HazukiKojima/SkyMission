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
		if (formationOffsets.empty() || formationOffsets.size() > 32)
			throw std::invalid_argument("formationOffsets must contain 1..32 entries");
		if (briefing.size() > 64) throw std::invalid_argument("briefing must contain at most 64 entries");
		for (size_t i = 0; i < briefing.size(); ++i) {
			const auto prefix = "briefing/" + std::to_string(i);
			for (const auto& line : briefing[i].lines)
				if (line.empty() || line.size() > 192)
					throw std::invalid_argument(prefix + "/lines must contain 1..192 UTF-8 bytes per line");
			positive(briefing[i].fallbackSeconds, (prefix + "/fallbackSeconds").c_str());
		}
		for (const auto& [field, value] : {std::pair{"audio/bgmVolume", audio.bgmVolume},
				 std::pair{"audio/briefingVolume", audio.briefingVolume},
				 std::pair{"audio/flightVolumeScale", audio.flightVolumeScale}})
			if (!std::isfinite(value) || value < 0 || value > 1)
				throw std::invalid_argument(std::string(field) + " must be within [0,1]");
		const auto finiteVector = [](const DirectX::XMFLOAT3& value, const std::string& field) {
			if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
				throw std::invalid_argument(field + " must contain finite components");
		};
		finiteVector(originOffsetMeters, "originOffsetMeters");
		for (size_t i = 0; i < formationOffsets.size(); ++i)
			finiteVector(formationOffsets[i], "formationOffsets/" + std::to_string(i));
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
		try {
			// 省略時の既定音声も設定ファイルの所在を基準に解決する。
			result.audio.bgm = path.parent_path() / result.audio.bgm;
			for (auto& entry : result.briefing)
				entry.audio = path.parent_path() / entry.audio;
			if (!std::filesystem::exists(path)) return result;
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
			vector("originOffsetMeters", result.originOffsetMeters);
			if (auto count = reader.TryResolveArrayLength(path, "formationOffsets")) {
				if (*count < 1 || *count > 32)
					throw std::invalid_argument("formationOffsets must contain 1..32 entries");
				result.formationOffsets.resize(*count);
				for (size_t i = 0; i < *count; ++i) {
					const auto key = "/formationOffsets/" + std::to_string(i);
					const auto values = reader.ResolveFloatArray(path, key);
					if (values.size() != 3) throw std::invalid_argument(key + " must contain 3 numbers");
					result.formationOffsets[i] = {values[0], values[1], values[2]};
				}
			}
			if (auto count = reader.TryResolveArrayLength(path, "briefing")) {
				if (*count > 64) throw std::invalid_argument("briefing must contain at most 64 entries");
				result.briefing.clear();
				for (size_t i = 0; i < *count; ++i) {
					const auto key = "/briefing/" + std::to_string(i);
					if (reader.TryResolveArrayLength(path, key + "/lines") != 2)
						throw std::invalid_argument(key + "/lines must contain exactly 2 strings");
					OpeningBriefingEntry entry;
					entry.lines = {reader.ResolveString(path, key + "/lines/0"),
						reader.ResolveString(path, key + "/lines/1")};
					const auto audio = reader.ResolveString(path, key + "/audio");
					entry.audio =
						audio.empty() ? std::filesystem::path{} : reader.Resolve(path, key + "/audio");
					entry.fallbackSeconds = reader.ResolveFloat(path, key + "/fallbackSeconds");
					result.briefing.push_back(std::move(entry));
				}
			}
			// 先頭から辿るため、audioがオブジェクト以外なら型エラーになる。
			if (auto bgm = reader.TryResolveString(path, "/audio/bgm"))
				result.audio.bgm =
					bgm->empty() ? std::filesystem::path{} : reader.Resolve(path, "/audio/bgm");
			number("/audio/bgmVolume", result.audio.bgmVolume);
			number("/audio/briefingVolume", result.audio.briefingVolume);
			number("/audio/flightVolumeScale", result.audio.flightVolumeScale);
			if (auto value = reader.TryResolveString(path, "title")) result.title = *value;
			if (auto value = reader.TryResolveString(path, "subtitle")) result.subtitle = *value;
			result.Validate();
			return result;
		} catch (const std::exception& error) {
			throw std::runtime_error(path.string() + ": " + error.what());
		}
	}
} // namespace Game
