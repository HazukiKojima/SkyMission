#include "WeaponDefinition.h"

namespace Game {
	void ValidateGunSettings(const GunSettings& settings) {
		for (const auto& [name, value] : {std::pair{"fireInterval", settings.fireInterval},
				 std::pair{"muzzleSpeed", settings.muzzleSpeed}, std::pair{"lifetime", settings.lifetime}}) {
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument(std::string(name) + " must be positive and finite");
		}

		if (settings.maxProjectiles < GunSettings::kMinimumProjectileCapacity)
			throw std::invalid_argument("maxProjectiles must be at least 4");

		if (settings.muzzleNames.size() != GunSettings::kTwinMuzzleCount &&
			settings.muzzleNames.size() != GunSettings::kQuadMuzzleCount)
			throw std::invalid_argument("muzzleNames must contain two or four names");

		std::unordered_set<std::string> names;

		for (const auto& name : settings.muzzleNames)
			if (name.empty() || !names.insert(name).second)
				throw std::invalid_argument("muzzleNames must contain unique nonempty names");
	}
} // namespace Game
