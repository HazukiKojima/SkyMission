#pragma once
#include <filesystem>

namespace Engine {
	class JsonAssetReader;
}

namespace Game {
	struct RumbleMotors {
		float lowFrequency = 0;
		float highFrequency = 0;
	};

	struct RumblePulseSettings {
		RumbleMotors motors;
		float durationSeconds = 0;
	};

	struct RumbleResponseSettings {
		RumbleMotors motors;
		float responsePerSecond = 1;
	};

	struct RumbleSettings {
		RumblePulseSettings gunStart{{0.60f, 0.45f}, 0.14f};
		RumblePulseSettings gunRepeat{{0.002f, 0.0043f}, 0.02f};
		float gunRepeatIntervalSeconds = 0.10f;
		RumblePulseSettings missileLaunch{{0.55f, 0.35f}, 0.19f};
		RumblePulseSettings playerHit{{0.80f, 0.45f}, 0.24f};
		RumblePulseSettings enemyDestroyed{{0.72f, 0.90f}, 0.12f};
		RumblePulseSettings carrierDestroyed{{0.95f, 0.55f}, 0.65f};
		RumbleMotors highG{0.22f, 0.0f};
		RumbleResponseSettings acceleration{{0.16f, 0.0f}, 8.0f};
		RumbleResponseSettings braking{{0.0f, 0.18f}, 16.0f};
	};

	RumbleSettings LoadRumbleSettings(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
} // namespace Game
