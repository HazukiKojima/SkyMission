#include "FlightScene.h"
#include <algorithm>
#include <cmath>
#include "Game/Combat/GunAimSolver.h"
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

	void FlightScene::ConfigurePlayerWeapon(const WeaponDefinition& definition, std::shared_ptr<Engine::Model> ammunition) {
		try {
			const auto* player = m_scene.FindObject(m_playerId);
			if (!player)
				throw std::invalid_argument("player model must be added before configuring its weapon");
			m_gun.Configure(definition.gun, *player->model);
			m_projectiles.Configure(definition.gun, std::move(ammunition), definition.modelForward);
			m_weaponConfigured = true;
			m_gunSettings = definition.gun;
		} catch (const std::exception& error) {
			const auto path = definition.sourcePath.u8string();
			throw std::runtime_error(std::string(reinterpret_cast<const char*>(path.data()), path.size()) + ": " + error.what());
		}
	}

	void FlightScene::Update(const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput,
		WeaponInput weaponInput, bool nextTarget) {
		using namespace DirectX;
		if (!std::isfinite(dt) || dt <= 0)
			return;
		dt = std::min(dt, 0.25f);
		m_combat.hitSeconds = std::max(0.0f, m_combat.hitSeconds-dt);
		m_combat.destroyedSeconds = std::max(0.0f, m_combat.destroyedSeconds-dt);
		m_combat.allDownSeconds = std::max(0.0f, m_combat.allDownSeconds-dt);
		// 更新後の機体位置・姿勢をカメラと描画状態へ反映する
		UpdatePlayer(input, dt);
		UpdateEnemies(dt);
		ApplyRenderStates();
		RefreshCombatSnapshot();
		m_targeting.Update(m_combat.enemies, m_combat.muzzle, m_combat.forward, nextTarget, dt);
		RefreshCombatSnapshot();
		const auto targets = BuildCollisionTargets();
		m_projectiles.Update(m_scene, dt, targets);
		// 集合変更前に機体行列を値として保持し、弾追加で無効になるポインタを残さない。
		if (m_weaponConfigured) {
			const bool hasPlayer = m_scene.FindObject(m_playerId) != nullptr;
			const auto ages = m_gun.Update({weaponInput.fire && hasPlayer}, dt);
			const auto pose = m_player.GetWorldTransform();
			DirectX::XMFLOAT4X4 world;
			DirectX::XMStoreFloat4x4(&world,
				DirectX::XMMatrixScaling(m_playerVisualScale, m_playerVisualScale, m_playerVisualScale) *
				DirectX::XMLoadFloat4x4(&pose));
			AimAssistance assistance;
			assistance.angleDegrees = m_combatSettings.targeting.assistAngleDegrees;
			if (m_combat.lockState == LockState::Locked) {
				for (const auto& target : m_combat.enemies)
					if (target.id == m_combat.selected && DirectX::XMVectorGetX(DirectX::XMVector3Length(
						DirectX::XMLoadFloat3(&target.center)-DirectX::XMLoadFloat3(&m_combat.muzzle))) <= m_combatSettings.targeting.acquireDistance)
						assistance.target = target;
			}
			for (const float age : ages)
				m_projectiles.FireVolley(m_scene, m_gun.GetMuzzles(), world, pose,
					m_player.GetForward(), m_player.GetVelocity(), age, assistance, targets, dt);
		}
		ApplyHits();
		RefreshCombatSnapshot();
		m_targeting.Update(m_combat.enemies, m_combat.muzzle, m_combat.forward, false, 0);
		RefreshCombatSnapshot();
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
			enemy.previousPosition = enemy.flightModel.GetPosition();
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
		const AircraftAIParameters& ai, float health, std::string name) {
		if (!model)
			return Engine::InvalidObjectId;
		EnemyAircraft enemy;
		enemy.flightModel.SetFlightParameters(flight);
		enemy.flightModel.SetPosition(position);
		enemy.aiParameters = ai;
		enemy.visualScale = scale;
		enemy.objectId = m_scene.AddObject(std::move(model));
		enemy.previousPosition = position;
		enemy.health = health;
		++m_enemySequence;
		enemy.name = name.empty() ? "ENEMY " + std::to_string(m_enemySequence) : std::move(name);
		const auto id = enemy.objectId;
		m_enemies.push_back(std::move(enemy));
		return id;
	}

	void FlightScene::ConfigureCombat(const CombatSettings& settings) {
		if (!std::isfinite(settings.enemyHealth) || settings.enemyHealth <= 0 ||
			!std::isfinite(settings.bulletDamage) || settings.bulletDamage <= 0)
			throw std::invalid_argument("enemyHealth and bulletDamage must be positive and finite");
		m_targeting.Configure(settings.targeting);
		m_combatSettings = settings;
	}

	std::vector<CollisionTarget> FlightScene::BuildCollisionTargets() {
		std::vector<CollisionTarget> targets;
		for (const auto& enemy : m_enemies)
			if (const auto* object = m_scene.FindObject(enemy.objectId); object && object->model->GetLocalBounds())
				targets.push_back({enemy.objectId, *object->model->GetLocalBounds(), enemy.previousPosition, object->worldTransform});
		return targets;
	}

	void FlightScene::RefreshCombatSnapshot() {
		using namespace DirectX;
		m_combat.enemies.clear();
		for (const auto& enemy : m_enemies) {
			const auto* object = m_scene.FindObject(enemy.objectId);
			if (!object) continue;
			const auto bounds = object->model->GetLocalBounds();
			const XMFLOAT3 center = bounds ? bounds->Center() : XMFLOAT3{};
			XMFLOAT3 worldCenter;
			XMStoreFloat3(&worldCenter, XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&object->worldTransform)));
			m_combat.enemies.push_back({enemy.objectId, worldCenter, enemy.flightModel.GetVelocity(), enemy.name, enemy.health});
		}
		m_combat.forward = m_player.GetForward();
		m_combat.muzzle = m_player.GetPosition();
		if (m_weaponConfigured) {
			XMVECTOR center = XMVectorZero();
			for (const auto& muzzle : m_gun.GetMuzzles()) center += XMLoadFloat3(&muzzle)*0.25f;
			const auto pose = m_player.GetWorldTransform();
			XMStoreFloat3(&m_combat.muzzle, XMVector3TransformCoord(center,
				XMMatrixScaling(m_playerVisualScale, m_playerVisualScale, m_playerVisualScale)*XMLoadFloat4x4(&pose)));
		}
		m_combat.selected = m_targeting.GetSelected();
		m_combat.lockState = m_targeting.GetState();
		m_combat.acquireProgress = m_targeting.GetProgress();
		m_combat.aim.reset(); m_combat.assistReady = false;
		for (const auto& target : m_combat.enemies) if (target.id == m_combat.selected && m_weaponConfigured) {
			m_combat.aim = SolveGunAim(m_combat.muzzle, m_player.GetVelocity(), target, m_gunSettings.muzzleSpeed, m_gunSettings.lifetime);
			m_combat.assistReady = m_combat.lockState == LockState::Locked && m_combat.aim &&
				XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.center)-XMLoadFloat3(&m_combat.muzzle))) <= m_combatSettings.targeting.acquireDistance &&
				IsWithinAngle(m_combat.forward, m_combat.aim->direction, m_combatSettings.targeting.assistAngleDegrees);
		}
	}

	void FlightScene::ApplyHits() {
		auto hits = m_projectiles.TakeHits();
		std::stable_sort(hits.begin(), hits.end(), [](const HitEvent& a, const HitEvent& b) { return a.frameSeconds < b.frameSeconds; });
		std::vector<Engine::ObjectId> destroyed;
		for (const auto& hit : hits) {
			auto enemy = std::find_if(m_enemies.begin(), m_enemies.end(), [&](const EnemyAircraft& e) { return e.objectId == hit.target; });
			if (enemy == m_enemies.end() || enemy->health <= 0) continue;
			enemy->health -= m_combatSettings.bulletDamage;
			m_combat.hitSeconds = 0.15f;
			if (enemy->health <= 0) { destroyed.push_back(enemy->objectId); m_combat.destroyedSeconds = 0.8f; }
		}
		for (const auto id : destroyed) RemoveObject(id);
		if (!destroyed.empty() && m_enemies.empty()) m_combat.allDownSeconds = 3;
	}

	void FlightScene::RemoveObject(Engine::ObjectId id) {
		m_scene.RemoveObject(id);
		std::erase_if(m_enemies, [id](const EnemyAircraft& e) { return e.objectId == id; });
		if (m_playerId == id)
			m_playerId = Engine::InvalidObjectId;
	}
} // namespace Game
