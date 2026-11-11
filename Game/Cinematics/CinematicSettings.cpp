#include "CinematicSettings.h"
#include "Engine/Resources/JsonAssetReader.h"
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace Game {
	void CinematicShotSettings::Validate() const {
		if (target != "model" && target != "carrier")
			throw std::invalid_argument("target must be model or carrier");
		const auto positive = [](float value, const char* key) {
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument(std::string(key) + " must be positive and finite");
		};
		if (formationOffsets.empty() || formationOffsets.size() > 32)
			throw std::invalid_argument("formationOffsets must contain 1..32 entries");
		const auto finiteVector = [](const DirectX::XMFLOAT3& value, const std::string& field) {
			if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z))
				throw std::invalid_argument(field + " must contain finite components");
		};
		finiteVector(originOffsetMeters, "originOffsetMeters");
		for (size_t i = 0; i < formationOffsets.size(); ++i)
			finiteVector(formationOffsets[i], "formationOffsets/" + std::to_string(i));
		positive(durationSeconds, "durationSeconds");
		positive(modelScale, "modelScale");
		if (model.empty() || model.size() > 192)
			throw std::invalid_argument("model must be a nonempty asset ID");
		if (!std::isfinite(headingDegrees)) throw std::invalid_argument("headingDegrees must be finite");
		positive(formationSpacingMeters, "formationSpacingMeters");
		positive(formationDepthMeters, "formationDepthMeters");
		if (!std::isfinite(flightSpeedMetersPerSecond) || flightSpeedMetersPerSecond < 0 ||
			!std::isfinite(propellerRadiansPerSecond) || propellerRadiansPerSecond < 0)
			throw std::invalid_argument(
				"flightSpeedMetersPerSecond/propellerRadiansPerSecond must be nonnegative and finite");
		positive(verticalFovDegrees, "verticalFovDegrees");
		if (verticalFovDegrees >= 120) throw std::invalid_argument("verticalFovDegrees must be below 120");
		for (const auto offset : {cameraStartOffsetMeters, cameraEndOffsetMeters, lookAtOffsetMeters})
			for (float value : {offset.x, offset.y, offset.z})
				if (!std::isfinite(value)) throw std::invalid_argument("camera offsets must be finite");
		// 補間経路全体で、注視点との重なりと真上からの特異点を防ぐ。
		const double x = double(cameraStartOffsetMeters.x) - lookAtOffsetMeters.x;
		const double z = double(cameraStartOffsetMeters.z) - lookAtOffsetMeters.z;
		const double dx = double(cameraEndOffsetMeters.x) - cameraStartOffsetMeters.x;
		const double dz = double(cameraEndOffsetMeters.z) - cameraStartOffsetMeters.z;
		const double distance = dx * dx + dz * dz;
		const double t = distance > 0 ? std::clamp(-(x * dx + z * dz) / distance, 0.0, 1.0) : 0;
		if ((x + t * dx) * (x + t * dx) + (z + t * dz) * (z + t * dz) < 1e-6)
			throw std::invalid_argument(
				"cameraStartOffsetMeters/cameraEndOffsetMeters cross the lookAtOffsetMeters vertical axis");
		if (title.empty() || subtitle.empty() || title.size() > 192 || subtitle.size() > 192)
			throw std::invalid_argument("title and subtitle must contain 1..192 UTF-8 bytes");
	}
	float CinematicSettings::TotalDuration() const {
		if (shots.empty()) return durationSeconds;
		float total = 0;
		for (const auto& shot : shots)
			total += shot.durationSeconds;
		return total;
	}
	void CinematicSettings::Validate() const {
		CinematicShotSettings::Validate();
		if (shots.size() > 64) throw std::invalid_argument("shots must contain 1..64 entries");
		for (size_t i = 0; i < shots.size(); ++i) {
			try {
				shots[i].Validate();
			} catch (const std::exception& error) {
				throw std::invalid_argument("shots/" + std::to_string(i) + ": " + error.what());
			}
		}
		if (!std::isfinite(TotalDuration()))
			throw std::invalid_argument("shots duration exceeds finite range");
		if (shots.empty() &&
			(!std::isfinite(fadeOutStartSeconds) || !std::isfinite(cutSeconds) ||
				!std::isfinite(fadeInStartSeconds) || !std::isfinite(fadeInSeconds) ||
				!(0 < fadeOutStartSeconds && fadeOutStartSeconds < cutSeconds &&
					cutSeconds <= fadeInStartSeconds && fadeInSeconds > 0 &&
					fadeInStartSeconds + fadeInSeconds <= durationSeconds)))
			throw std::invalid_argument("fadeOutStartSeconds < cutSeconds <= fadeInStartSeconds; "
										"fadeInStartSeconds + fadeInSeconds <= durationSeconds required");
		if (radio.size() > 64) throw std::invalid_argument("radio must contain at most 64 entries");
		float previousEnd = 0;
		for (const auto& entry : radio) {
			if (entry.caption.speaker.empty() || entry.caption.speaker.size() > 192 ||
				entry.caption.text.empty() || entry.caption.text.size() > 384 ||
				(entry.caption.side != "friendly" && entry.caption.side != "enemy"))
				throw std::invalid_argument("radio requires speaker, text and friendly/enemy side");
			const float end = entry.startSeconds + entry.durationSeconds;
			if (!std::isfinite(entry.startSeconds) || !std::isfinite(entry.durationSeconds) ||
				!std::isfinite(end) || entry.startSeconds < previousEnd || entry.durationSeconds <= 0)
				throw std::invalid_argument("radio intervals must be ordered and non-overlapping");
			previousEnd = end;
		}
		if (briefing.size() > 64) throw std::invalid_argument("briefing must contain at most 64 entries");
		for (size_t i = 0; i < briefing.size(); ++i) {
			const auto prefix = "briefing/" + std::to_string(i);
			for (const auto& line : briefing[i].lines)
				if (line.empty() || line.size() > 192)
					throw std::invalid_argument(prefix + "/lines must contain 1..192 UTF-8 bytes per line");
			if (!std::isfinite(briefing[i].fallbackSeconds) || briefing[i].fallbackSeconds <= 0)
				throw std::invalid_argument(prefix + "/fallbackSeconds must be positive and finite");
		}
		for (const auto& [field, value] : {std::pair{"audio/bgmVolume", audio.bgmVolume},
				 std::pair{"audio/briefingVolume", audio.briefingVolume},
				 std::pair{"audio/flightVolumeScale", audio.flightVolumeScale}})
			if (!std::isfinite(value) || value < 0 || value > 1)
				throw std::invalid_argument(std::string(field) + " must be within [0,1]");
	}

	CinematicSettings LoadCinematicSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		CinematicSettings result;
		try {
			// 省略時の既定音声も設定ファイルの所在を基準に解決する。
			result.audio.bgm = path.parent_path() / result.audio.bgm;
			for (auto& entry : result.briefing)
				entry.audio = path.parent_path() / entry.audio;
			if (!std::filesystem::exists(path)) return result;
			result.text.Load(reader, path);
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
					CinematicBriefingEntry entry;
					entry.lines = {reader.ResolveString(path, key + "/lines/0"),
						reader.ResolveString(path, key + "/lines/1")};
					const auto audio = reader.ResolveString(path, key + "/audio");
					entry.audio =
						audio.empty() ? std::filesystem::path{} : reader.Resolve(path, key + "/audio");
					entry.fallbackSeconds = reader.ResolveFloat(path, key + "/fallbackSeconds");
					result.briefing.push_back(std::move(entry));
				}
			}
			if (auto count = reader.TryResolveArrayLength(path, "radio")) {
				if (*count > 64) throw std::invalid_argument("radio must contain at most 64 entries");
				for (size_t i = 0; i < *count; ++i) {
					const auto key = "/radio/" + std::to_string(i) + "/";
					CinematicRadioEntry entry;
					entry.caption.speaker = reader.ResolveString(path, key + "speaker");
					entry.caption.text = reader.ResolveString(path, key + "text");
					if (auto side = reader.TryResolveString(path, key + "side")) entry.caption.side = *side;
					if (auto audio = reader.TryResolveString(path, key + "audio"); audio && !audio->empty())
						entry.audio = reader.Resolve(path, key + "audio");
					number((key + "startSeconds").c_str(), entry.startSeconds);
					number((key + "durationSeconds").c_str(), entry.durationSeconds);
					result.radio.push_back(std::move(entry));
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
			if (auto value = reader.TryResolveString(path, "target")) result.target = *value;
			if (auto value = reader.TryResolveString(path, "model")) result.model = *value;
			number("modelScale", result.modelScale);
			number("headingDegrees", result.headingDegrees);
			if (auto count = reader.TryResolveArrayLength(path, "shots")) {
				if (*count < 1 || *count > 64)
					throw std::invalid_argument("shots must contain 1..64 entries");
				for (size_t i = 0; i < *count; ++i) {
					const auto prefix = "/shots/" + std::to_string(i) + "/";
					CinematicShotSettings shot = result;
					const auto num = [&](const char* name, float& value) {
						number((prefix + name).c_str(), value);
					};
					const auto vec = [&](const char* name, DirectX::XMFLOAT3& value) {
						vector((prefix + name).c_str(), value);
					};
					const auto str = [&](const char* name, std::string& value) {
						if (auto text = reader.TryResolveString(path, prefix + name)) value = *text;
					};
					str("target", shot.target);
					str("model", shot.model);
					str("title", shot.title);
					str("subtitle", shot.subtitle);
					num("durationSeconds", shot.durationSeconds);
					num("modelScale", shot.modelScale);
					num("headingDegrees", shot.headingDegrees);
					num("formationSpacingMeters", shot.formationSpacingMeters);
					num("formationDepthMeters", shot.formationDepthMeters);
					num("flightSpeedMetersPerSecond", shot.flightSpeedMetersPerSecond);
					num("propellerRadiansPerSecond", shot.propellerRadiansPerSecond);
					num("verticalFovDegrees", shot.verticalFovDegrees);
					vec("originOffsetMeters", shot.originOffsetMeters);
					vec("cameraStartOffsetMeters", shot.cameraStartOffsetMeters);
					vec("cameraEndOffsetMeters", shot.cameraEndOffsetMeters);
					vec("lookAtOffsetMeters", shot.lookAtOffsetMeters);
					if (auto formations = reader.TryResolveArrayLength(path, prefix + "formationOffsets")) {
						if (*formations < 1 || *formations > 32)
							throw std::invalid_argument(
								prefix + "formationOffsets must contain 1..32 entries");
						shot.formationOffsets.resize(*formations);
						for (size_t j = 0; j < *formations; ++j) {
							const auto key = prefix + "formationOffsets/" + std::to_string(j);
							const auto values = reader.ResolveFloatArray(path, key);
							if (values.size() != 3)
								throw std::invalid_argument(key + " must contain 3 numbers");
							shot.formationOffsets[j] = {values[0], values[1], values[2]};
						}
					}
					result.shots.push_back(std::move(shot));
				}
			}
			result.Validate();
			return result;
		} catch (const std::exception& error) {
			throw std::runtime_error(path.string() + ": " + error.what());
		}
	}
} // namespace Game
