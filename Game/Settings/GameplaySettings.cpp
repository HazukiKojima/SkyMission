#include "GameplaySettings.h"

namespace Game {
	namespace {
		constexpr float kMaximumCombatFeedbackSeconds = 10000.0f;
		constexpr float kMaximumPropellerAngularSpeedPerSpeed = 10000.0f;
		constexpr float kMaximumAiTuningMagnitude = 100000.0f;
		constexpr float kMaximumHudLayoutValue = 10000.0f;
		constexpr float kMaximumHudIndicatorScale = 4.0f;
		constexpr float kMaximumCameraShakeDegrees = 5.0f;
		constexpr float kMaximumCameraShakeSeconds = 5.0f;
		constexpr float kMinimumCameraFovDegrees = 1.0f;
		constexpr float kMaximumCameraFovDegrees = 120.0f;
		constexpr float kMaximumCameraPitchLimitRadians = 1.5f;
		constexpr float kMaximumGunSpreadDegrees = 45.0f;
		constexpr float kMaximumLetterboxBarFraction = 0.4f;
		constexpr float kMaximumCinematicTextScale = 4.0f;
		constexpr float kMinimumRadarSize = 100.0f;
		constexpr float kMaximumRadarSize = 1080.0f;
		constexpr float kMaximumRadarRadius = 160.0f;
	}
	void CombatFeedbackSettings::Load(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("/feedback/playerHitSeconds", playerHitSeconds, 0, kMaximumCombatFeedbackSeconds);
		r.Number("/feedback/hitSeconds", hitSeconds, 0, kMaximumCombatFeedbackSeconds);
		r.Number("/feedback/destroyedSeconds", destroyedSeconds, 0, kMaximumCombatFeedbackSeconds);
		r.Number("/feedback/enemyFireWarningSeconds", enemyFireWarningSeconds, 0, kMaximumCombatFeedbackSeconds);
		r.Number("/feedback/allDownSeconds", allDownSeconds, 0, kMaximumCombatFeedbackSeconds);
	}

	void AircraftAnimationSettings::Load(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("propellerAngularSpeedPerSpeed", propellerAngularSpeedPerSpeed, 0, kMaximumPropellerAngularSpeedPerSpeed);
	}

