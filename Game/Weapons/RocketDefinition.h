#pragma once
#include "WeaponDefinition.h"

namespace Game {
	struct RocketSettings {
		std::array<std::string, 2> muzzleNames{"GunMuzzle_L1", "GunMuzzle_R1"};
		float cooldownSeconds = 3;
		float speed = 1200;
		float turnDegreesPerSecond = 30;
		float lifetimeSeconds = 8;
		float damage = 100;
		size_t maxRockets = 16;
	};
	inline void ValidateRocketSettings(const RocketSettings& settings) {
		for (const auto& [name, value] :
			{std::pair{"cooldownSeconds", settings.cooldownSeconds}, std::pair{"speed", settings.speed},
				std::pair{"turnDegreesPerSecond", settings.turnDegreesPerSecond},
				std::pair{"lifetimeSeconds", settings.lifetimeSeconds}, std::pair{"damage", settings.damage}})
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument(std::string(name) + " must be positive and finite");
		if (settings.maxRockets < 1 || settings.maxRockets > 1024)
			throw std::invalid_argument("maxRockets must be between 1 and 1024");
		if (settings.muzzleNames[0].empty() || settings.muzzleNames[1].empty() ||
			settings.muzzleNames[0] == settings.muzzleNames[1])
			throw std::invalid_argument("muzzleNames must contain two unique nonempty names");
	}
	struct RocketDefinition {
		RocketSettings settings;
		Engine::ModelAssetDefinition ammunition;
		DirectX::XMFLOAT3 modelForward{1, 0, 0};
		std::filesystem::path sourcePath;
	};
} // namespace Game
