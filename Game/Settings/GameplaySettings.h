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

		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	};

	struct AircraftAnimationSettings {
		float propellerAngularSpeedPerSpeed = 0.06f;

		float AdvancePropeller(float angle, float speed, float dt) const {
			// 回転角を1周の範囲へ戻して値の増大を防ぐ
			return std::fmod(
				angle + speed * propellerAngularSpeedPerSpeed * std::min(dt, 0.25f), DirectX::XM_2PI);
		}

		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	};

	struct CameraShakeSettings {
		// 強さは角度（度）。0でその場面の揺れを無効化する
		float accelerationDegrees = 0.03f;
		float highGDegrees = 0.096f;
		float gunFireDegrees = 0.07f;
		float gunFireSeconds = 0.10f;
		float missileLaunchDegrees = 0.42f;
		float missileLaunchSeconds = 0.24f;
		float gunHitDegrees = 0.65f;
		float heavyHitDegrees = 1.35f;
		float hitSeconds = 0.42f;
		float enemyDestroyedDegrees = 0.25f;
		float enemyDestroyedSeconds = 0.32f;
		float frequencyHz = 9.0f;
		float responsePerSecond = 9.0f;
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
		float bankRollDegrees = 0.0f;
		float bankOffsetMeters = 0.0f;
		CameraShakeSettings shake;
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
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
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
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
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	};

	struct MissileSettings {
		float turnRateDegreesPerSecond = 90.0f;
		float releaseSideOffsetMeters = 14.0f;
		float releaseOffsetMeters = 12.0f;
		float lifetimeSeconds = 8.0f;
		float smokeIntervalSeconds = 0.06f;
		unsigned ammoCount = 2;
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
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
		std::string gamepadSkipHint = "OPTIONS：スキップ";
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
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
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
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

		// 幅1920の基準レイアウトにおける、ミッション文の右端
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
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	};

	struct UiTuning {
		HudLayoutSettings layout;
		float radarRangeMeters = 10000.0f;
		DirectX::XMFLOAT4 ink{0.9098039215686274f, 0.8509803921568627f, 0.7098039215686275f, 0.95f};
		DirectX::XMFLOAT4 selected{0.95f, 0.65f, 0.24f, 1.0f};
		DirectX::XMFLOAT4 alert{0.83f, 0.34f, 0.23f, 1.0f};
		DirectX::XMFLOAT4 caution{0.85f, 0.64f, 0.32f, 1.0f};
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	};
} // namespace Game
