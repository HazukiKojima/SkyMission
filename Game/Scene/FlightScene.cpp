#include "FlightScene.h"
#include "FlightSceneLoader.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/CombatSnapshotBuilder.h"
#include "Game/Combat/ProjectileCollision.h"
#include "Game/Combat/EnemyEngagementProfile.h"
namespace Game {
	namespace {

		void ApplyAircraftPose(
			Engine::SceneObject& object, const AircraftFlightModel& aircraft, float scale, float angle) {
			const auto transform = aircraft.GetWorldTransform();
			DirectX::XMStoreFloat4x4(&object.worldTransform,
				DirectX::XMMatrixScaling(scale, scale, scale) * DirectX::XMLoadFloat4x4(&transform));
			object.nodeRotationRadians = angle;
		}
	} // namespace
	FlightScene FlightScene::CreateMission(const FlightSceneDefinition& assets,
		const std::function<std::shared_ptr<Engine::Model>(const std::string&)>& resolveModel, UINT width,
		UINT height, float farZ) {
		if (!resolveModel || width == 0 || height == 0 || !std::isfinite(farZ) || farZ <= 1) {
			throw std::invalid_argument("Mission requires a model resolver and valid viewport/clip range");
		}
		if (assets.ships.size() < 3) {
			throw std::invalid_argument("Mission requires a carrier and two escorts");
		}
		const auto requireModel = [&](const std::string& id) {
			auto model = resolveModel(id);
			if (!model || !model->IsLoaded()) {
				throw std::invalid_argument("Mission requires loaded model: " + id);
			}
			return model;
		};
		FlightScene scene;
		scene.ConfigureCombat(assets.combat);
		scene.Initialize(static_cast<float>(width) / static_cast<float>(height), 1.0f, farZ);
		Engine::SceneEnvironment environment;
		environment.sunDirection = assets.environment.sky.sunDirection;
		environment.sunIntensity = assets.environment.sky.sunIntensity;
		environment.sunColor = assets.environment.sky.sunColor;
		environment.ambientIntensity = assets.environment.sky.ambientIntensity;
		environment.ambientColor = assets.environment.sky.ambientColor;
		scene.SetEnvironment(environment);
		scene.m_text = assets.text;
		scene.m_playerAnimation = assets.playerAircraft.animation;
		scene.m_enemyAnimation = assets.enemyAircraft.animation;
		scene.m_mission.ConfigureText(assets.text);
		scene.m_enemyGunTuning = assets.enemyGunTuning;
		scene.m_enemyAITuning = assets.enemyAITuning;
		scene.m_effects.Configure(assets.effects);
		scene.m_projectiles.ConfigureEffects(assets.effects);
		scene.m_hostileProjectiles.ConfigureEffects(assets.effects);
		scene.m_cameraController.Configure(assets.camera);
		scene.m_cameraController.Initialize(
			DirectX::XMConvertToRadians(assets.camera.fovDegrees), float(width) / height, 1.0f, farZ);
		scene.SetPlayerAircraftPosition(assets.mission.playerStartPosition);
		scene.SetPlayerAircraftVisualScale(assets.playerAircraft.visualScale);
		scene.SetPlayerAircraftFlightParameters(assets.playerAircraft.flightParameters);
		scene.AddPlayerObject(requireModel(PlayerModelId));
		scene.ConfigureMission(assets.mission, requireModel(EnemyModelId), assets.enemyAircraft.visualScale,
			assets.enemyAircraft.flightParameters, assets.enemyAIParameters);
		scene.ConfigurePlayerWeapon(assets.playerWeapon, requireModel(AmmunitionModelId));
		scene.ConfigureEnemyWeapons(assets.playerWeapon, assets.mission, requireModel(AmmunitionModelId));
		scene.ConfigureCarrierMissileWeapon(
			assets.bomb, assets.mission.bombReloadSeconds, requireModel(BombModelId));
		for (size_t i = 0; i < assets.ships.size(); ++i) {
			const auto& ship = assets.ships[i];
			const auto id = scene.AddObject(requireModel(ship.asset.id), ship.BuildTransform());
			scene.RegisterMissionShip(id, i == 0);
		}
		scene.OnResize(width, height);
		scene.ApplyRenderStates();
		scene.RefreshCombatSnapshot();
		// 初回Updateの有無によらず、完成時点を再挑戦の基準にする。
		scene.SaveInitialState();
		return scene;
	}

