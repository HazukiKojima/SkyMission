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

	void FlightScene::ConfigureEnemyWeapon(const GunSettings& settings,
		const std::array<DirectX::XMFLOAT3, 4>& muzzles, std::shared_ptr<Engine::Model> ammunition,
		const DirectX::XMFLOAT3& modelForward) {
		m_enemyProjectiles.Configure(settings, std::move(ammunition), modelForward);
		m_enemyGunSettings = settings;
		m_enemyMuzzles = muzzles;
		m_enemyWeaponConfigured = true;
	}

	void FlightScene::ConfigurePlayerWeapon(
		const WeaponDefinition& definition, std::shared_ptr<Engine::Model> ammunition) {
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
			throw std::runtime_error(
				std::string(reinterpret_cast<const char*>(path.data()), path.size()) + ": " + error.what());
		}
	}

	void FlightScene::Update(const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput,
		CombatInput combatInput) {
		using namespace DirectX;
		m_combat.firedVolleys = 0;
		m_combat.hitCount = 0;
		if (!std::isfinite(dt) || dt <= 0)
			return;
		dt = std::min(dt, 0.25f);
		m_combat.playerHitSeconds = std::max(0.0f, m_combat.playerHitSeconds - dt);
		m_combat.hitSeconds = std::max(0.0f, m_combat.hitSeconds - dt);
		m_combat.destroyedSeconds = std::max(0.0f, m_combat.destroyedSeconds - dt);
		m_combat.allDownSeconds = std::max(0.0f, m_combat.allDownSeconds - dt);
		if (!m_initialState)
			m_initialState = std::make_shared<FlightScene>(*this);
		if (m_combat.result != CombatResult::Active) {
			if (combatInput.retry) {
				const auto initial = m_initialState;
				const auto width = m_width, height = m_height;
				*this = *initial;
				m_initialState = initial;
				OnResize(width, height);
				ApplyRenderStates();
				RefreshCombatSnapshot();
				UpdateCamera({}, dt, {});
			} else
				m_effects.Update(dt);
			return;
		}
		m_battleSeconds += dt;
		m_previousPlayerPosition = m_player.GetPosition();

		// 既存の演出を進めてから、このフレームの新しい演出を生成する
		m_effects.Update(dt);

		// 更新後の機体位置・姿勢をカメラと描画状態へ反映する
		UpdatePlayer(input, dt);
		UpdateEnemies(dt);
		ApplyRenderStates();
		RefreshCombatSnapshot();
		// 更新後の位置を使ってロック状態を決め、両武器で同じ衝突対象を共有する
		m_targeting.Update(m_combat.enemies, m_combat.muzzle, m_combat.forward, combatInput.nextTarget, dt);
		RefreshCombatSnapshot();
		const auto targets = BuildCollisionTargets();
		m_projectiles.Update(m_scene, dt, targets);
		// 集合変更前に機体行列を値として保持し、弾追加で無効になるポインタを残さない
		if (m_weaponConfigured) {
			const bool hasPlayer = m_scene.FindObject(m_playerId) != nullptr;
			const auto ages = m_gun.Update({combatInput.weapon.fire && hasPlayer}, dt);
			const auto pose = m_player.GetWorldTransform();
			DirectX::XMFLOAT4X4 world;
			DirectX::XMStoreFloat4x4(&world,
				DirectX::XMMatrixScaling(m_playerVisualScale, m_playerVisualScale, m_playerVisualScale) *
					DirectX::XMLoadFloat4x4(&pose));
			AimAssistance assistance;
			assistance.angleDegrees = m_combatSettings.targeting.assistAngleDegrees;
			if (m_combat.lockState == LockState::Locked) {
				for (const auto& target : m_combat.enemies)
					if (target.id == m_combat.selected &&
						DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMLoadFloat3(&target.center) -
							DirectX::XMLoadFloat3(&m_combat.muzzle))) <=
							m_combatSettings.targeting.acquireDistance)
						assistance.target = target;
			}
			for (const float age : ages)
				if (m_projectiles.FireVolley(m_scene, m_gun.GetMuzzles(), world, pose, m_player.GetForward(),
						m_player.GetVelocity(), age, assistance, targets, dt))
					++m_combat.firedVolleys;
		}
		UpdateGunEffects();
		UpdateRockets(combatInput, dt, targets);

		// 命中と撃破を反映してから、HUDとカメラへ最終状態を渡す
		UpdateEnemyWeapons(dt);
		ApplyHits();
		RefreshCombatSnapshot();
		m_targeting.Update(m_combat.enemies, m_combat.muzzle, m_combat.forward, false, 0);
		RefreshCombatSnapshot();
		UpdateCamera(input, dt, cameraInput);
		ApplyRenderStates();
	}

	void FlightScene::UpdateGunEffects() {
		for (const auto& shot : m_projectiles.TakeShotEvents())
			m_effects.SpawnGunShot(shot);
		m_effects.SetGunTracers(m_projectiles.TakeTracers());
		const auto* aircraft = m_scene.FindObject(m_playerId);
		if (!m_weaponConfigured || !aircraft) {
			m_effects.ClearGunFlashes();
			return;
		}
		// 閃光だけを現在の銃口に追従させ、放出した煙と実弾は独立して動かす
		std::array<DirectX::XMFLOAT3, 4> positions{};
		for (size_t index = 0; index < positions.size(); ++index) {
			DirectX::XMStoreFloat3(&positions[index],
				DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&m_gun.GetMuzzles()[index]),
					DirectX::XMLoadFloat4x4(&aircraft->worldTransform)));
		}
		m_effects.SetGunMuzzlePose(positions, m_player.GetForward());
	}

	void FlightScene::ConfigurePlayerRocket(
		const RocketDefinition& definition, std::shared_ptr<Engine::Model> model) {
		try {
			const auto* aircraft = m_scene.FindObject(m_playerId);
			if (!aircraft)
				throw std::invalid_argument("player model must be added before configuring rocket weapon");
			m_rocketLauncher.Configure(definition.settings, *aircraft->model);
			m_rockets.Configure(definition.settings, std::move(model), definition.modelForward);
			m_rocketSettings = definition.settings;
			m_rocketConfigured = true;
		} catch (const std::exception& error) {
			const auto path = definition.sourcePath.u8string();
			throw std::runtime_error(
				std::string(reinterpret_cast<const char*>(path.data()), path.size()) + ": " + error.what());
		}
	}

	void FlightScene::UpdateRockets(
		const CombatInput& input, float dt, std::span<const CollisionTarget> targets) {
		if (!m_rocketConfigured)
			return;
		// 入力を判定する前に経過時間を反映し、クールタイム終了直後の押下を保持する
		m_rocketLauncher.Update(dt);
		if (const auto* aircraft = m_scene.FindObject(m_playerId)) {
			// オブジェクト追加で集合が変わるため、銃口変換は値で保持する
			const auto world = aircraft->worldTransform;
			const auto target =
				m_combat.lockState == LockState::Locked ? m_combat.selected : Engine::InvalidObjectId;
			for (unsigned press = 0; press < std::min(input.rocketPressCount, 2u); ++press) {
				const auto muzzle = m_rocketLauncher.GetNextMuzzle();
				if (!muzzle)
					break;
				if (m_rockets.Fire(
						m_scene, *muzzle, world, m_player.GetForward(), m_player.GetVelocity(), target))
					m_rocketLauncher.CommitShot();
			}
		}
		m_combat.rocket = m_rocketLauncher.GetState();
		m_rockets.Update(m_scene, dt, targets, m_combat.enemies);
		for (const auto& sample : m_rockets.TakeTrail())
			m_effects.SpawnRocketSmoke(sample.position, sample.age);
		m_effects.SetRocketExhausts(m_rockets.GetExhausts());
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
			const AircraftAITarget target{
				m_player.GetPosition(), m_player.GetVelocity(), m_player.GetForward()};
			const CombatTarget combatTarget{
				m_playerId, target.position, target.velocity, "PLAYER", m_combat.playerHealth};
			const auto aim = SolveGunAim(enemy.flightModel.GetPosition(), enemy.flightModel.GetVelocity(),
				combatTarget, m_enemyGunSettings.muzzleSpeed, m_enemyGunSettings.lifetime);
			const auto position = enemy.flightModel.GetPosition();
			DirectX::XMFLOAT3 direction;
			DirectX::XMStoreFloat3(&direction,
				DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(
					DirectX::XMLoadFloat3(&target.position), DirectX::XMLoadFloat3(&position))));
			if (aim)
				direction = aim->direction;
			const size_t reserveSlot = static_cast<size_t>(m_battleSeconds / 6) % m_enemies.size();
			const bool allowed = m_enemies.size() <= 2 || &enemy != &m_enemies[reserveSlot];
			const auto decision = enemy.controller.ReadCombatInput(
				enemy.flightModel, target, direction, dt, enemy.aiParameters, enemy.slot, allowed);
			const auto input = decision.flight;
			enemy.fire = decision.fire && aim.has_value();
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
		m_width = width;
		m_height = height;
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
		enemy.slot = m_enemySequence;
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
			!std::isfinite(settings.bulletDamage) || settings.bulletDamage <= 0 ||
			!std::isfinite(settings.playerHealth) || settings.playerHealth <= 0 ||
			!std::isfinite(settings.enemyBulletDamage) || settings.enemyBulletDamage <= 0)
			throw std::invalid_argument("Combat health and damage must be positive and finite");
		m_targeting.Configure(settings.targeting);
		m_combatSettings = settings;
		m_combat.playerHealth = m_combat.playerMaxHealth = settings.playerHealth;
	}

	std::vector<CollisionTarget> FlightScene::BuildCollisionTargets() {
		std::vector<CollisionTarget> targets;
		for (const auto& enemy : m_enemies)
			if (const auto* object = m_scene.FindObject(enemy.objectId);
				object && object->model->GetLocalBounds())
				targets.push_back({enemy.objectId, *object->model->GetLocalBounds(), enemy.previousPosition,
					object->worldTransform});
		return targets;
	}

	void FlightScene::RefreshCombatSnapshot() {
		using namespace DirectX;
		m_combat.enemies.clear();
		for (const auto& enemy : m_enemies) {
			const auto* object = m_scene.FindObject(enemy.objectId);
			if (!object)
				continue;
			const auto bounds = object->model->GetLocalBounds();
			const XMFLOAT3 center = bounds ? bounds->Center() : XMFLOAT3{};
			XMFLOAT3 worldCenter;
			XMStoreFloat3(&worldCenter,
				XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&object->worldTransform)));
			m_combat.enemies.push_back(
				{enemy.objectId, worldCenter, enemy.flightModel.GetVelocity(), enemy.name, enemy.health});
		}
		m_combat.forward = m_player.GetForward();
		m_combat.muzzle = m_player.GetPosition();
		if (m_weaponConfigured) {
			XMVECTOR center = XMVectorZero();
			for (const auto& muzzle : m_gun.GetMuzzles())
				center += XMLoadFloat3(&muzzle) * 0.25f;
			const auto pose = m_player.GetWorldTransform();
			XMStoreFloat3(&m_combat.muzzle,
				XMVector3TransformCoord(center,
					XMMatrixScaling(m_playerVisualScale, m_playerVisualScale, m_playerVisualScale) *
						XMLoadFloat4x4(&pose)));
		}
		m_combat.selected = m_targeting.GetSelected();
		m_combat.lockState = m_targeting.GetState();
		m_combat.acquireProgress = m_targeting.GetProgress();
		m_combat.aim.reset();
		m_combat.assistReady = false;
		for (const auto& target : m_combat.enemies)
			if (target.id == m_combat.selected && m_weaponConfigured) {
				m_combat.aim = SolveGunAim(m_combat.muzzle, m_player.GetVelocity(), target,
					m_gunSettings.muzzleSpeed, m_gunSettings.lifetime);
				m_combat.assistReady = m_combat.lockState == LockState::Locked && m_combat.aim &&
					XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.center) -
						XMLoadFloat3(&m_combat.muzzle))) <= m_combatSettings.targeting.acquireDistance &&
					IsWithinAngle(m_combat.forward, m_combat.aim->direction,
						m_combatSettings.targeting.assistAngleDegrees);
			}
		m_combat.gunImpactPoint.reset();
		if (m_weaponConfigured) {
			// 一定距離の照準位置は、対象選択や予測補正とは独立して計算する
			const auto direction = XMLoadFloat3(&m_combat.forward);
			const auto aircraftVelocity = m_player.GetVelocity();
			const auto velocity = XMLoadFloat3(&aircraftVelocity) + direction * m_gunSettings.muzzleSpeed;
			constexpr float distance = CombatSnapshot::gunSightDistance;
			const float speed = XMVectorGetX(XMVector3Length(velocity));
			if (speed > 0.001f) {
				float seconds = std::min(distance / speed, m_gunSettings.lifetime);
				const float vy = XMVectorGetY(velocity);
				if (vy < 0)
					seconds = std::min(seconds, std::max(0.0f, -m_combat.muzzle.y / vy));
				XMFLOAT3 point;
				XMStoreFloat3(&point, XMLoadFloat3(&m_combat.muzzle) + velocity * seconds);
				m_combat.gunImpactPoint = point;
			}
		}
	}

	void FlightScene::UpdateEnemyWeapons(float dt) {
		if (!m_enemyWeaponConfigured)
			return;
		std::vector<CollisionTarget> targets;
		if (const auto* player = m_scene.FindObject(m_playerId); player && player->model->GetLocalBounds())
			targets.push_back({m_playerId, *player->model->GetLocalBounds(), m_previousPlayerPosition,
				player->worldTransform});
		m_enemyProjectiles.Update(m_scene, dt, targets);
		for (auto& enemy : m_enemies) {
			if (!enemy.fire || targets.empty()) {
				enemy.gunCooldown = std::max(0.0f, enemy.gunCooldown - dt);
				continue;
			}
			const auto pose = enemy.flightModel.GetWorldTransform();
			// 銃口は機体ローカルのメートル指定。発射後の弾は直進する。
			for (size_t volley = 0; enemy.gunCooldown <= dt && volley < m_enemyGunSettings.maxProjectiles / 4;
				++volley) {
				const float age = dt - enemy.gunCooldown;
				m_enemyProjectiles.FireVolley(m_scene, m_enemyMuzzles, pose, pose,
					enemy.flightModel.GetForward(), enemy.flightModel.GetVelocity(), age, {}, targets, dt);
				enemy.gunCooldown += m_enemyGunSettings.fireInterval;
			}
			enemy.gunCooldown = std::max(0.0f, enemy.gunCooldown - dt);
		}
	}

	void FlightScene::ApplyHits() {
		// 武器ごとの命中をまとめ、フレーム内の接触時刻順にダメージを反映する
		auto hits = m_projectiles.TakeHits();
		const auto enemyHits = m_enemyProjectiles.TakeHits();
		hits.insert(hits.end(), enemyHits.begin(), enemyHits.end());
		const auto rocketHits = m_rockets.TakeHits();
		hits.insert(hits.end(), rocketHits.begin(), rocketHits.end());
		std::stable_sort(hits.begin(), hits.end(), [](const HitEvent& a, const HitEvent& b) {
			return a.frameSeconds < b.frameSeconds;
		});
		std::vector<Engine::ObjectId> destroyed;
		for (const auto& hit : hits) {
			if (hit.target == m_playerId) {
				if (m_combat.playerHealth <= 0)
					continue;
				m_combat.playerHealth =
					std::max(0.0f, m_combat.playerHealth - m_combatSettings.enemyBulletDamage);
				m_combat.playerHitSeconds = 0.7f;
				m_combat.playerHitDirection = {-hit.direction.x, -hit.direction.y, -hit.direction.z};
				m_effects.SpawnImpact(hit, m_player.GetVelocity());
				if (m_combat.playerHealth <= 0) {
					if (const auto* object = m_scene.FindObject(m_playerId))
						m_effects.SpawnExplosion(*object, m_player.GetVelocity(), m_playerVisualScale);
				}
				continue;
			}
			auto enemy = std::find_if(m_enemies.begin(), m_enemies.end(), [&](const EnemyAircraft& e) {
				return e.objectId == hit.target;
			});
			if (enemy == m_enemies.end() || enemy->health <= 0) {
				if (hit.kind == ProjectileKind::Rocket)
					m_effects.SpawnExplosion(hit.position, {});
				continue;
			}

			enemy->health -=
				hit.kind == ProjectileKind::Rocket ? m_rocketSettings.damage : m_combatSettings.bulletDamage;
			++m_combat.hitCount;
			if (hit.kind == ProjectileKind::Gun)
				m_effects.SpawnImpact(hit, enemy->flightModel.GetVelocity());
			else if (enemy->health > 0)
				m_effects.SpawnExplosion(hit.position, enemy->flightModel.GetVelocity());
			m_combat.hitSeconds = 0.15f;
			if (enemy->health <= 0) {
				if (const auto* aircraft = m_scene.FindObject(enemy->objectId))
					m_effects.SpawnExplosion(*aircraft, enemy->flightModel.GetVelocity(), enemy->visualScale);
				else
					m_effects.SpawnExplosion(
						enemy->flightModel.GetPosition(), enemy->flightModel.GetVelocity());
				destroyed.push_back(enemy->objectId);
				m_combat.destroyedSeconds = 0.8f;
			}
		}
		// 命中の走査中は敵を削除せず、後続の命中でも耐久値を参照できるようにする
		for (const auto id : destroyed)
			RemoveObject(id);
		if (m_combat.playerHealth <= 0) {
			RemoveObject(m_playerId);
			m_combat.result = CombatResult::Defeat;
		} else if (!destroyed.empty() && m_enemies.empty())
			m_combat.result = CombatResult::Victory;
		if (!destroyed.empty() && m_enemies.empty())
			m_combat.allDownSeconds = 3;
	}

	void FlightScene::RemoveObject(Engine::ObjectId id) {
		m_scene.RemoveObject(id);
		std::erase_if(m_enemies, [id](const EnemyAircraft& e) {
			return e.objectId == id;
		});
		if (m_playerId == id) {
			m_playerId = Engine::InvalidObjectId;
			m_effects.ClearGunFlashes();
		}
	}
} // namespace Game
