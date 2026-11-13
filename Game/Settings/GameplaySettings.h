#pragma once
#include "SettingsReader.h"
#include <algorithm>
namespace Game {
	struct CombatFeedbackSettings {
		float playerHitSeconds = 0.7f;
		float hitSeconds = 0.15f;
		float destroyedSeconds = 0.8f;
		float enemyFireWarningSeconds = 1.0f;
		float allDownSeconds = 3.0f;

		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("/feedback/playerHitSeconds", playerHitSeconds, 0, 10000);
			r.Number("/feedback/hitSeconds", hitSeconds, 0, 10000);
			r.Number("/feedback/destroyedSeconds", destroyedSeconds, 0, 10000);
			r.Number("/feedback/enemyFireWarningSeconds", enemyFireWarningSeconds, 0, 10000);
			r.Number("/feedback/allDownSeconds", allDownSeconds, 0, 10000);
		}
	};
	struct AircraftAnimationSettings {
		float propellerAngularSpeedPerSpeed = 0.06f;

		float AdvancePropeller(float angle, float speed, float dt) const {
			return std::fmod(
				angle + speed * propellerAngularSpeedPerSpeed * std::min(dt, 0.25f), DirectX::XM_2PI);
		}

		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("propellerAngularSpeedPerSpeed", propellerAngularSpeedPerSpeed, 0, 10000);
		}
	};

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
		float safeDistanceScale = 1.42f;
		float safeDistanceSlotStep = 0.06f;
		float reentryAheadDot = 0.85f;
		float reentryReadyTimeScale = 0.5f;
		float collisionClosestMeters = 1000.0f;
		float collisionClosestDistanceScale = 0.34f;
		float collisionRangeMeters = 3500.0f;
		float collisionRangeScale = 1.2f;
		float collisionForwardWeight = 0.35f;
		float collisionClimbWeight = 0.35f;
		float collisionRecheckSeconds = 0.35f;
		float continuousTurnThreshold = 0.3f;
		float opportunisticFireDistance = 2200.0f;
		float recoverClimb = 0.7f;
		float turnDirectionThresholdRadians = 0.05f;
		float defenseTurnRadius = 10000.0f;
		float passMinimumVariationSeconds = 0.9f;
		float passMaximumBaseSeconds = 7.0f;
		float passMaximumSlotSeconds = 0.38f;
		float passMaximumVariationSeconds = 0.6f;
		float passLegVariationMeters = 900.0f;
		float passReferenceDistanceMeters = 4500.0f;
		float initialReentrySeconds = 1.5f;
		float reentryMinimumSeconds = 1.2f;
		float reentryVariationSeconds = 1.8f;
		float reentryLaneScale = 0.8f;
		float reentryLaneVariation = 0.4f;
		float reentryForwardStepMeters = 600.0f;
		float reentryForwardOffsetMeters = -500.0f;
		float reentryClimbMeters = 2000.0f;
		float turnBurstScale = 0.8f;
		float turnBurstVariation = 0.4f;
		float exitAngleOffsetRadians = 0.12f;
		float exitAngleVariationRadians = 0.35f;
		float closePassDistanceMeters = 1800.0f;
		float closePassDistanceScale = 0.65f;
		float closePassLateralThresholdMeters = 100.0f;
		float closePassAngleDegrees = 55.0f;
		float closePassAngleSlotDegrees = 5.0f;
		float closePassAngleVariationDegrees = 5.0f;
		float climbVariation = 0.08f;
		float lowAltitudeClearanceMeters = 350.0f;
		float escapeClimbLimit = 0.18f;
		float bankYawThresholdRadians = 0.15f;
		float rudderYawLimitRadians = 0.6f;
		float rudderGain = 2.0f;
		float desiredPitchLimitRadians = 0.6f;
		float targetAltitudeClearanceMeters = 300.0f;
		float descentPitchLimitRadians = 0.25f;
		float spacingHighGSeconds = 1.8f;
		float highGYawThresholdRadians = 0.7f;
		float patrolPitchLimitRadians = 0.45f;
		float patrolHighGThreshold = 0.3f;
		float patrolHighGMinimumTurn = 0.85f;

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
			r.Number("safeDistanceScale", safeDistanceScale, 0, 100000);
			r.Number("safeDistanceSlotStep", safeDistanceSlotStep, 0, 100000);
			r.Number("reentryAheadDot", reentryAheadDot, 0, 1);
			r.Number("reentryReadyTimeScale", reentryReadyTimeScale, 0, 100000);
			r.Number("collisionClosestMeters", collisionClosestMeters, 0, 100000);
			r.Number("collisionClosestDistanceScale", collisionClosestDistanceScale, 0, 100000);
			r.Number("collisionRangeMeters", collisionRangeMeters, 0, 100000);
			r.Number("collisionRangeScale", collisionRangeScale, 0, 100000);
			r.Number("collisionForwardWeight", collisionForwardWeight, 0, 100000);
			r.Number("collisionClimbWeight", collisionClimbWeight, 0, 100000);
			r.Number("collisionRecheckSeconds", collisionRecheckSeconds, 0.000001f, 100000);
			r.Number("continuousTurnThreshold", continuousTurnThreshold, 0, 1);
			r.Number("opportunisticFireDistance", opportunisticFireDistance, 0, 100000);
			r.Number("recoverClimb", recoverClimb, 0, 100000);
			r.Number("turnDirectionThresholdRadians", turnDirectionThresholdRadians, 0, 100000);
			r.Number("defenseTurnRadius", defenseTurnRadius, 0, 100000);
			r.Number("passMinimumVariationSeconds", passMinimumVariationSeconds, 0, 100000);
			r.Number("passMaximumBaseSeconds", passMaximumBaseSeconds, 0, 100000);
			r.Number("passMaximumSlotSeconds", passMaximumSlotSeconds, 0, 100000);
			r.Number("passMaximumVariationSeconds", passMaximumVariationSeconds, 0, 100000);
			r.Number("passLegVariationMeters", passLegVariationMeters, 0, 100000);
			r.Number("passReferenceDistanceMeters", passReferenceDistanceMeters, 0.000001f, 100000);
			r.Number("initialReentrySeconds", initialReentrySeconds, 0.000001f, 100000);
			r.Number("reentryMinimumSeconds", reentryMinimumSeconds, 0.000001f, 100000);
			r.Number("reentryVariationSeconds", reentryVariationSeconds, 0, 100000);
			r.Number("reentryLaneScale", reentryLaneScale, 0, 100000);
			r.Number("reentryLaneVariation", reentryLaneVariation, 0, 100000);
			r.Number("reentryForwardStepMeters", reentryForwardStepMeters, 0, 100000);
			r.Number("reentryForwardOffsetMeters", reentryForwardOffsetMeters, -100000, 100000);
			r.Number("reentryClimbMeters", reentryClimbMeters, 0, 100000);
			r.Number("turnBurstScale", turnBurstScale, 0, 100000);
			r.Number("turnBurstVariation", turnBurstVariation, 0, 100000);
			r.Number("exitAngleOffsetRadians", exitAngleOffsetRadians, 0, 100000);
			r.Number("exitAngleVariationRadians", exitAngleVariationRadians, 0, 100000);
			r.Number("closePassDistanceMeters", closePassDistanceMeters, 0, 100000);
			r.Number("closePassDistanceScale", closePassDistanceScale, 0, 100000);
			r.Number("closePassLateralThresholdMeters", closePassLateralThresholdMeters, 0, 100000);
			r.Number("closePassAngleDegrees", closePassAngleDegrees, 0, 100000);
			r.Number("closePassAngleSlotDegrees", closePassAngleSlotDegrees, 0, 100000);
			r.Number("closePassAngleVariationDegrees", closePassAngleVariationDegrees, 0, 100000);
			r.Number("climbVariation", climbVariation, 0, 100000);
			r.Number("lowAltitudeClearanceMeters", lowAltitudeClearanceMeters, 0, 100000);
			r.Number("escapeClimbLimit", escapeClimbLimit, 0, 1.5f);
			r.Number("bankYawThresholdRadians", bankYawThresholdRadians, 0, 100000);
			r.Number("rudderYawLimitRadians", rudderYawLimitRadians, 0, 100000);
			r.Number("rudderGain", rudderGain, 0, 100000);
			r.Number("desiredPitchLimitRadians", desiredPitchLimitRadians, 0, 1.5f);
			r.Number("targetAltitudeClearanceMeters", targetAltitudeClearanceMeters, 0, 100000);
			r.Number("descentPitchLimitRadians", descentPitchLimitRadians, 0, 1.5f);
			r.Number("spacingHighGSeconds", spacingHighGSeconds, 0, 100000);
			r.Number("highGYawThresholdRadians", highGYawThresholdRadians, 0, 100000);
			r.Number("patrolPitchLimitRadians", patrolPitchLimitRadians, 0, 1.5f);
			r.Number("patrolHighGThreshold", patrolHighGThreshold, 0, 1);
			r.Number("patrolHighGMinimumTurn", patrolHighGMinimumTurn, 0, 1);

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
	struct HudLayoutSettings {
		float controlsFontSize = 36.0f;
		float controlsLineHeight = 48.0f;
		float controlsRightMargin = 48.0f;
		float controlsBottomMargin = 48.0f;
		float radarSize = 320.0f;
		float radarLeftMargin = 48.0f;
		float radarBottomMargin = 48.0f;
		float radarRadius = 126.0f;
		float radarStatusGap = 24.0f;
		float flightPanelTop = 420.0f;
		float flightPanelWidth = 240.0f;
		float flightPanelHeight = 64.0f;
		float flightPanelTipWidth = 16.0f;
		float speedPanelX = 100.0f;
		float altitudePanelX = 1580.0f;
		float flightLabelGap = 8.0f;
		float flightValuePadding = 12.0f;
		float healthStatusY = 108.0f;
		float fireWarningY = 330.0f;
		// Right edge of the mission text in the 1920-wide reference layout.
		float missionX = 1872.0f;
		float missionY = 16.0f;
		float missionLineHeight = 44.0f;
		float missileStatusY = 188.0f;
		float lockStatusY = 236.0f;
		float carrierMarkerRadius = 44.0f;
		float combatAreaMarkerRadius = 26.0f;
		float impactMarkerRadius = 14.0f;
		float directionArrowScale = 1.0f;
		float offscreenIndicatorScale = 1.0f;
		float gunSightScale = 1.0f;
		float selectedEnemyRadius = 32.0f;
		float enemyRadius = 22.0f;
		float markerTextGap = 8.0f;
		float markerLabelGap = 4.0f;
		float screenTextMargin = 16.0f;

		std::array<float, 2> resultPosition{740.0f, 380.0f};
		std::array<float, 2> retryPosition{740.0f, 440.0f};
		std::array<float, 2> playerHitPosition{820.0f, 800.0f};
		std::array<float, 2> targetStatusPosition{80.0f, 80.0f};
		std::array<float, 2> destroyedPosition{920.0f, 640.0f};
		std::array<float, 2> hitPosition{920.0f, 640.0f};
		std::array<float, 2> allDownPosition{760.0f, 180.0f};
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Array("/layout/resultPosition", resultPosition, 0, 10000);
			r.Array("/layout/retryPosition", retryPosition, 0, 10000);
			r.Array("/layout/playerHitPosition", playerHitPosition, 0, 10000);
			r.Array("/layout/targetStatusPosition", targetStatusPosition, 0, 10000);
			r.Array("/layout/destroyedPosition", destroyedPosition, 0, 10000);
			r.Array("/layout/hitPosition", hitPosition, 0, 10000);
			r.Array("/layout/allDownPosition", allDownPosition, 0, 10000);

			r.Number("/layout/controlsFontSize", controlsFontSize, 0, 10000);
			r.Number("/layout/controlsLineHeight", controlsLineHeight, 0, 10000);
			r.Number("/layout/controlsRightMargin", controlsRightMargin, 0, 10000);
			r.Number("/layout/controlsBottomMargin", controlsBottomMargin, 0, 10000);
			r.Number("/layout/radarSize", radarSize, 0, 10000);
			r.Number("/layout/radarLeftMargin", radarLeftMargin, 0, 10000);
			r.Number("/layout/radarBottomMargin", radarBottomMargin, 0, 10000);
			r.Number("/layout/radarRadius", radarRadius, 0, 10000);
			r.Number("/layout/radarStatusGap", radarStatusGap, 0, 10000);
			r.Number("/layout/flightPanelTop", flightPanelTop, 0, 10000);
			r.Number("/layout/flightPanelWidth", flightPanelWidth, 0, 10000);
			r.Number("/layout/flightPanelHeight", flightPanelHeight, 0, 10000);
			r.Number("/layout/flightPanelTipWidth", flightPanelTipWidth, 0, 10000);
			r.Number("/layout/speedPanelX", speedPanelX, 0, 10000);
			r.Number("/layout/altitudePanelX", altitudePanelX, 0, 10000);
			r.Number("/layout/flightLabelGap", flightLabelGap, 0, 10000);
			r.Number("/layout/flightValuePadding", flightValuePadding, 0, 10000);
			r.Number("/layout/healthStatusY", healthStatusY, 0, 10000);
			r.Number("/layout/fireWarningY", fireWarningY, 0, 10000);
			r.Number("/layout/missionX", missionX, 0, 10000);
			r.Number("/layout/missionY", missionY, 0, 10000);
			r.Number("/layout/missionLineHeight", missionLineHeight, 0, 10000);
			r.Number("/layout/missileStatusY", missileStatusY, 0, 10000);
			r.Number("/layout/lockStatusY", lockStatusY, 0, 10000);
			r.Number("/layout/carrierMarkerRadius", carrierMarkerRadius, 0, 10000);
			r.Number("/layout/combatAreaMarkerRadius", combatAreaMarkerRadius, 0, 10000);
			r.Number("/layout/impactMarkerRadius", impactMarkerRadius, 0, 10000);
			r.Number("/layout/directionArrowScale", directionArrowScale, 0, 10000);
			r.Number("/layout/offscreenIndicatorScale", offscreenIndicatorScale, 0, 10000);
			r.Number("/layout/gunSightScale", gunSightScale, 0, 10000);
			r.Number("/layout/selectedEnemyRadius", selectedEnemyRadius, 0, 10000);
			r.Number("/layout/enemyRadius", enemyRadius, 0, 10000);
			r.Number("/layout/markerTextGap", markerTextGap, 0, 10000);
			r.Number("/layout/markerLabelGap", markerLabelGap, 0, 10000);
			r.Number("/layout/screenTextMargin", screenTextMargin, 0, 10000);

			r.Require(radarSize >= 100 && radarSize <= 1080, "layout", "inconsistent HUD layout");
			r.Require(radarRadius > 0 && radarRadius <= 160, "layout", "inconsistent HUD layout");
			r.Require(controlsFontSize > 0 && controlsLineHeight >= controlsFontSize, "layout",
				"inconsistent HUD layout");
			r.Require(flightPanelWidth > flightPanelTipWidth && flightPanelHeight > 0, "layout",
				"inconsistent HUD layout");
			r.Require(missionLineHeight > 0, "layout", "inconsistent HUD layout");
			r.Require(
				directionArrowScale > 0 && directionArrowScale <= 4, "layout", "inconsistent HUD layout");
			r.Require(offscreenIndicatorScale > 0 && offscreenIndicatorScale <= 4, "layout",
				"inconsistent HUD layout");
			r.Require(gunSightScale > 0 && gunSightScale <= 4, "layout", "inconsistent HUD layout");
		}
	};

	struct UiTuning {
		HudLayoutSettings layout;
		float radarRangeMeters = 10000.0f;
		DirectX::XMFLOAT4 ink{0.9098039215686274f, 0.8509803921568627f, 0.7098039215686275f, 0.95f};
		DirectX::XMFLOAT4 selected{0.95f, 0.65f, 0.24f, 1.0f};
		DirectX::XMFLOAT4 alert{0.83f, 0.34f, 0.23f, 1.0f};
		DirectX::XMFLOAT4 caution{0.85f, 0.64f, 0.32f, 1.0f};
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			layout.Load(reader, path);
			r.Number("radarRangeMeters", radarRangeMeters, 0.000001f, 1000000);
			r.Color("ink", ink);
			r.Color("selected", selected);
			r.Color("alert", alert);
			r.Color("caution", caution);
		}
	};
} // namespace Game
