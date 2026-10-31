#pragma once
#include "Engine/Renderer/RendererDefinition.h"
#include <array>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace Game {
	struct GunSettings {
		std::array<std::string, 4> muzzleNames;
		float fireInterval = 0.1f;
		float muzzleSpeed = 900.0f;
		float lifetime = 3.0f;
		size_t maxProjectiles = 128;
	};

	inline void ValidateGunSettings(const GunSettings& settings) {
		for (const auto& [name, value] : {std::pair{"fireInterval", settings.fireInterval},
				 std::pair{"muzzleSpeed", settings.muzzleSpeed}, std::pair{"lifetime", settings.lifetime}}) {
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument(std::string(name) + " must be positive and finite");
		}
		if (settings.maxProjectiles < 4)
			throw std::invalid_argument("maxProjectiles must be at least 4");
		std::unordered_set<std::string> names;
		for (const auto& name : settings.muzzleNames)
			if (name.empty() || !names.insert(name).second)
				throw std::invalid_argument("muzzleNames must contain four unique nonempty names");
	}

	struct WeaponDefinition {
		GunSettings gun;
		Engine::ModelAssetDefinition ammunition;
		// Assimpの座標変換後のモデル前方軸
		DirectX::XMFLOAT3 modelForward{0, 1, 0};
		std::filesystem::path sourcePath;
	};
} // namespace Game
