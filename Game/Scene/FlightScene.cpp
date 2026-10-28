#include "FlightScene.h"
#include <algorithm>
#include <cmath>
namespace Game {
	namespace {
		constexpr float kPropellerAngularSpeedPerSpeed = 0.06f;
		constexpr float kMaximumAnimationFrameSeconds = 0.25f;
		float AdvancePropeller(float angle, float speed, float dt) {
			// 飛行モデルと同じ時間上限を使う（角度はラジアン、時間は秒）
			return std::fmod(
				angle + speed * kPropellerAngularSpeedPerSpeed * std::min(dt, kMaximumAnimationFrameSeconds),
				DirectX::XM_2PI);
		}

		void ApplyAircraftPose(
			Engine::SceneObject& object, const AircraftFlightModel& aircraft, float scale, float angle) {
			const auto transform = aircraft.GetWorldTransform();
			DirectX::XMStoreFloat4x4(&object.worldTransform,
				DirectX::XMMatrixScaling(scale, scale, scale) * DirectX::XMLoadFloat4x4(&transform));
			object.nodeRotationRadians = angle;
		}
	} // namespace
	void FlightScene::Initialize(float aspect, float nearZ, float farZ) {
		m_cameraController.Initialize(DirectX::XM_PIDIV4, aspect, nearZ, farZ);
		m_scene.SetCamera(m_cameraController.GetCamera());
	}

	void FlightScene::Update(const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput) {
		if (!std::isfinite(dt) || dt <= 0)
			return;
		// 更新後の機体位置・姿勢をカメラと描画状態へ反映する
		UpdatePlayer(input, dt);
		UpdateEnemies(dt);
		UpdateCamera(input, dt, cameraInput);
		ApplyRenderStates();
	}

	void FlightScene::UpdatePlayer(const AircraftInput& input, float dt) {
		if (!m_scene.FindObject(m_playerId))
			return;
		m_player.Update(input, dt);
		m_playerPropellerAngle = AdvancePropeller(m_playerPropellerAngle, m_player.GetSpeed(), dt);
	}

	void FlightScene::UpdateEnemies(float dt) {
		for (auto& enemy : m_enemies) {
			if (!m_scene.FindObject(enemy.objectId))
				continue;
			const auto input = enemy.controller.ReadInput(
				enemy.flightModel, m_player.GetMaximumSpeed(), dt, enemy.aiParameters);
			enemy.flightModel.Update(input, dt);
			enemy.propellerAngle = AdvancePropeller(enemy.propellerAngle, enemy.flightModel.GetSpeed(), dt);
		}
	}

	void FlightScene::UpdateCamera(
		const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput) {
		if (m_scene.FindObject(m_playerId))
			m_cameraController.SetFollowTarget({.position = m_player.GetPosition(),
				.heading = m_player.GetHeading(),
				.forward = m_player.GetForward(),
				.pitchInputActive = input.pitch > 0.01f || input.pitch < -0.01f,
				.speed = m_player.GetSpeed(),
				.maximumSpeed = m_player.GetMaximumSpeed()});
		m_cameraController.Update(dt, cameraInput);
		m_scene.SetCamera(m_cameraController.GetCamera());
	}

	void FlightScene::ApplyRenderStates() {
		if (auto* object = m_scene.FindObject(m_playerId))
			ApplyAircraftPose(*object, m_player, m_playerVisualScale, m_playerPropellerAngle);
		for (const auto& enemy : m_enemies)
			if (auto* object = m_scene.FindObject(enemy.objectId))
				ApplyAircraftPose(*object, enemy.flightModel, enemy.visualScale, enemy.propellerAngle);
	}

	void FlightScene::OnResize(UINT width, UINT height) {
		m_cameraController.OnResize(width, height);
		m_scene.SetCamera(m_cameraController.GetCamera());
	}

	Engine::ObjectId FlightScene::AddPlayerObject(std::shared_ptr<Engine::Model> model) {
		m_playerId = m_scene.AddObject(std::move(model));
		m_playerPropellerAngle = 0;
		return m_playerId;
	}

	Engine::ObjectId FlightScene::AddEnemyAircraft(std::shared_ptr<Engine::Model> model,
		const DirectX::XMFLOAT3& position, float scale, const AircraftFlightParameters& flight,
		const AircraftAIParameters& ai) {
		if (!model)
			return Engine::InvalidObjectId;
		EnemyAircraft enemy;
		enemy.flightModel.SetFlightParameters(flight);
		enemy.flightModel.SetPosition(position);
		enemy.aiParameters = ai;
		enemy.visualScale = scale;
		enemy.objectId = m_scene.AddObject(std::move(model));
		const auto id = enemy.objectId;
		m_enemies.push_back(std::move(enemy));
		return id;
	}

	void FlightScene::RemoveObject(Engine::ObjectId id) {
		m_scene.RemoveObject(id);
		std::erase_if(m_enemies, [id](const EnemyAircraft& e) { return e.objectId == id; });
		if (m_playerId == id)
			m_playerId = Engine::InvalidObjectId;
	}
} // namespace Game
