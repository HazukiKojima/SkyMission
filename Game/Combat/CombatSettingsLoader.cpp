#include "CombatSettingsLoader.h"
#include "TargetingSystem.h"
#include <cmath>

namespace Game {
	namespace {
		std::string PathText(const std::filesystem::path& path) {
			const auto utf8 = path.u8string();
			return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
		}
		float ReadPositiveFloat(
			const Engine::JsonAssetReader& reader, const std::filesystem::path& path, const char* field) {
			const float value = reader.ResolveFloat(path, field);
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument(std::string(field) + " must be positive and finite");
			return value;
		}
	} // namespace
	CombatSettings LoadCombatSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		try {
			CombatSettings settings;
			auto& t = settings.targeting;
			t.selectionDistance = ReadPositiveFloat(reader, path, "selectionDistance");
			t.acquireDistance = ReadPositiveFloat(reader, path, "acquireDistance");
			t.acquireAngleDegrees = ReadPositiveFloat(reader, path, "acquireAngleDegrees");
			t.acquireSeconds = ReadPositiveFloat(reader, path, "acquireSeconds");
			t.maintainDistance = ReadPositiveFloat(reader, path, "maintainDistance");
			t.maintainAngleDegrees = ReadPositiveFloat(reader, path, "maintainAngleDegrees");
			t.lossGraceSeconds = ReadPositiveFloat(reader, path, "lossGraceSeconds");
			t.assistAngleDegrees = ReadPositiveFloat(reader, path, "assistAngleDegrees");
			TargetingSystem{}.Configure(t);
			settings.enemyHealth = ReadPositiveFloat(reader, path, "enemyHealth");
			settings.playerHealth = ReadPositiveFloat(reader, path, "playerHealth");
			settings.bulletDamage = ReadPositiveFloat(reader, path, "bulletDamage");
			return settings;
		} catch (const std::exception& error) {
			throw std::runtime_error(PathText(path) + ": " + error.what());
		}
	}
	Engine::HudSettings LoadHudSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		try {
			Engine::HudSettings settings;
			const auto rgb = reader.ResolveFloatArray(path, "color");
			const float alpha = reader.ResolveFloat(path, "alpha");
			if (rgb.size() != 3) throw std::invalid_argument("color must contain three components");
			for (float component : {rgb[0], rgb[1], rgb[2], alpha})
				if (!std::isfinite(component) || component < 0 || component > 1)
					throw std::invalid_argument("color/alpha must be within [0,1]");
			settings.color = {rgb[0], rgb[1], rgb[2], alpha};
			settings.lineWidth = ReadPositiveFloat(reader, path, "lineWidth");
			settings.fontSize = ReadPositiveFloat(reader, path, "fontSize");
			return settings;
		} catch (const std::exception& error) {
			throw std::runtime_error(PathText(path) + ": " + error.what());
		}
	}
} // namespace Game
