#include "FlightScene.h"
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace Game {
	void FlightScene::ConfigureCinematics(const std::map<std::string, CinematicSettings>& definitions,
		CinematicModelResolver resolve, float farZ) {
		if (!resolve || !std::isfinite(farZ) || farZ <= 1 || m_cinematic.GetPresentation().active)
			throw std::invalid_argument(
				"Configure cinematics before playback with a resolver and valid farZ");
		for (const auto& [id, settings] : definitions) {
			if (id.empty()) throw std::invalid_argument("Cinematic ID must not be empty");
			settings.Validate();
			const auto validateModel = [&](const CinematicShotSettings& shot) {
				if (shot.target == "carrier") return;
				const auto asset = resolve(shot.model);
				if (!asset.model || !asset.model->GetLocalBounds() || !std::isfinite(asset.scale) ||
					asset.scale <= 0)
					throw std::invalid_argument(
						"Cinematic '" + id + "' requires loaded model: " + shot.model);
			};
			if (settings.shots.empty())
				validateModel(settings);
			else
				for (const auto& shot : settings.shots)
					validateModel(shot);
		}
		m_cinematics = definitions;
		m_resolveCinematicModel = std::move(resolve);
		m_cinematicFarZ = farZ;
		m_openingEnabled = definitions.contains("opening");
		if (m_openingEnabled) m_openingSettings = definitions.at("opening");
		m_midMissionPlayed = m_endingPlayed = false;
		m_reinforcementsPlayed = false;
		m_carrierLookSeconds = 0;
		m_patrolKills = 0;
		m_patrolEncouraged = m_patrolPraiseStarted = m_patrolPraiseComplete = false;
		m_carrierCommandDelay = 0;
	}
	void FlightScene::EnableOpening(const CinematicSettings& settings, std::shared_ptr<Engine::Model> model,
		float visualScale, float farZ) {
		m_cinematic = {};
		ConfigureCinematics(
			{{"opening", settings}},
			[model = std::move(model), visualScale](const std::string&) {
				return CinematicModel{model, visualScale};
			},
			farZ);
		StartOpening();
	}
	bool FlightScene::PlayCinematic(const std::string& id) {
		if (m_cinematic.GetPresentation().active) return false;
		const auto found = m_cinematics.find(id);
		if (found == m_cinematics.end()) return false;
		const auto needsCarrier = [](const CinematicShotSettings& shot) { return shot.target == "carrier"; };
		if ((needsCarrier(found->second) || std::any_of(found->second.shots.begin(),
			found->second.shots.end(), needsCarrier)) &&
			!m_mission.GetCarrierCenter(m_scene)) return false;
		if (id == "opening") {
			m_briefingEnabled = true;
			StartOpening();
		} else {
			BeginCinematic(found->second, id, true);
		}
		return true;
	}
	void FlightScene::InitializeCinematicShot() {
		const auto& shot = m_cinematic.GetCurrentShot();
		if (shot.target == "carrier") {
			UpdateCarrierCinematicCamera();
			return;
		}
		const auto asset = m_resolveCinematicModel(shot.model);
		auto origin = m_cinematicOrigin;
		origin.x += shot.originOffsetMeters.x;
		origin.y += shot.originOffsetMeters.y;
		origin.z += shot.originOffsetMeters.z;
		m_cinematicScene.Initialize(shot, asset.model, asset.scale, m_scene.GetEnvironment(), origin, m_width,
			m_height, m_cinematicFarZ);
	}
	void FlightScene::UpdateCarrierCinematicCamera() {
		using namespace DirectX;
		if (m_width == 0 || m_height == 0) return;
		const auto* ship = m_scene.FindObject(m_mission.GetCarrierId());
		const auto center = m_mission.GetCarrierCenter(m_scene);
		if (!ship || !center || !ship->model->GetLocalBounds()) return;
		const auto& bounds = *ship->model->GetLocalBounds();
		XMVECTOR minimum = XMVectorReplicate(FLT_MAX), maximum = XMVectorReplicate(-FLT_MAX);
		for (int corner = 0; corner < 8; ++corner) {
			const auto point = XMVector3TransformCoord(XMVectorSet(
				corner & 1 ? bounds.maximum.x : bounds.minimum.x,
				corner & 2 ? bounds.maximum.y : bounds.minimum.y,
				corner & 4 ? bounds.maximum.z : bounds.minimum.z, 1), XMLoadFloat4x4(&ship->worldTransform));
			minimum = XMVectorMin(minimum, point);
			maximum = XMVectorMax(maximum, point);
		}
		XMFLOAT3 dimensions;
		XMStoreFloat3(&dimensions, maximum - minimum);
		const float extent = std::max({dimensions.x, dimensions.y, dimensions.z, 1.0f});
		const auto& shot = m_cinematic.GetCurrentShot();
		// Carrier offsets are multiples of the world-space maximum ship dimension.
		const auto pose = m_cinematic.GetPose();
		XMFLOAT3 eye, target;
		XMStoreFloat3(&eye, XMLoadFloat3(&*center) + XMLoadFloat3(&pose.cameraOffsetMeters) * extent);
		XMStoreFloat3(&target, XMLoadFloat3(&*center) + XMLoadFloat3(&shot.lookAtOffsetMeters) * extent);
		const float aspect = float(m_width) / m_height;
		const float fov = 2 * std::atan(std::tan(XMConvertToRadians(shot.verticalFovDegrees) * 0.5f) *
			std::max(1.0f, (16.0f / 9) / aspect));
		Engine::Camera camera;
		camera.Initialize(fov, aspect, 1, m_cinematicFarZ);
		camera.LookAt(eye, target);
		m_scene.SetCamera(camera);
	}
	void FlightScene::BeginCinematic(
		const CinematicSettings& settings, const std::string& id, bool briefing) {
		m_activeCinematicSettings = settings;
		m_cinematicOrigin = m_player.GetPosition();
		m_cinematic.Start(settings, id);
		InitializeCinematicShot();
		if (m_cinematic.GetCurrentShot().target == "carrier") UpdateCarrierCinematicCamera();
		else m_cinematicScene.Update(m_cinematic.GetPose());
		if (briefing) m_cinematic.EnableBriefing();
		m_combat.cinematic = m_cinematic.GetPresentation();
		m_combat.audio.flying = m_combat.cinematic.briefingLine < 0;
		m_combat.audio.gunFiring = false;
	}
	void FlightScene::StartOpening() {
		UpdateCamera({}, 0, {});
		m_cameraController.SnapToFollowTarget();
		m_scene.SetCamera(m_cameraController.GetCamera());
		BeginCinematic(m_openingSettings, "opening", m_briefingEnabled);
	}
	void FlightScene::EnableOpeningBriefing() {
		m_briefingEnabled = true;
		m_cinematic.EnableBriefing();
		m_combat.cinematic = m_cinematic.GetPresentation();
		m_combat.audio.flying = m_combat.cinematic.briefingLine < 0;
	}
	void FlightScene::UpdateCinematicBriefing(CinematicVoiceStatus voice, float deltaSeconds) {
		m_cinematic.UpdateBriefing(voice, deltaSeconds);
		if (voice == CinematicVoiceStatus::Finished && m_cinematic.GetPresentation().radioLine >= 0 &&
			!m_cinematic.GetPresentation().active) {
			const auto id = m_cinematic.GetPresentation().cinematicId;
			if (id == "patrolComplete" && !m_patrolPraiseComplete) {
				m_patrolPraiseComplete = true;
				m_carrierCommandDelay = 0.5f;
			}
			if (id == "patrolComplete" || id == "patrolEncouragement") m_cinematic.StopRadio();
		}
		m_combat.cinematic = m_cinematic.GetPresentation();
	}
	void FlightScene::UpdateCinematic(float deltaSeconds, bool skipRequested) {
		if (m_cinematic.GetPresentation().briefingLine >= 0 && !skipRequested) {
			m_combat.audio.flying = false;
			return;
		}
		const auto before = m_cinematic.GetPresentation();
		m_cinematic.Update(deltaSeconds, skipRequested);
		const auto& after = m_cinematic.GetPresentation();
		if (after.active && before.shotIndex != after.shotIndex) InitializeCinematicShot();
		if (m_cinematic.GetCurrentShot().target == "carrier") UpdateCarrierCinematicCamera();
		else m_cinematicScene.Update(m_cinematic.GetPose());
		// 従来の開始演出のみ、暗転後の自機カットで無入力飛行を続ける。
		// 途中・終了・明示的なカット列は戦闘状態を一切進めない。
		const float flightSeconds =
			skipRequested || before.cinematicId != "opening" || !m_activeCinematicSettings.shots.empty()
			? 0.0f
			: std::max(0.0f,
				  after.elapsedSeconds -
					  std::max(before.elapsedSeconds, m_activeCinematicSettings.cutSeconds));
		if (flightSeconds > 0) {
			UpdatePlayer({}, flightSeconds);
			UpdateEnemies(flightSeconds);
			ApplyRenderStates();
			RefreshCombatSnapshot();
			UpdateCamera({}, flightSeconds, {});
		}
		if (!after.active) m_scene.SetCamera(m_cameraController.GetCamera());
		m_combat.cinematic = after;
		m_combat.audio.flying = after.active && after.briefingLine < 0;
	}
	bool FlightScene::StartCommandRadio(const std::string& id) {
		const auto found = m_cinematics.find(id);
		if (found == m_cinematics.end() || found->second.radio.empty()) return false;
		m_cinematic.StartRadio(found->second, id);
		m_combat.cinematic = m_cinematic.GetPresentation();
		return true;
	}
	void FlightScene::UpdatePatrolCommand(float dt) {
		if (m_combat.result != CombatResult::Active || m_cinematic.GetPresentation().active) return;
		const auto phase = m_mission.GetPhase();
		const bool speaking = m_cinematic.GetPresentation().radioLine >= 0;
		if (phase == MissionPhase::Patrol) {
			if (m_patrolKills >= 2 && !m_patrolEncouraged && !m_enemyRadio.IsBusy() && !speaking)
				m_patrolEncouraged = StartCommandRadio("patrolEncouragement");
			return;
		}
		if (phase != MissionPhase::CarrierAttack || m_midMissionPlayed) return;
		if (!m_patrolPraiseStarted) {
			if (m_enemyRadio.IsBusy() || speaking) return;
			m_patrolPraiseStarted = true;
			if (StartCommandRadio("patrolComplete")) return;
			// Configurations without the new dialogue retain their carrier introduction.
			m_patrolPraiseComplete = true;
		}
		if (!m_patrolPraiseComplete) {
			// Missing audio devices/files fall back to the subtitle duration.
			if (!speaking) { m_patrolPraiseComplete = true; m_carrierCommandDelay = 0.5f; }
			return;
		}
		m_carrierCommandDelay = std::max(0.0f, m_carrierCommandDelay - dt);
		if (m_carrierCommandDelay > 0) return;
		m_midMissionPlayed = true;
		m_carrierLookSeconds = 3;
		if (m_mission.GetCarrierCenter(m_scene)) StartCommandRadio("midMission");
	}
	void FlightScene::CheckMissionCinematics() {
		if (m_combat.result != CombatResult::Active && !m_endingPlayed) {
			m_endingPlayed = true;
			PlayCinematic(m_combat.result == CombatResult::Victory ? "ending" : "defeat");
		} else if (m_combat.result == CombatResult::Active &&
			m_mission.GetPhase() == MissionPhase::FinalFighters && !m_reinforcementsPlayed) {
			m_reinforcementsPlayed = true;
			m_carrierLookSeconds = 0;
			PlayCinematic("reinforcements");
		}
	}
} // namespace Game
