#pragma once
#include "SettingsReader.h"
namespace Game {
	struct CameraSettings {
		float fovDegrees = 60.0f;
		float moveSpeed = 100.0f;
		float mouseSensitivity = 0.0025f;
		float lookReturnRate = 6.0f;
		float followDistance = 240.0f;
		float maxFollowDistance = 280.0f;
		float followHeight = 65.0f;
		float followLookHeight = 40.0f;
		float followSmoothing = 1000.0f;
		float followDirectionSmoothing = 2.0f;
		float lookPitchLimit = 1.1f;
		float orbitPitchLimit = 1.15f;
		float pitchFollowScale = 0.4f;
		float pitchFollowLimit = 0.35f;
		float speedDistanceStart = 60.0f;
		float lookAheadMeters = 12.0f;
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("fovDegrees", fovDegrees, 0.000001f, 1000000);
			r.Number("moveSpeed", moveSpeed, 0.000001f, 1000000);
			r.Number("mouseSensitivity", mouseSensitivity, 0.000001f, 1000000);
			r.Number("lookReturnRate", lookReturnRate, 0.000001f, 1000000);
			r.Number("followDistance", followDistance, 0.000001f, 1000000);
			r.Number("maxFollowDistance", maxFollowDistance, 0.000001f, 1000000);
			r.Number("followHeight", followHeight, 0.000001f, 1000000);
			r.Number("followLookHeight", followLookHeight, 0.000001f, 1000000);
			r.Number("followSmoothing", followSmoothing, 0.000001f, 1000000);
			r.Number("followDirectionSmoothing", followDirectionSmoothing, 0.000001f, 1000000);
			r.Number("lookPitchLimit", lookPitchLimit, 0.000001f, 1000000);
			r.Number("orbitPitchLimit", orbitPitchLimit, 0.000001f, 1000000);
			r.Number("pitchFollowScale", pitchFollowScale, 0.000001f, 1000000);
			r.Number("pitchFollowLimit", pitchFollowLimit, 0.000001f, 1000000);
			r.Number("speedDistanceStart", speedDistanceStart, 0, 1000000);
			r.Number("lookAheadMeters", lookAheadMeters, 0.000001f, 1000000);
			r.Require(maxFollowDistance >= followDistance, "CameraSettings", "inconsistent settings");
			r.Require(fovDegrees > 1 && fovDegrees < 120, "CameraSettings", "inconsistent settings");
			r.Require(lookPitchLimit < 1.5f && orbitPitchLimit < 1.5f && pitchFollowLimit < 1.5f,
				"CameraSettings", "inconsistent settings");
		}
	};
	struct EnemyGunTuning {
		float damage = 2.0f;
		float lifetimeSeconds = 3.0f;
		float initialDelaySeconds = 0.35f;
		float initialDelayRangeSeconds = 2.4f;
		float burstSeconds = 0.4f;
		float burstRangeSeconds = 0.45f;
		float pauseSeconds = 1.1f;
		float pauseRangeSeconds = 1.7f;
		float minimumLeadFraction = 0.35f;
		float leadVariation = 0.2f;
		float yawErrorDegrees = 2.5f;
		float pitchErrorDegrees = 1.8f;
		float minimumSpreadDegrees = 3.0f;
		float spreadRangeDegrees = 3.0f;
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("damage", damage, 0.000001f, 1000000);
			r.Number("lifetimeSeconds", lifetimeSeconds, 0.001f, 120);
			r.Number("initialDelaySeconds", initialDelaySeconds, 0.000001f, 1000000);
			r.Number("initialDelayRangeSeconds", initialDelayRangeSeconds, 0, 1000000);
			r.Number("burstSeconds", burstSeconds, 0.000001f, 1000000);
			r.Number("burstRangeSeconds", burstRangeSeconds, 0, 1000000);
			r.Number("pauseSeconds", pauseSeconds, 0.000001f, 1000000);
			r.Number("pauseRangeSeconds", pauseRangeSeconds, 0, 1000000);
			r.Number("minimumLeadFraction", minimumLeadFraction, 0, 1);
			r.Number("leadVariation", leadVariation, 0, 1);
			r.Number("yawErrorDegrees", yawErrorDegrees, 0, 1000000);
			r.Number("pitchErrorDegrees", pitchErrorDegrees, 0, 1000000);
			r.Number("minimumSpreadDegrees", minimumSpreadDegrees, 0, 1000000);
			r.Number("spreadRangeDegrees", spreadRangeDegrees, 0, 1000000);
			r.Require(minimumLeadFraction + leadVariation <= 1, "EnemyGunTuning", "inconsistent settings");
			r.Require(
				minimumSpreadDegrees + spreadRangeDegrees < 45, "EnemyGunTuning", "inconsistent settings");
		}
	};
	struct EnemyAITuning {
		std::array<float, 6> stylePassDelay{0.0f, 0.5f, 1.1f, 0.2f, 0.8f, 1.4f};
		std::array<float, 6> styleTurnScale{1.0f, 0.92f, 0.98f, 0.88f, 0.94f, 0.9f};
		std::array<float, 6> styleClimb{0.0f, 0.1f, -0.04f, 0.16f, -0.09f, 0.06f};
		std::array<float, 6> styleExitAngle{0.0f, 0.1f, 0.14f, 0.07f, 0.13f, 0.18f};
		std::array<float, 6> styleHighGSeconds{2.5f, 2.0f, 2.8f, 2.2f, 3.0f, 1.8f};

		float minimumPassSeconds = 3.0f;
		float maximumPassSeconds = 9.0f;
		float defenseOuterRadius = 12000.0f;
		float defenseInnerRadius = 8000.0f;
		float maximumContinuousTurnSeconds = 6.0f;
		float rearTargetDotThreshold = -0.2f;
		float unlockedCruiseSpeedScale = 1.1f;
		float unlockedPassDistanceScale = 0.8f;
		float unlockedAimHoldScale = 0.65f;
		float unlockedAttackRangeScale = 1.1f;
		float unlockedBurstTempoScale = 1.35f;
		float unlockedShotIntervalScale = 0.75f;
		std::array<float, 6> passLegsMeters{3200.0f, 4700.0f, 3900.0f, 6000.0f, 5200.0f, 6800.0f};
		std::array<float, 6> reentryLanesMeters{-900.0f, 700.0f, -1400.0f, 1200.0f, -500.0f, 1700.0f};
		std::array<float, 8> directionChangesRadians{1.5707963268f, -1.5707963268f, 3.1415926536f,
			-0.7853981634f, 1.5707963268f, 0.7853981634f, -3.1415926536f, -1.5707963268f};
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("minimumPassSeconds", minimumPassSeconds, 0.000001f, 1000000);
			r.Number("maximumPassSeconds", maximumPassSeconds, 0.000001f, 1000000);
			r.Number("defenseOuterRadius", defenseOuterRadius, 0.000001f, 1000000);
			r.Number("defenseInnerRadius", defenseInnerRadius, 0.000001f, 1000000);
			r.Number("maximumContinuousTurnSeconds", maximumContinuousTurnSeconds, 0.000001f, 1000000);
			r.Number("rearTargetDotThreshold", rearTargetDotThreshold, -1, 1);
			r.Number("unlockedCruiseSpeedScale", unlockedCruiseSpeedScale, 0.000001f, 1000000);
			r.Number("unlockedPassDistanceScale", unlockedPassDistanceScale, 0.000001f, 1000000);
			r.Number("unlockedAimHoldScale", unlockedAimHoldScale, 0.000001f, 1000000);
			r.Number("unlockedAttackRangeScale", unlockedAttackRangeScale, 0.000001f, 1000000);
			r.Number("unlockedBurstTempoScale", unlockedBurstTempoScale, 0.000001f, 1000000);
			r.Number("unlockedShotIntervalScale", unlockedShotIntervalScale, 0.000001f, 1000000);
			r.Array("stylePassDelay", stylePassDelay, -10, 10);
			r.Array("styleTurnScale", styleTurnScale, 0.000001f, 10);
			r.Array("styleClimb", styleClimb, -10, 10);
			r.Array("styleExitAngle", styleExitAngle, -10, 10);
			r.Array("styleHighGSeconds", styleHighGSeconds, 0.000001f, 10);
			r.Array("passLegsMeters", passLegsMeters, 1);
			r.Array("reentryLanesMeters", reentryLanesMeters);
			r.Array("directionChangesRadians", directionChangesRadians);
			r.Require(maximumPassSeconds >= minimumPassSeconds, "EnemyAITuning", "inconsistent settings");
			r.Require(defenseOuterRadius > defenseInnerRadius, "EnemyAITuning", "inconsistent settings");
		}
	};
	struct MissileSettings {
		float turnRateDegreesPerSecond = 90.0f;
		float releaseSideOffsetMeters = 14.0f;
		float releaseOffsetMeters = 12.0f;
		float lifetimeSeconds = 8.0f;
		float smokeIntervalSeconds = 0.06f;
		unsigned ammoCount = 2;
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("turnRateDegreesPerSecond", turnRateDegreesPerSecond, 0.000001f, 360);
			r.Number("releaseSideOffsetMeters", releaseSideOffsetMeters, 0, 1000000);
			r.Number("releaseOffsetMeters", releaseOffsetMeters, 0, 1000000);
			r.Number("lifetimeSeconds", lifetimeSeconds, 0.001f, 120);
			r.Number("smokeIntervalSeconds", smokeIntervalSeconds, 0.001f, 120);
			r.Integer("ammoCount", ammoCount, 16);
		}
	};
	struct CinematicTextSettings {
		float textScale = 1.0f;
		float briefingFontSize = 48.0f;
		float titleFontSize = 48.0f;
		float subtitleFontSize = 28.0f;
		float hintFontSize = 24.0f;
		float lineSpacing = 1.6f;
		float barHeightFraction = 0.1f;
		float titleBottomOffset = 120.0f;
		float subtitleBottomOffset = 58.0f;
		std::string skipHint = "Enter：スキップ";
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("textScale", textScale, 0.000001f, 1000000);
			r.Number("briefingFontSize", briefingFontSize, 0.000001f, 1000000);
			r.Number("titleFontSize", titleFontSize, 0.000001f, 1000000);
			r.Number("subtitleFontSize", subtitleFontSize, 0.000001f, 1000000);
			r.Number("hintFontSize", hintFontSize, 0.000001f, 1000000);
			r.Number("lineSpacing", lineSpacing, 0.000001f, 1000000);
			r.Number("barHeightFraction", barHeightFraction, 0.000001f, 1000000);
			r.Number("titleBottomOffset", titleBottomOffset, 0.000001f, 1000000);
			r.Number("subtitleBottomOffset", subtitleBottomOffset, 0.000001f, 1000000);
			r.Text("skipHint", skipHint);
			r.Require(barHeightFraction < 0.4f, "CinematicTextSettings", "inconsistent settings");
			r.Require(lineSpacing >= 1, "CinematicTextSettings", "inconsistent settings");
			r.Require(textScale <= 4, "CinematicTextSettings", "inconsistent settings");
		}
	};
	struct AudioSettings {
		float flightVolume = 0.18f;
		float gunVolume = 0.42f;
		float battleBgmVolume = 0.35f;
		float launchVolume = 0.65f;
		float gunHitVolume = 0.65f;
		float missileHitVolume = 0.75f;
		float aircraftVolume = 0.65f;
		float shipVolume = 0.75f;
		float hitVolume = 0.32f;
		float warningVolume = 0.45f;
		float missileWarningIntervalSeconds = 0.22f;
		float lockWarningIntervalSeconds = 0.65f;
		std::string flightFile = "プレイヤー飛行音(50秒).mp3";
		std::string gunFile = "プレイヤー機関銃発射音(5秒間).mp3";
		std::string launchFile = "プレイヤーミサイル発射音.mp3";
		std::string gunHitFile = "プレイヤー機関銃被弾.mp3";
		std::string missileHitFile = "プレイヤーミサイル被弾.mp3";
		std::string aircraftFile = "敵航空機爆発.mp3";
		std::string shipFile = "敵戦艦爆発.mp3";
		std::string battleBgmFile = "BattleBGM.mp3";
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("flightVolume", flightVolume, 0, 1);
			r.Number("gunVolume", gunVolume, 0, 1);
			r.Number("battleBgmVolume", battleBgmVolume, 0, 1);
			r.Number("launchVolume", launchVolume, 0, 1);
			r.Number("gunHitVolume", gunHitVolume, 0, 1);
			r.Number("missileHitVolume", missileHitVolume, 0, 1);
			r.Number("aircraftVolume", aircraftVolume, 0, 1);
			r.Number("shipVolume", shipVolume, 0, 1);
			r.Number("hitVolume", hitVolume, 0, 1);
			r.Number("warningVolume", warningVolume, 0, 1);
			r.Number("missileWarningIntervalSeconds", missileWarningIntervalSeconds, 0.000001f, 1000000);
			r.Number("lockWarningIntervalSeconds", lockWarningIntervalSeconds, 0.000001f, 1000000);
			r.Text("flightFile", flightFile);
			r.Text("gunFile", gunFile);
			r.Text("launchFile", launchFile);
			r.Text("gunHitFile", gunHitFile);
			r.Text("missileHitFile", missileHitFile);
			r.Text("aircraftFile", aircraftFile);
			r.Text("shipFile", shipFile);
			r.Text("battleBgmFile", battleBgmFile);
		}
	};
	struct UiTuning {
		float radarRangeMeters = 10000.0f;
		DirectX::XMFLOAT4 ink{0.9098039215686274f, 0.8509803921568627f, 0.7098039215686275f, 0.95f};
		DirectX::XMFLOAT4 selected{0.95f, 0.65f, 0.24f, 1.0f};
		DirectX::XMFLOAT4 alert{0.83f, 0.34f, 0.23f, 1.0f};
		DirectX::XMFLOAT4 caution{0.85f, 0.64f, 0.32f, 1.0f};
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("radarRangeMeters", radarRangeMeters, 0.000001f, 1000000);
			r.Color("ink", ink);
			r.Color("selected", selected);
			r.Color("alert", alert);
			r.Color("caution", caution);
		}
	};
} // namespace Game