	void CameraSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("fovDegrees", fovDegrees);
		r.Number("moveSpeed", moveSpeed);
		r.Number("mouseSensitivity", mouseSensitivity);
		r.Number("lookReturnRate", lookReturnRate);
		r.Number("followDistance", followDistance);
		r.Number("maxFollowDistance", maxFollowDistance);
		r.Number("followHeight", followHeight);
		r.Number("followLookHeight", followLookHeight);
		r.Number("followSmoothing", followSmoothing);
		r.Number("followDirectionSmoothing", followDirectionSmoothing);
		r.Number("lookPitchLimit", lookPitchLimit);
		r.Number("orbitPitchLimit", orbitPitchLimit);
		r.Number("pitchFollowScale", pitchFollowScale);
		r.Number("pitchFollowLimit", pitchFollowLimit);
		r.Number("speedDistanceStart", speedDistanceStart, 0);
		r.Number("lookAheadMeters", lookAheadMeters);
		r.Number("bankRollDegrees", bankRollDegrees, 0, 45);
		r.Number("bankOffsetMeters", bankOffsetMeters, 0);
		r.Number("/shake/accelerationDegrees", shake.accelerationDegrees, 0, kMaximumCameraShakeDegrees);
		r.Number("/shake/highGDegrees", shake.highGDegrees, 0, kMaximumCameraShakeDegrees);
		r.Number("/shake/gunFireDegrees", shake.gunFireDegrees, 0, kMaximumCameraShakeDegrees);
		r.Number("/shake/gunFireSeconds", shake.gunFireSeconds, 0, kMaximumCameraShakeSeconds);
		r.Number("/shake/missileLaunchDegrees", shake.missileLaunchDegrees, 0, kMaximumCameraShakeDegrees);
		r.Number("/shake/missileLaunchSeconds", shake.missileLaunchSeconds, 0, kMaximumCameraShakeSeconds);
		r.Number("/shake/gunHitDegrees", shake.gunHitDegrees, 0, kMaximumCameraShakeDegrees);
		r.Number("/shake/heavyHitDegrees", shake.heavyHitDegrees, 0, kMaximumCameraShakeDegrees);
		r.Number("/shake/hitSeconds", shake.hitSeconds, 0, kMaximumCameraShakeSeconds);
		r.Number("/shake/enemyDestroyedDegrees", shake.enemyDestroyedDegrees, 0, kMaximumCameraShakeDegrees);
		r.Number("/shake/enemyDestroyedSeconds", shake.enemyDestroyedSeconds, 0, kMaximumCameraShakeSeconds);
		r.Number("/shake/frequencyHz", shake.frequencyHz, 1, 20);
		r.Number("/shake/responsePerSecond", shake.responsePerSecond, 0.1f, 100);
		r.Require(maxFollowDistance >= followDistance, "CameraSettings", "inconsistent settings");
		r.Require(fovDegrees > kMinimumCameraFovDegrees && fovDegrees < kMaximumCameraFovDegrees, "CameraSettings", "inconsistent settings");
		r.Require(lookPitchLimit < kMaximumCameraPitchLimitRadians && orbitPitchLimit < kMaximumCameraPitchLimitRadians &&
				pitchFollowLimit < kMaximumCameraPitchLimitRadians,
			"CameraSettings", "inconsistent settings");
	}

	void EnemyGunTuning::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("damage", damage);
		r.Number("lifetimeSeconds", lifetimeSeconds, 0.001f, 120);
		r.Number("initialDelaySeconds", initialDelaySeconds);
		r.Number("initialDelayRangeSeconds", initialDelayRangeSeconds, 0);
		r.Number("burstSeconds", burstSeconds);
		r.Number("burstRangeSeconds", burstRangeSeconds, 0);
		r.Number("pauseSeconds", pauseSeconds);
		r.Number("pauseRangeSeconds", pauseRangeSeconds, 0);

		// 予測照準への寄せ方と弾の散布幅を読み込む
		r.Number("minimumLeadFraction", minimumLeadFraction, 0, 1);
		r.Number("leadVariation", leadVariation, 0, 1);
		r.Number("yawErrorDegrees", yawErrorDegrees, 0);
		r.Number("pitchErrorDegrees", pitchErrorDegrees, 0);
		r.Number("minimumSpreadDegrees", minimumSpreadDegrees, 0);
		r.Number("spreadRangeDegrees", spreadRangeDegrees, 0);
		r.Require(minimumLeadFraction + leadVariation <= 1, "EnemyGunTuning", "inconsistent settings");
		r.Require(minimumSpreadDegrees + spreadRangeDegrees < kMaximumGunSpreadDegrees, "EnemyGunTuning", "inconsistent settings");
	}

	void EnemyAITuning::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("safeDistanceScale", safeDistanceScale, 0, kMaximumAiTuningMagnitude);
		r.Number("safeDistanceSlotStep", safeDistanceSlotStep, 0, kMaximumAiTuningMagnitude);
		r.Number("reentryAheadDot", reentryAheadDot, 0, 1);
		r.Number("reentryReadyTimeScale", reentryReadyTimeScale, 0, kMaximumAiTuningMagnitude);

		// 衝突回避を始める距離と回避方向を読み込む
		r.Number("collisionClosestMeters", collisionClosestMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("collisionClosestDistanceScale", collisionClosestDistanceScale, 0, kMaximumAiTuningMagnitude);
		r.Number("collisionRangeMeters", collisionRangeMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("collisionRangeScale", collisionRangeScale, 0, kMaximumAiTuningMagnitude);
		r.Number("collisionForwardWeight", collisionForwardWeight, 0, kMaximumAiTuningMagnitude);
		r.Number("collisionClimbWeight", collisionClimbWeight, 0, kMaximumAiTuningMagnitude);
		r.Number("collisionRecheckSeconds", collisionRecheckSeconds, SettingsReader::kMinimumPositiveValue, kMaximumAiTuningMagnitude);
		r.Number("continuousTurnThreshold", continuousTurnThreshold, 0, 1);
		r.Number("opportunisticFireDistance", opportunisticFireDistance, 0, kMaximumAiTuningMagnitude);
		r.Number("recoverClimb", recoverClimb, 0, kMaximumAiTuningMagnitude);
		r.Number("turnDirectionThresholdRadians", turnDirectionThresholdRadians, 0, kMaximumAiTuningMagnitude);
		r.Number("defenseTurnRadius", defenseTurnRadius, 0, kMaximumAiTuningMagnitude);

		// 離脱から再接近までの時間と経路の変化量を読み込む
		r.Number("passMinimumVariationSeconds", passMinimumVariationSeconds, 0, kMaximumAiTuningMagnitude);
		r.Number("passMaximumBaseSeconds", passMaximumBaseSeconds, 0, kMaximumAiTuningMagnitude);
		r.Number("passMaximumSlotSeconds", passMaximumSlotSeconds, 0, kMaximumAiTuningMagnitude);
		r.Number("passMaximumVariationSeconds", passMaximumVariationSeconds, 0, kMaximumAiTuningMagnitude);
		r.Number("passLegVariationMeters", passLegVariationMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("passReferenceDistanceMeters", passReferenceDistanceMeters, SettingsReader::kMinimumPositiveValue, kMaximumAiTuningMagnitude);
		r.Number("initialReentrySeconds", initialReentrySeconds, SettingsReader::kMinimumPositiveValue, kMaximumAiTuningMagnitude);
		r.Number("reentryMinimumSeconds", reentryMinimumSeconds, SettingsReader::kMinimumPositiveValue, kMaximumAiTuningMagnitude);
		r.Number("reentryVariationSeconds", reentryVariationSeconds, 0, kMaximumAiTuningMagnitude);
		r.Number("reentryLaneScale", reentryLaneScale, 0, kMaximumAiTuningMagnitude);
		r.Number("reentryLaneVariation", reentryLaneVariation, 0, kMaximumAiTuningMagnitude);
		r.Number("reentryForwardStepMeters", reentryForwardStepMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("reentryForwardOffsetMeters", reentryForwardOffsetMeters, -kMaximumAiTuningMagnitude, kMaximumAiTuningMagnitude);
		r.Number("reentryClimbMeters", reentryClimbMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("turnBurstScale", turnBurstScale, 0, kMaximumAiTuningMagnitude);
		r.Number("turnBurstVariation", turnBurstVariation, 0, kMaximumAiTuningMagnitude);
		r.Number("exitAngleOffsetRadians", exitAngleOffsetRadians, 0, kMaximumAiTuningMagnitude);
		r.Number("exitAngleVariationRadians", exitAngleVariationRadians, 0, kMaximumAiTuningMagnitude);
		r.Number("closePassDistanceMeters", closePassDistanceMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("closePassDistanceScale", closePassDistanceScale, 0, kMaximumAiTuningMagnitude);
		r.Number("closePassLateralThresholdMeters", closePassLateralThresholdMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("closePassAngleDegrees", closePassAngleDegrees, 0, kMaximumAiTuningMagnitude);
		r.Number("closePassAngleSlotDegrees", closePassAngleSlotDegrees, 0, kMaximumAiTuningMagnitude);
		r.Number("closePassAngleVariationDegrees", closePassAngleVariationDegrees, 0, kMaximumAiTuningMagnitude);
		r.Number("climbVariation", climbVariation, 0, kMaximumAiTuningMagnitude);
		r.Number("lowAltitudeClearanceMeters", lowAltitudeClearanceMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("escapeClimbLimit", escapeClimbLimit, 0, 1.5f);
		r.Number("bankYawThresholdRadians", bankYawThresholdRadians, 0, kMaximumAiTuningMagnitude);
		r.Number("rudderYawLimitRadians", rudderYawLimitRadians, 0, kMaximumAiTuningMagnitude);
		r.Number("rudderGain", rudderGain, 0, kMaximumAiTuningMagnitude);
		r.Number("desiredPitchLimitRadians", desiredPitchLimitRadians, 0, 1.5f);
		r.Number("targetAltitudeClearanceMeters", targetAltitudeClearanceMeters, 0, kMaximumAiTuningMagnitude);
		r.Number("descentPitchLimitRadians", descentPitchLimitRadians, 0, 1.5f);
		r.Number("spacingHighGSeconds", spacingHighGSeconds, 0, kMaximumAiTuningMagnitude);
		r.Number("highGYawThresholdRadians", highGYawThresholdRadians, 0, kMaximumAiTuningMagnitude);
		r.Number("patrolPitchLimitRadians", patrolPitchLimitRadians, 0, 1.5f);
		r.Number("patrolHighGThreshold", patrolHighGThreshold, 0, 1);
		r.Number("patrolHighGMinimumTurn", patrolHighGMinimumTurn, 0, 1);

		// 離脱・防衛の上限と非ロック機の攻撃倍率を読み込む
		r.Number("minimumPassSeconds", minimumPassSeconds);
		r.Number("maximumPassSeconds", maximumPassSeconds);
		r.Number("defenseOuterRadius", defenseOuterRadius);
		r.Number("defenseInnerRadius", defenseInnerRadius);
		r.Number("maximumContinuousTurnSeconds", maximumContinuousTurnSeconds);
		r.Number("rearTargetDotThreshold", rearTargetDotThreshold, -1, 1);
		r.Number("unlockedCruiseSpeedScale", unlockedCruiseSpeedScale);
		r.Number("unlockedPassDistanceScale", unlockedPassDistanceScale);
		r.Number("unlockedAimHoldScale", unlockedAimHoldScale);
		r.Number("unlockedAttackRangeScale", unlockedAttackRangeScale);
		r.Number("unlockedBurstTempoScale", unlockedBurstTempoScale);
		r.Number("unlockedShotIntervalScale", unlockedShotIntervalScale);

		// 個体ごとの行動差と再進入経路を読み込む
		r.Array("stylePassDelay", stylePassDelay, -10, 10);
		r.Array("styleTurnScale", styleTurnScale, SettingsReader::kMinimumPositiveValue, 10);
		r.Array("styleClimb", styleClimb, -10, 10);
		r.Array("styleExitAngle", styleExitAngle, -10, 10);
		r.Array("styleHighGSeconds", styleHighGSeconds, SettingsReader::kMinimumPositiveValue, 10);
		r.Array("passLegsMeters", passLegsMeters, 1);
		r.Array("reentryLanesMeters", reentryLanesMeters);
		r.Array("directionChangesRadians", directionChangesRadians);

		// 各値の範囲に加えて時間と防衛半径の大小関係を確認する
		r.Require(maximumPassSeconds >= minimumPassSeconds, "EnemyAITuning", "inconsistent settings");
		r.Require(defenseOuterRadius > defenseInnerRadius, "EnemyAITuning", "inconsistent settings");
	}

	void MissileSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("turnRateDegreesPerSecond", turnRateDegreesPerSecond, SettingsReader::kMinimumPositiveValue, 360);
		r.Number("releaseSideOffsetMeters", releaseSideOffsetMeters, 0);
		r.Number("releaseOffsetMeters", releaseOffsetMeters, 0);
		r.Number("lifetimeSeconds", lifetimeSeconds, 0.001f, 120);
		r.Number("smokeIntervalSeconds", smokeIntervalSeconds, 0.001f, 120);
		r.Integer("ammoCount", ammoCount, 16);
	}

	void CinematicTextSettings::Load(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("textScale", textScale);
		r.Number("briefingFontSize", briefingFontSize);
		r.Number("titleFontSize", titleFontSize);
		r.Number("subtitleFontSize", subtitleFontSize);
		r.Number("hintFontSize", hintFontSize);
		r.Number("lineSpacing", lineSpacing);
		r.Number("barHeightFraction", barHeightFraction);
		r.Number("titleBottomOffset", titleBottomOffset);
		r.Number("subtitleBottomOffset", subtitleBottomOffset);
		r.Text("skipHint", skipHint);
		r.Text("gamepadSkipHint", gamepadSkipHint);
		r.Require(barHeightFraction < kMaximumLetterboxBarFraction, "CinematicTextSettings", "inconsistent settings");
		r.Require(lineSpacing >= 1, "CinematicTextSettings", "inconsistent settings");
		r.Require(textScale <= kMaximumCinematicTextScale, "CinematicTextSettings", "inconsistent settings");
	}

	void AudioSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
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
		r.Number("missileWarningIntervalSeconds", missileWarningIntervalSeconds);
		r.Number("lockWarningIntervalSeconds", lockWarningIntervalSeconds);

		// 音量とは別に再生する音声ファイルを読み込む
		r.Text("flightFile", flightFile);
		r.Text("gunFile", gunFile);
		r.Text("launchFile", launchFile);
		r.Text("gunHitFile", gunHitFile);
		r.Text("missileHitFile", missileHitFile);
		r.Text("aircraftFile", aircraftFile);
		r.Text("shipFile", shipFile);
		r.Text("battleBgmFile", battleBgmFile);
	}

	void HudLayoutSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Array("/layout/resultPosition", resultPosition, 0, kMaximumHudLayoutValue);
		r.Array("/layout/retryPosition", retryPosition, 0, kMaximumHudLayoutValue);
		r.Array("/layout/playerHitPosition", playerHitPosition, 0, kMaximumHudLayoutValue);
		r.Array("/layout/targetStatusPosition", targetStatusPosition, 0, kMaximumHudLayoutValue);
		r.Array("/layout/destroyedPosition", destroyedPosition, 0, kMaximumHudLayoutValue);
		r.Array("/layout/hitPosition", hitPosition, 0, kMaximumHudLayoutValue);
		r.Array("/layout/allDownPosition", allDownPosition, 0, kMaximumHudLayoutValue);

		// 操作説明と計器の配置を読み込む
		r.Number("/layout/controlsFontSize", controlsFontSize, 0, kMaximumHudLayoutValue);
		r.Number("/layout/controlsLineHeight", controlsLineHeight, 0, kMaximumHudLayoutValue);
		r.Number("/layout/controlsRightMargin", controlsRightMargin, 0, kMaximumHudLayoutValue);
		r.Number("/layout/controlsBottomMargin", controlsBottomMargin, 0, kMaximumHudLayoutValue);
		r.Number("/layout/radarSize", radarSize, 0, kMaximumHudLayoutValue);
		r.Number("/layout/radarLeftMargin", radarLeftMargin, 0, kMaximumHudLayoutValue);
		r.Number("/layout/radarBottomMargin", radarBottomMargin, 0, kMaximumHudLayoutValue);
		r.Number("/layout/radarRadius", radarRadius, 0, kMaximumHudLayoutValue);
		r.Number("/layout/radarStatusGap", radarStatusGap, 0, kMaximumHudLayoutValue);
		r.Number("/layout/flightPanelTop", flightPanelTop, 0, kMaximumHudLayoutValue);
		r.Number("/layout/flightPanelWidth", flightPanelWidth, 0, kMaximumHudLayoutValue);
		r.Number("/layout/flightPanelHeight", flightPanelHeight, 0, kMaximumHudLayoutValue);
		r.Number("/layout/flightPanelTipWidth", flightPanelTipWidth, 0, kMaximumHudLayoutValue);
		r.Number("/layout/speedPanelX", speedPanelX, 0, kMaximumHudLayoutValue);
		r.Number("/layout/altitudePanelX", altitudePanelX, 0, kMaximumHudLayoutValue);
		r.Number("/layout/flightLabelGap", flightLabelGap, 0, kMaximumHudLayoutValue);
		r.Number("/layout/flightValuePadding", flightValuePadding, 0, kMaximumHudLayoutValue);
		r.Number("/layout/healthStatusY", healthStatusY, 0, kMaximumHudLayoutValue);
		r.Number("/layout/fireWarningY", fireWarningY, 0, kMaximumHudLayoutValue);
		r.Number("/layout/missionX", missionX, 0, kMaximumHudLayoutValue);
		r.Number("/layout/missionY", missionY, 0, kMaximumHudLayoutValue);
		r.Number("/layout/missionLineHeight", missionLineHeight, 0, kMaximumHudLayoutValue);
		r.Number("/layout/missileStatusY", missileStatusY, 0, kMaximumHudLayoutValue);
		r.Number("/layout/lockStatusY", lockStatusY, 0, kMaximumHudLayoutValue);

		// 照準や目標マーカーの寸法を読み込む
		r.Number("/layout/carrierMarkerRadius", carrierMarkerRadius, 0, kMaximumHudLayoutValue);
		r.Number("/layout/combatAreaMarkerRadius", combatAreaMarkerRadius, 0, kMaximumHudLayoutValue);
		r.Number("/layout/impactMarkerRadius", impactMarkerRadius, 0, kMaximumHudLayoutValue);
		r.Number("/layout/directionArrowScale", directionArrowScale, 0, kMaximumHudLayoutValue);
		r.Number("/layout/offscreenIndicatorScale", offscreenIndicatorScale, 0, kMaximumHudLayoutValue);
		r.Number("/layout/gunSightScale", gunSightScale, 0, kMaximumHudLayoutValue);
		r.Number("/layout/selectedEnemyRadius", selectedEnemyRadius, 0, kMaximumHudLayoutValue);
		r.Number("/layout/enemyRadius", enemyRadius, 0, kMaximumHudLayoutValue);
		r.Number("/layout/markerTextGap", markerTextGap, 0, kMaximumHudLayoutValue);
		r.Number("/layout/markerLabelGap", markerLabelGap, 0, kMaximumHudLayoutValue);
		r.Number("/layout/screenTextMargin", screenTextMargin, 0, kMaximumHudLayoutValue);

		// 組み合わせたときに表示が成立する寸法と倍率か確認する
		r.Require(radarSize >= kMinimumRadarSize && radarSize <= kMaximumRadarSize, "layout", "inconsistent HUD layout");
		r.Require(radarRadius > 0 && radarRadius <= kMaximumRadarRadius, "layout", "inconsistent HUD layout");
		r.Require(controlsFontSize > 0 && controlsLineHeight >= controlsFontSize, "layout",
			"inconsistent HUD layout");
		r.Require(flightPanelWidth > flightPanelTipWidth && flightPanelHeight > 0, "layout",
			"inconsistent HUD layout");
		r.Require(missionLineHeight > 0, "layout", "inconsistent HUD layout");
		r.Require(directionArrowScale > 0 && directionArrowScale <= kMaximumHudIndicatorScale, "layout", "inconsistent HUD layout");
		r.Require(
			offscreenIndicatorScale > 0 && offscreenIndicatorScale <= kMaximumHudIndicatorScale, "layout", "inconsistent HUD layout");
		r.Require(gunSightScale > 0 && gunSightScale <= kMaximumHudIndicatorScale, "layout", "inconsistent HUD layout");
	}

	void UiTuning::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		layout.Load(reader, path);
		r.Number("radarRangeMeters", radarRangeMeters);
		r.Color("ink", ink);
		r.Color("selected", selected);
		r.Color("alert", alert);
		r.Color("caution", caution);
	}
} // namespace Game
