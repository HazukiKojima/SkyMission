#include "CombatSettingsLoader.h"
#include "TargetingSystem.h"
#include <cmath>

namespace Game {
	namespace {
		std::string PathText(const std::filesystem::path& path) {
			const auto utf8 = path.u8string();
			return std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size());
		}
		float Positive(
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
			t.selectionDistance = Positive(reader, path, "selectionDistance");
			t.acquireDistance = Positive(reader, path, "acquireDistance");
			t.acquireAngleDegrees = Positive(reader, path, "acquireAngleDegrees");
			t.acquireSeconds = Positive(reader, path, "acquireSeconds");
			t.maintainDistance = Positive(reader, path, "maintainDistance");
			t.maintainAngleDegrees = Positive(reader, path, "maintainAngleDegrees");
			t.lossGraceSeconds = Positive(reader, path, "lossGraceSeconds");
			t.assistAngleDegrees = Positive(reader, path, "assistAngleDegrees");
			TargetingSystem{}.Configure(t);
			settings.enemyHealth = Positive(reader, path, "enemyHealth");
			settings.playerHealth = Positive(reader, path, "playerHealth");
			settings.enemyBulletDamage = Positive(reader, path, "enemyBulletDamage");
			settings.bulletDamage = Positive(reader, path, "bulletDamage");
			for (size_t i = 0; i < 3; ++i) {
				const auto key = "enemyPosition" + std::to_string(i + 1);
				const auto position = reader.ResolveFloatArray(path, key);
				if (position.size() != 3 || !std::isfinite(position[0]) || !std::isfinite(position[1]) ||
					!std::isfinite(position[2]))
					throw std::invalid_argument(key + " must contain three finite coordinates");
				settings.enemyPositions[i] = {position[0], position[1], position[2]};
			}
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
			if (rgb.size() != 3)
				throw std::invalid_argument("color must contain three components");
			for (float component : {rgb[0], rgb[1], rgb[2], alpha})
				if (!std::isfinite(component) || component < 0 || component > 1)
					throw std::invalid_argument("color/alpha must be within [0,1]");
			settings.color = {rgb[0], rgb[1], rgb[2], alpha};
			settings.lineWidth = Positive(reader, path, "lineWidth");
			settings.fontSize = Positive(reader, path, "fontSize");
			return settings;
		} catch (const std::exception& error) {
			throw std::runtime_error(PathText(path) + ": " + error.what());
		}
	}
} // namespace Game
