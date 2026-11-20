#include "RumbleSettings.h"
#include "SettingsReader.h"

namespace Game {
	RumbleSettings LoadRumbleSettings(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		const SettingsReader json(reader, path);
		RumbleSettings settings;
		const auto motors = [&](const std::string& key, RumbleMotors& values) {
			json.Number(key + "/lowFrequency", values.lowFrequency, 0, 1);
			json.Number(key + "/highFrequency", values.highFrequency, 0, 1);
		};
		const auto pulse = [&](const std::string& key, RumblePulseSettings& values) {
			motors(key, values.motors);
			json.Number(key + "/durationSeconds", values.durationSeconds, 0);
		};
		const auto response = [&](const std::string& key, RumbleResponseSettings& values) {
			motors(key, values.motors);
			json.Number(key + "/responsePerSecond", values.responsePerSecond);
		};

		pulse("/gun/start", settings.gunStart);
		pulse("/gun/repeat", settings.gunRepeat);
		json.Number("/gun/repeat/intervalSeconds", settings.gunRepeatIntervalSeconds);
		json.Require(settings.gunRepeat.durationSeconds <= settings.gunRepeatIntervalSeconds,
			"/gun/repeat/durationSeconds", "must be at most intervalSeconds");
		pulse("/missileLaunch", settings.missileLaunch);
		pulse("/playerHit", settings.playerHit);
		pulse("/enemyDestroyed", settings.enemyDestroyed);
		pulse("/carrierDestroyed", settings.carrierDestroyed);
		motors("/highG", settings.highG);
		response("/acceleration", settings.acceleration);
		response("/braking", settings.braking);
		return settings;
	}
} // namespace Game
