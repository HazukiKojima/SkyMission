#include "GameplaySettings.h"

namespace Game {
	void CombatFeedbackSettings::Load(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("/feedback/playerHitSeconds", playerHitSeconds, 0, 10000);
		r.Number("/feedback/hitSeconds", hitSeconds, 0, 10000);
		r.Number("/feedback/destroyedSeconds", destroyedSeconds, 0, 10000);
		r.Number("/feedback/enemyFireWarningSeconds", enemyFireWarningSeconds, 0, 10000);
		r.Number("/feedback/allDownSeconds", allDownSeconds, 0, 10000);
	}

	void AircraftAnimationSettings::Load(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("propellerAngularSpeedPerSpeed", propellerAngularSpeedPerSpeed, 0, 10000);
	}

	void CameraSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
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
		r.Number("bankRollDegrees", bankRollDegrees, 0, 45);
		r.Number("bankOffsetMeters", bankOffsetMeters, 0, 1000000);
		r.Number("/shake/accelerationDegrees", shake.accelerationDegrees, 0, 5);
		r.Number("/shake/highGDegrees", shake.highGDegrees, 0, 5);
		r.Number("/shake/gunFireDegrees", shake.gunFireDegrees, 0, 5);
		r.Number("/shake/gunFireSeconds", shake.gunFireSeconds, 0, 5);
		r.Number("/shake/missileLaunchDegrees", shake.missileLaunchDegrees, 0, 5);
		r.Number("/shake/missileLaunchSeconds", shake.missileLaunchSeconds, 0, 5);
		r.Number("/shake/gunHitDegrees", shake.gunHitDegrees, 0, 5);
		r.Number("/shake/heavyHitDegrees", shake.heavyHitDegrees, 0, 5);
		r.Number("/shake/hitSeconds", shake.hitSeconds, 0, 5);
		r.Number("/shake/enemyDestroyedDegrees", shake.enemyDestroyedDegrees, 0, 5);
		r.Number("/shake/enemyDestroyedSeconds", shake.enemyDestroyedSeconds, 0, 5);
		r.Number("/shake/frequencyHz", shake.frequencyHz, 1, 20);
		r.Number("/shake/responsePerSecond", shake.responsePerSecond, 0.1f, 100);
		r.Require(maxFollowDistance >= followDistance, "CameraSettings", "inconsistent settings");
		r.Require(fovDegrees > 1 && fovDegrees < 120, "CameraSettings", "inconsistent settings");
		r.Require(lookPitchLimit < 1.5f && orbitPitchLimit < 1.5f && pitchFollowLimit < 1.5f,
			"CameraSettings", "inconsistent settings");
	}

	void EnemyGunTuning::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("damage", damage, 0.000001f, 1000000);
		r.Number("lifetimeSeconds", lifetimeSeconds, 0.001f, 120);
		r.Number("initialDelaySeconds", initialDelaySeconds, 0.000001f, 1000000);
		r.Number("initialDelayRangeSeconds", initialDelayRangeSeconds, 0, 1000000);
		r.Number("burstSeconds", burstSeconds, 0.000001f, 1000000);
		r.Number("burstRangeSeconds", burstRangeSeconds, 0, 1000000);
		r.Number("pauseSeconds", pauseSeconds, 0.000001f, 1000000);
		r.Number("pauseRangeSeconds", pauseRangeSeconds, 0, 1000000);

		// 予測照準への寄せ方と弾の散布幅を読み込む
		r.Number("minimumLeadFraction", minimumLeadFraction, 0, 1);
		r.Number("leadVariation", leadVariation, 0, 1);
		r.Number("yawErrorDegrees", yawErrorDegrees, 0, 1000000);
		r.Number("pitchErrorDegrees", pitchErrorDegrees, 0, 1000000);
		r.Number("minimumSpreadDegrees", minimumSpreadDegrees, 0, 1000000);
		r.Number("spreadRangeDegrees", spreadRangeDegrees, 0, 1000000);
		r.Require(minimumLeadFraction + leadVariation <= 1, "EnemyGunTuning", "inconsistent settings");
		r.Require(minimumSpreadDegrees + spreadRangeDegrees < 45, "EnemyGunTuning", "inconsistent settings");
	}

	void EnemyAITuning::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("safeDistanceScale", safeDistanceScale, 0, 100000);
		r.Number("safeDistanceSlotStep", safeDistanceSlotStep, 0, 100000);
		r.Number("reentryAheadDot", reentryAheadDot, 0, 1);
		r.Number("reentryReadyTimeScale", reentryReadyTimeScale, 0, 100000);

		// 衝突回避を始める距離と回避方向を読み込む
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

		// 離脱から再接近までの時間と経路の変化量を読み込む
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

		// 離脱・防衛の上限と非ロック機の攻撃倍率を読み込む
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

		// 個体ごとの行動差と再進入経路を読み込む
		r.Array("stylePassDelay", stylePassDelay, -10, 10);
		r.Array("styleTurnScale", styleTurnScale, 0.000001f, 10);
		r.Array("styleClimb", styleClimb, -10, 10);
		r.Array("styleExitAngle", styleExitAngle, -10, 10);
		r.Array("styleHighGSeconds", styleHighGSeconds, 0.000001f, 10);
		r.Array("passLegsMeters", passLegsMeters, 1);
		r.Array("reentryLanesMeters", reentryLanesMeters);
		r.Array("directionChangesRadians", directionChangesRadians);

		// 各値の範囲に加えて時間と防衛半径の大小関係を確認する
		r.Require(maximumPassSeconds >= minimumPassSeconds, "EnemyAITuning", "inconsistent settings");
		r.Require(defenseOuterRadius > defenseInnerRadius, "EnemyAITuning", "inconsistent settings");
	}

	void MissileSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("turnRateDegreesPerSecond", turnRateDegreesPerSecond, 0.000001f, 360);
		r.Number("releaseSideOffsetMeters", releaseSideOffsetMeters, 0, 1000000);
		r.Number("releaseOffsetMeters", releaseOffsetMeters, 0, 1000000);
		r.Number("lifetimeSeconds", lifetimeSeconds, 0.001f, 120);
		r.Number("smokeIntervalSeconds", smokeIntervalSeconds, 0.001f, 120);
		r.Integer("ammoCount", ammoCount, 16);
	}

	void CinematicTextSettings::Load(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
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
		r.Text("gamepadSkipHint", gamepadSkipHint);
		r.Require(barHeightFraction < 0.4f, "CinematicTextSettings", "inconsistent settings");
		r.Require(lineSpacing >= 1, "CinematicTextSettings", "inconsistent settings");
		r.Require(textScale <= 4, "CinematicTextSettings", "inconsistent settings");
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
		r.Number("missileWarningIntervalSeconds", missileWarningIntervalSeconds, 0.000001f, 1000000);
		r.Number("lockWarningIntervalSeconds", lockWarningIntervalSeconds, 0.000001f, 1000000);

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
		r.Array("/layout/resultPosition", resultPosition, 0, 10000);
		r.Array("/layout/retryPosition", retryPosition, 0, 10000);
		r.Array("/layout/playerHitPosition", playerHitPosition, 0, 10000);
		r.Array("/layout/targetStatusPosition", targetStatusPosition, 0, 10000);
		r.Array("/layout/destroyedPosition", destroyedPosition, 0, 10000);
		r.Array("/layout/hitPosition", hitPosition, 0, 10000);
		r.Array("/layout/allDownPosition", allDownPosition, 0, 10000);

		// 操作説明と計器の配置を読み込む
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

		// 照準や目標マーカーの寸法を読み込む
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

		// 組み合わせたときに表示が成立する寸法と倍率か確認する
		r.Require(radarSize >= 100 && radarSize <= 1080, "layout", "inconsistent HUD layout");
		r.Require(radarRadius > 0 && radarRadius <= 160, "layout", "inconsistent HUD layout");
		r.Require(controlsFontSize > 0 && controlsLineHeight >= controlsFontSize, "layout",
			"inconsistent HUD layout");
		r.Require(flightPanelWidth > flightPanelTipWidth && flightPanelHeight > 0, "layout",
			"inconsistent HUD layout");
		r.Require(missionLineHeight > 0, "layout", "inconsistent HUD layout");
		r.Require(directionArrowScale > 0 && directionArrowScale <= 4, "layout", "inconsistent HUD layout");
		r.Require(
			offscreenIndicatorScale > 0 && offscreenIndicatorScale <= 4, "layout", "inconsistent HUD layout");
		r.Require(gunSightScale > 0 && gunSightScale <= 4, "layout", "inconsistent HUD layout");
	}

	void UiTuning::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		layout.Load(reader, path);
		r.Number("radarRangeMeters", radarRangeMeters, 0.000001f, 1000000);
		r.Color("ink", ink);
		r.Color("selected", selected);
		r.Color("alert", alert);
		r.Color("caution", caution);
	}
} // namespace Game