	void FlightScene::Initialize(float aspect, float nearZ, float farZ) {
		m_cameraController.Initialize(AircraftCameraController::DefaultFovY, aspect, nearZ, farZ);
		m_scene.SetCamera(m_cameraController.GetCamera());
	}

	void FlightScene::ConfigurePlayerWeapon(
		const WeaponDefinition& definition, std::shared_ptr<Engine::Model> ammunition) {
		try {
			const auto* player = m_scene.FindObject(m_playerId);
			if (!player) {
				throw std::invalid_argument("player model must be added before configuring its weapon");
			}
			m_gun.Configure(definition.gun, *player->model);
			m_projectiles.Configure(definition.gun, ammunition, definition.modelForward);
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
		m_combat.audio = {};
		m_combat.firedVolleys = 0;
		m_combat.hitCount = 0;
		if (!std::isfinite(dt) || dt <= 0) return;
		dt = std::min(dt, 0.25f);
		if (!m_initialState) {
			SaveInitialState();
		}
		if (m_cinematic.GetPresentation().active) {
			UpdateCinematic(dt, combatInput.skipOpening);
			UpdateEnemyRadio(dt);
			return;
		}
		m_cinematic.Update(dt, false);
		m_combat.cinematic = m_cinematic.GetPresentation();
		UpdateCombatTimers(dt);
		if (m_combat.result != CombatResult::Active) {
			if (combatInput.retry) {
				Restart(dt);
			} else {
				UpdateEnemyRadio(dt);
				m_effects.Update(dt);
				m_mission.UpdateDeckSmoke(m_scene, m_effects, dt);
			}
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
		UpdateWeapons(combatInput, dt);

		// 命中と撃破を反映してから、HUDとカメラへ最終状態を渡す
		ApplyHits();
		UpdateMission(dt);
		ApplyRenderStates();
		RefreshCombatSnapshot();
		m_combat.audio.flying = m_combat.result == CombatResult::Active && m_scene.FindObject(m_playerId);
		m_combat.audio.gunFiring &= m_combat.audio.flying;
		UpdateCamera(input, dt, cameraInput);
		CheckMissionCinematics();
		UpdateEnemyRadio(dt);
	}

	void FlightScene::UpdateCombatTimers(float dt) {
		m_combat.enemyFireWarningSeconds = std::max(0.0f, m_combat.enemyFireWarningSeconds - dt);
		m_combat.enemyRearFireWarningSeconds = std::max(0.0f, m_combat.enemyRearFireWarningSeconds - dt);
		m_combat.playerHitSeconds = std::max(0.0f, m_combat.playerHitSeconds - dt);
		m_combat.hitSeconds = std::max(0.0f, m_combat.hitSeconds - dt);
		m_combat.destroyedSeconds = std::max(0.0f, m_combat.destroyedSeconds - dt);
		m_combat.allDownSeconds = std::max(0.0f, m_combat.allDownSeconds - dt);
	}

	void FlightScene::UpdateWeapons(const CombatInput& input, float dt) {
		const auto geometry = BuildCombatGeometry();
		UpdateTargeting(geometry, input.nextTarget, dt);
		const auto targets = BuildCollisionTargets();
		m_projectiles.Update(m_scene, dt, targets);
		FirePlayerGun(input, dt, geometry, targets);
		UpdateGunEffects();
		UpdateCarrierMissiles(input, dt);
		UpdateHostileWeapons(dt);
	}

	void FlightScene::FirePlayerGun(const CombatInput& input, float dt, const CombatGeometry& geometry,
		std::span<const CollisionTarget> targets) {
		using namespace DirectX;
		if (!m_weaponConfigured) return;
		// 弾追加でシーン内のポインタが無効になるため、機体行列は値として保持する。
		const bool hasPlayer = m_scene.FindObject(m_playerId) != nullptr;
		m_combat.audio.gunFiring = input.weapon.fire && hasPlayer;
		const auto ages = m_gun.Update({input.weapon.fire && hasPlayer}, dt);
		const auto pose = m_player.GetWorldTransform();
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world,
			XMMatrixScaling(m_playerVisualScale, m_playerVisualScale, m_playerVisualScale) *
				XMLoadFloat4x4(&pose));
		AimAssistance assistance;
		assistance.angleDegrees = m_combatSettings.targeting.assistAngleDegrees;
		if (m_targeting.GetState() == LockState::Locked) {
			const auto selectedTarget = m_targeting.GetSelected();
			const float acquireDistance = m_combatSettings.targeting.acquireDistance;
			for (const auto& target : geometry.enemies) {
				if (target.id != selectedTarget) continue;
				const auto offset = XMLoadFloat3(&target.center) - XMLoadFloat3(&geometry.muzzle);
				const float distance = XMVectorGetX(XMVector3Length(offset));
				if (distance <= acquireDistance) {
					assistance.target = target;
				}
			}
		}
		for (const float age : ages) {
			if (m_projectiles.FireVolley(m_scene,
					{.muzzles = m_gun.GetMuzzles(),
						.aircraftWorld = world,
						.aircraftPose = pose,
						.forward = m_player.GetForward(),
						.aircraftVelocity = m_player.GetVelocity(),
						.ageSeconds = age,
						.assistance = assistance,
						.targets = targets,
						.frameSeconds = dt,
						.damage = m_combatSettings.bulletDamage})) {
				++m_combat.firedVolleys;
			}
		}
	}

	void FlightScene::UpdateGunEffects() {
		for (const auto& shot : m_projectiles.TakeShotEvents()) {
			m_effects.SpawnGunShot(shot);
		}
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

	void FlightScene::UpdatePlayer(const AircraftInput& input, float dt) {
		if (!m_scene.FindObject(m_playerId)) return;
		m_player.Update(input, dt);
		m_playerPropellerAngle =
			m_playerAnimation.AdvancePropeller(m_playerPropellerAngle, m_player.GetSpeed(), dt);
	}

	void FlightScene::UpdateTargeting(const CombatGeometry& geometry, bool next, float dt) {
		if (m_mission.GetPhase() == MissionPhase::CarrierAttack && !m_mission.IsDeckDisabled()) {
			if (auto center = m_mission.GetCarrierCenter(m_scene)) {
				const CombatTarget carrier{m_mission.GetCarrierId(), *center, {}, m_text.Text("敵空母"),
					m_mission.GetCarrierHealth()};
				m_targeting.UpdateObjective(carrier, geometry.muzzle, geometry.forward, dt);
				return;
			}
		}
		m_targeting.Update(geometry.enemies, geometry.muzzle, geometry.forward, next, dt);
	}

	void FlightScene::UpdateEnemies(float dt) {
		for (auto& enemy : m_enemies) {
			if (!m_scene.FindObject(enemy.objectId)) continue;
			const AircraftAITarget target{
				m_player.GetPosition(), m_player.GetVelocity(), m_player.GetForward()};
			const auto position = enemy.flightModel.GetPosition();
			DirectX::XMFLOAT3 direction;
			DirectX::XMStoreFloat3(&direction,
				DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(
					DirectX::XMLoadFloat3(&target.position), DirectX::XMLoadFloat3(&position))));
			{
				const CombatTarget playerTarget{m_playerId, target.position, target.velocity};
				const auto aim = SolveGunAim(position, enemy.flightModel.GetVelocity(), playerTarget,
					m_enemyGunSettings.muzzleSpeed, m_enemyGunSettings.lifetime);
				if (aim) {
					direction = aim->direction;
				}
			}
			enemy.weaponCooldownSeconds = std::max(0.0f, enemy.weaponCooldownSeconds - dt);
			const bool allowed = true;
			const auto engagement = GetEnemyEngagementProfile(
				m_targeting.GetState(), m_targeting.GetSelected(), enemy.objectId, m_enemyAITuning);
			const auto aiParameters = engagement.Apply(enemy.aiParameters);
			const bool gunWindow =
				enemy.gunPattern.Update(dt * engagement.burstTempoScale, enemy.slot, m_enemyGunTuning);
			enemy.controller.ConfigureTuning(m_enemyAITuning);
			const auto decision = enemy.controller.ReadCombatInput(enemy.flightModel, target, direction, dt,
				aiParameters, enemy.slot, allowed, enemy.weaponCooldownSeconds <= 0 && gunWindow,
				m_mission.GetDefenseCenter(m_scene), true);
			const auto input = decision.flight;
			enemy.fire = decision.fire;
			enemy.previousPosition = enemy.flightModel.GetPosition();
			enemy.flightModel.Update(input, dt);
			enemy.propellerAngle =
				m_enemyAnimation.AdvancePropeller(enemy.propellerAngle, enemy.flightModel.GetSpeed(), dt);
		}
	}

	void FlightScene::UpdateCamera(
		const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput) {
		if (m_scene.FindObject(m_playerId)) {
			m_cameraController.SetFollowTarget({.position = m_player.GetPosition(),
				.heading = m_player.GetHeading(),
				.forward = m_player.GetForward(),
				.pitchInputActive = input.pitch > 0.01f || input.pitch < -0.01f,
				.speed = m_player.GetSpeed(),
				.maximumSpeed = m_player.GetMaximumSpeed()});
		}
		m_cameraController.Update(dt, cameraInput);
		m_scene.SetCamera(m_cameraController.GetCamera());
	}

	void FlightScene::ApplyRenderStates() {
		if (auto* object = m_scene.FindObject(m_playerId)) {
			ApplyAircraftPose(*object, m_player, m_playerVisualScale, m_playerPropellerAngle);
		}
		for (const auto& enemy : m_enemies) {
			if (auto* object = m_scene.FindObject(enemy.objectId)) {
				ApplyAircraftPose(*object, enemy.flightModel, enemy.visualScale, enemy.propellerAngle);
			}
		}
	}

	void FlightScene::OnResize(UINT width, UINT height) {
		m_width = width;
		m_height = height;
		m_cameraController.OnResize(width, height);
		m_cinematicScene.OnResize(width, height);
		m_scene.SetCamera(m_cameraController.GetCamera());
		if (m_cinematic.GetPresentation().active && m_cinematic.GetCurrentShot().target == "carrier")
			UpdateCarrierCinematicCamera();
	}

	Engine::ObjectId FlightScene::AddPlayerObject(std::shared_ptr<Engine::Model> model) {
		m_playerId = m_scene.AddObject(std::move(model));
		m_playerPropellerAngle = 0;
		return m_playerId;
	}

	Engine::ObjectId FlightScene::AddEnemyAircraft(std::shared_ptr<Engine::Model> model,
		const DirectX::XMFLOAT3& position, float scale, const AircraftFlightParameters& flight,
		const AircraftAIParameters& ai, float health, std::string name, bool facePlayer) {
		if (!model) return Engine::InvalidObjectId;
		EnemyAircraft enemy;
		enemy.flightModel.SetFlightParameters(flight);
		enemy.flightModel.SetPosition(position);
		if (facePlayer) {
			const auto target = m_player.GetPosition();
			enemy.flightModel.SetHeading(
				{target.x - position.x, target.y - position.y, target.z - position.z});
		}
		enemy.aiParameters = ai;
		enemy.slot = m_enemySequence;
		enemy.visualScale = scale;
		enemy.objectId = m_scene.AddObject(std::move(model));
		enemy.previousPosition = position;
		enemy.health = health;
		++m_enemySequence;
		enemy.name = name.empty() ? m_text.Text("敵機 ") + std::to_string(m_enemySequence) : std::move(name);
		const auto id = enemy.objectId;
		m_enemies.push_back(std::move(enemy));
		return id;
	}

	void FlightScene::ConfigureCombat(const CombatSettings& settings) {
		if (!std::isfinite(settings.enemyHealth) || settings.enemyHealth <= 0 ||
			!std::isfinite(settings.bulletDamage) || settings.bulletDamage <= 0 ||
			!std::isfinite(settings.playerHealth) || settings.playerHealth <= 0) {
			throw std::invalid_argument("Combat health and damage must be positive and finite");
		}
		m_targeting.Configure(settings.targeting);
		m_combatSettings = settings;
		m_combat.playerHealth = m_combat.playerMaxHealth = settings.playerHealth;
	}

	std::vector<CollisionTarget> FlightScene::BuildCollisionTargets() {
		std::vector<CollisionTarget> targets;
		for (const auto& enemy : m_enemies) {
			if (const auto* object = m_scene.FindObject(enemy.objectId);
				object && object->model->GetLocalBounds()) {
				targets.push_back({.id = enemy.objectId,
					.bounds = *object->model->GetLocalBounds(),
					.previousPosition = enemy.previousPosition,
					.world = object->worldTransform});
			}
		}
		return targets;
	}

	FlightScene::CombatGeometry FlightScene::BuildCombatGeometry() {
		CombatGeometry geometry;
		using namespace DirectX;
		for (const auto& enemy : m_enemies) {
			const auto* object = m_scene.FindObject(enemy.objectId);
			if (!object) continue;
			const auto bounds = object->model->GetLocalBounds();
			const XMFLOAT3 center = bounds ? bounds->Center() : XMFLOAT3{};
			XMFLOAT3 worldCenter;
			XMStoreFloat3(&worldCenter,
				XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&object->worldTransform)));
			geometry.enemies.push_back({.id = enemy.objectId,
				.center = worldCenter,
				.velocity = enemy.flightModel.GetVelocity(),
				.name = enemy.name,
				.health = enemy.health,
				.heading = enemy.flightModel.GetHeading()});
		}
		geometry.forward = m_player.GetForward();
		geometry.muzzle = m_player.GetPosition();
		if (m_weaponConfigured) {
			XMVECTOR center = XMVectorZero();
			for (const auto& muzzle : m_gun.GetMuzzles()) {
				center += XMLoadFloat3(&muzzle) * 0.25f;
			}
			const auto pose = m_player.GetWorldTransform();
			XMStoreFloat3(&geometry.muzzle,
				XMVector3TransformCoord(center,
					XMMatrixScaling(m_playerVisualScale, m_playerVisualScale, m_playerVisualScale) *
						XMLoadFloat4x4(&pose)));
		}
		return geometry;
	}

	void FlightScene::RefreshCombatSnapshot() {
		m_combat.elapsedSeconds = m_battleSeconds;
		m_combat.missionEnabled = true;
		m_combat.carrierPresent = m_scene.FindObject(m_mission.GetCarrierId()) != nullptr;
		m_combat.carrierHealth = m_mission.GetCarrierHealth();
		m_combat.carrierMaxHealth = m_mission.GetCarrierMaxHealth();
		m_combat.carrierMissileAmmo = m_carrierMissiles.GetAmmo();
		m_combat.carrierMissileCapacity = m_carrierMissiles.GetCapacity();
		m_combat.missionPhase = m_mission.GetPhase();
		m_combat.deckDisabled = m_mission.IsDeckDisabled();
		m_combat.missileCooldownSeconds = m_carrierMissiles.GetCooldownSeconds();
		m_combat.missionMessageSeconds = m_mission.GetMessageSeconds();
		m_combat.missileImpactPoint.reset();
		if (const auto deck = GetDeckTarget()) {
			const auto center = deck->bounds.Center();
			DirectX::XMStoreFloat3(&m_combat.objectivePosition,
				DirectX::XMVector3TransformCoord(
					DirectX::XMLoadFloat3(&center), DirectX::XMLoadFloat4x4(&deck->world)));
			const auto launch = m_carrierMissiles.BuildLaunchFlight(
				m_player.GetPosition(), m_player.GetVelocity(), m_player.GetForward());
			m_combat.missileImpactPoint = launch.Predict(m_combat.objectivePosition.y);
		}
		if (m_mission.GetPhase() == MissionPhase::FinalFighters ||
			m_mission.GetPhase() == MissionPhase::Commander) {
			m_combat.objectivePosition = m_mission.GetAirCombatPosition();
		}
		using namespace DirectX;
		auto geometry = BuildCombatGeometry();
		m_combat.enemies = geometry.enemies;
		m_combat.muzzle = geometry.muzzle;
		m_combat.forward = geometry.forward;
		// 命中・増援後の対象を再検証する。捕捉時間はここでは進めない。
		UpdateTargeting(geometry, false, 0);
		m_combat.selected = m_targeting.GetSelected();
		m_combat.lockState = m_targeting.GetState();
		m_combat.acquireProgress = m_targeting.GetProgress();
		BuildCombatSnapshot(m_combat, {m_player, m_gunSettings, m_combatSettings, m_weaponConfigured});
	}

	void FlightScene::ApplyHits() {
		// 武器ごとの命中をまとめ、フレーム内の接触時刻順にダメージを反映する
		auto hits = m_projectiles.TakeHits();
		const auto missileHits = m_carrierMissiles.TakeHits();
		hits.insert(hits.end(), missileHits.begin(), missileHits.end());
		auto hostileHits = m_hostileProjectiles.TakeHits();
		hits.insert(hits.end(), hostileHits.begin(), hostileHits.end());
		std::stable_sort(hits.begin(), hits.end(), [](const HitEvent& a, const HitEvent& b) {
			return a.frameSeconds < b.frameSeconds;
		});
		std::vector<Engine::ObjectId> destroyed;
		for (const auto& hit : hits) {
			if (hit.target == m_mission.GetCarrierId() &&
				(hit.kind == ProjectileKind::Rocket || hit.kind == ProjectileKind::Bomb)) {
				if (m_mission.ApplyCarrierDamage(hit.damage)) {
					if (m_mission.IsDeckDisabled()) {
						++m_combat.audio.shipExplosions;
						if (auto center = m_mission.GetCarrierCenter(m_scene)) {
							m_effects.SpawnCarrierExplosion(*center);
						}
					} else {
						m_effects.SpawnExplosion(hit.position, {});
					}
				}
				continue;
			}
			if (hit.target == m_playerId && m_playerId != Engine::InvalidObjectId) {
				if (m_combat.playerHealth <= 0) continue;
				m_combat.playerHealth = std::max(0.0f, m_combat.playerHealth - hit.damage);
				if (hit.kind == ProjectileKind::Gun) {
					++m_combat.audio.gunHits;
				} else {
					++m_combat.audio.missileHits;
				}
				m_combat.playerHitSeconds = m_combatSettings.feedback.playerHitSeconds;
				m_combat.playerHitDirection = {-hit.direction.x, -hit.direction.y, -hit.direction.z};
				m_effects.SpawnExplosion(hit.position, m_player.GetVelocity());
				if (m_combat.playerHealth <= 0) {
					++m_combat.audio.aircraftExplosions;
					if (const auto* object = m_scene.FindObject(m_playerId)) {
						m_effects.SpawnExplosion(*object, m_player.GetVelocity(), m_playerVisualScale);
					}
				}
				continue;
			}
			auto enemy = std::find_if(m_enemies.begin(), m_enemies.end(), [&](const EnemyAircraft& e) {
				return e.objectId == hit.target;
			});
			if (enemy == m_enemies.end() || enemy->health <= 0) continue;

			enemy->health -= hit.damage;
			++m_combat.hitCount;
			if (hit.kind == ProjectileKind::Gun) {
				m_effects.SpawnImpact(hit, enemy->flightModel.GetVelocity());
			} else if (enemy->health > 0) {
				m_effects.SpawnExplosion(hit.position, enemy->flightModel.GetVelocity());
			}
			m_combat.hitSeconds = m_combatSettings.feedback.hitSeconds;
			if (enemy->health <= 0) {
				++m_combat.audio.aircraftExplosions;
				if (const auto* aircraft = m_scene.FindObject(enemy->objectId)) {
					m_effects.SpawnExplosion(*aircraft, enemy->flightModel.GetVelocity(), enemy->visualScale);
				} else {
					m_effects.SpawnExplosion(
						enemy->flightModel.GetPosition(), enemy->flightModel.GetVelocity());
				}
				destroyed.push_back(enemy->objectId);
				m_combat.destroyedSeconds = m_combatSettings.feedback.destroyedSeconds;
			}
		}
		// 命中の走査中は敵を削除せず、後続の命中でも耐久値を参照できるようにする
		for (const auto id : destroyed) {
			RemoveObject(id);
		}
		std::vector<Engine::ObjectId> survivors;
		for (const auto& enemy : m_enemies)
			if (enemy.health > 0 && m_scene.FindObject(enemy.objectId)) survivors.push_back(enemy.objectId);
		for (const auto id : destroyed) m_enemyRadio.NotifyDestroyed(id, survivors);
		if (m_combat.playerHealth <= 0) {
			RemoveObject(m_playerId);
			m_combat.result = CombatResult::Defeat;
		}
	}

	void FlightScene::UpdateEnemyRadio(float dt) {
		std::vector<Engine::ObjectId> survivors;
		for (const auto& enemy : m_enemies)
			if (enemy.health > 0 && m_scene.FindObject(enemy.objectId)) survivors.push_back(enemy.objectId);
		const auto& movie = m_combat.cinematic;
		m_enemyRadio.Update(dt, survivors, m_combat.result == CombatResult::Defeat ||
			movie.briefingLine >= 0 || movie.radioLine >= 0);
		m_combat.enemyRadio = m_enemyRadio.GetPresentation();
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
