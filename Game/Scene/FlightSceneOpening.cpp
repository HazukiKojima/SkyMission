#include "FlightScene.h"
#include <algorithm>

namespace Game {
	void FlightScene::EnableOpening(const OpeningCinematicSettings& settings,
		std::shared_ptr<Engine::Model> model, float visualScale, float farZ) {
		settings.Validate();
		m_openingSettings = settings;
		auto origin = m_player.GetPosition();
		origin.x += settings.originOffsetMeters.x;
		origin.y += settings.originOffsetMeters.y;
		origin.z += settings.originOffsetMeters.z;
		m_openingScene.Initialize(settings, std::move(model), visualScale, m_scene.GetEnvironment(), origin,
			m_width, m_height, farZ);
		m_openingEnabled = true;
		StartOpening();
	}
	void FlightScene::StartOpening() {
		// 通常の追従視点も先に確定し、暗転後と操作開始後で同じカメラを使う
		UpdateCamera({}, 0, {});
		m_cameraController.SnapToFollowTarget();
		m_scene.SetCamera(m_cameraController.GetCamera());
		m_opening.Start(m_openingSettings);
		m_openingScene.Update(m_opening.GetPose());
		if (m_briefingEnabled) m_opening.EnableBriefing();
		m_combat.opening = m_opening.GetPresentation();
		m_combat.audio.flying = m_combat.opening.briefingLine < 0;
	}
	void FlightScene::EnableOpeningBriefing() {
		m_briefingEnabled = true;
		m_opening.EnableBriefing();
		m_combat.opening = m_opening.GetPresentation();
		m_combat.audio.flying = m_combat.opening.briefingLine < 0;
	}
	void FlightScene::UpdateOpeningBriefing(OpeningVoiceStatus voice, float deltaSeconds) {
		m_opening.UpdateBriefing(voice, deltaSeconds);
		m_combat.opening = m_opening.GetPresentation();
	}
	void FlightScene::UpdateOpening(float deltaSeconds, bool skipRequested) {
		if (m_opening.GetPresentation().briefingLine >= 0 && !skipRequested) {
			m_combat.audio.flying = false;
			return;
		}
		const float previousSeconds = m_opening.GetPresentation().elapsedSeconds;
		m_opening.Update(deltaSeconds, skipRequested);
		m_openingScene.Update(m_opening.GetPose());
		// 自機視点に戻した後は無入力で飛行を進め、静止画から操作開始する見え方を防ぐ
		// カット境界を跨いだフレームは、切替後の時間だけ飛行させる
		const float flightSeconds = skipRequested
			? 0.0f
			: std::max(0.0f,
				  m_opening.GetPresentation().elapsedSeconds -
					  std::max(previousSeconds, m_openingSettings.cutSeconds));
		if (flightSeconds > 0) {
			UpdatePlayer({}, flightSeconds);
			UpdateEnemies(flightSeconds);
			ApplyRenderStates();
			RefreshCombatSnapshot();
			UpdateCamera({}, flightSeconds, {});
		}
		m_combat.opening = m_opening.GetPresentation();
		m_combat.audio.flying = m_combat.opening.briefingLine < 0;
		// 武器・ダメージ・任務の更新や操作入力は、終了・スキップのフレームにも実行しない
	}
} // namespace Game
