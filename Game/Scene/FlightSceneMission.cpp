#include "FlightScene.h"
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/ProjectileCollision.h"
#include <algorithm>
#include <cmath>
namespace Game {
	using namespace DirectX;
	namespace {
		constexpr float kAircraftGunDamage = 2;
	}
	void FlightScene::ConfigureMission(const MissionSettings& settings,
		std::shared_ptr<Engine::Model> enemyModel, float enemyScale, const AircraftFlightParameters& flight,
		const AircraftAIParameters& ai) {
		settings.Validate();
		if (!enemyModel || !m_enemies.empty() || m_initialState) {
			throw std::invalid_argument(
				"Configure mission with loaded models before adding enemies/updating");
		}
		m_mission.Configure(settings, std::move(enemyModel), enemyScale, flight, ai);
		m_carrierMissiles.ConfigureFlight(settings.carrierMissileSpeed, settings.bombDamage);
		SpawnMissionAircraft(settings.patrolCount, SquadronKind::Patrol);
	}
	void FlightScene::SpawnMissionAircraft(size_t count, SquadronKind kind) {
		for (auto& spawn : m_mission.BuildWave(m_scene, m_player.GetPosition(), count, kind,
				 m_combatSettings.enemyHealth, m_player.GetForward(), m_enemySequence)) {
			const auto id = AddEnemyAircraft(spawn.model, spawn.position, spawn.scale, spawn.flight, spawn.ai,
				spawn.health, std::move(spawn.name), spawn.facePlayer);
			if (spawn.heading) {
				auto& enemy = m_enemies.back();
				enemy.flightModel.SetHeading(*spawn.heading);
				if (auto* object = m_scene.FindObject(id)) {
					object->worldTransform = enemy.flightModel.GetWorldTransform();
				}
			}
		}
	}

	void FlightScene::UpdateMission(float dt) {
		if (m_mission.Advance(
				m_enemies.size(), m_combat.playerHealth > 0 && m_scene.FindObject(m_playerId), dt)) {
			switch (m_mission.GetPhase()) {
			case MissionPhase::FinalFighters:
				SpawnMissionAircraft(m_mission.GetSettings().finalFighterCount, SquadronKind::Fighters);
				break;
			case MissionPhase::Commander:
				SpawnMissionAircraft(1, SquadronKind::Commander);
				break;
			case MissionPhase::Complete:
				m_combat.result = CombatResult::Victory;
				m_combat.allDownSeconds = 3;
				break;
			case MissionPhase::Failed:
				m_combat.result = CombatResult::Defeat;
				break;
			default:
				break;
			}
		}
		m_mission.UpdateDeckSmoke(m_scene, m_effects, dt);
	}
	void FlightScene::ConfigureEnemyWeapons(const WeaponDefinition& definition,
		const MissionSettings& settings, std::shared_ptr<Engine::Model> ammunition) {
		settings.Validate();
		auto gun = definition.gun;
		gun.maxProjectiles = settings.maxHostileProjectiles;
		gun.lifetime = EnemyGunLifetimeSeconds;
		gun.muzzleSpeed = settings.enemyGunSpeed;
		gun.fireInterval = settings.enemyGunInterval;
		m_hostileProjectiles.Configure(gun, std::move(ammunition), definition.modelForward);
		m_enemyGunSettings = gun;
		m_commanderGunInterval = settings.commanderGunInterval;
	}

	void FlightScene::ConfigureCarrierMissileWeapon(
		const BombDefinition& definition, float reloadSeconds, std::shared_ptr<Engine::Model> model) {
		m_carrierMissiles.Configure(reloadSeconds, std::move(model), definition.modelForward);
		m_carrierMissiles.ConfigureFlight(
			m_mission.GetSettings().carrierMissileSpeed, m_mission.GetSettings().bombDamage);
	}

	void FlightScene::UpdateCarrierMissiles(const CombatInput& input, float dt) {
		const bool carrierAttackActive =
			m_mission.GetPhase() == MissionPhase::CarrierAttack && !m_mission.IsDeckDisabled();
		std::optional<CollisionTarget> hull;
		if (carrierAttackActive) {
			if (const auto* carrier = m_scene.FindObject(m_mission.GetCarrierId());
				carrier && carrier->model->GetLocalBounds()) {
				hull = CollisionTarget{carrier->id, *carrier->model->GetLocalBounds(),
					{carrier->worldTransform._41, carrier->worldTransform._42, carrier->worldTransform._43},
					carrier->worldTransform};
			}
		}
		m_carrierMissiles.Update(m_scene, dt, hull);
		for (const auto& position : m_carrierMissiles.TakeMissImpacts()) {
			m_effects.SpawnExplosion(position, {});
		}
		for (const auto& position : m_carrierMissiles.TakeTrail()) {
			m_effects.SpawnRocketSmoke(position, 0);
		}
		if (carrierAttackActive && m_scene.FindObject(m_playerId)) {
			const unsigned count = std::min(input.missilePressCount, m_carrierMissiles.GetAmmo());
			const auto target = m_targeting.GetState() == LockState::Locked &&
					m_targeting.GetSelected() == m_mission.GetCarrierId()
				? m_mission.GetCarrierId()
				: Engine::InvalidObjectId;
			for (unsigned i = 0; i < count; ++i) {
				if (m_carrierMissiles.TryLaunch(m_scene, m_player.GetPosition(), m_player.GetVelocity(),
						m_player.GetForward(), target)) {
					++m_combat.audio.missileLaunches;
				}
			}
		}
	}

	void FlightScene::UpdateHostileWeapons(float dt) {
		std::vector<CollisionTarget> playerTargets;
		if (const auto* player = m_scene.FindObject(m_playerId); player && player->model->GetLocalBounds()) {
			playerTargets.push_back({.id = m_playerId,
				.bounds = *player->model->GetLocalBounds(),
				.previousPosition = m_previousPlayerPosition,
				.world = player->worldTransform});
		}
		m_hostileProjectiles.Update(m_scene, dt, playerTargets);
		UpdateEnemyGuns();
		m_mission.UpdateAntiAircraftGuns(m_scene, m_player, m_hostileProjectiles, m_battleSeconds, dt);
		auto tracers = m_effects.GetSnapshot().gunTracers;
		auto hostileTracers = m_hostileProjectiles.TakeTracers();
		tracers.insert(tracers.end(), hostileTracers.begin(), hostileTracers.end());
		m_effects.SetGunTracers(std::move(tracers));
	}

	void FlightScene::UpdateEnemyGuns() {
		constexpr float kEnemyFireWarningDurationSeconds = 1.0f;
		for (auto& enemy : m_enemies) {
			if (!enemy.fire || enemy.weaponCooldownSeconds > 0) continue;
			const CombatTarget player{m_playerId, m_player.GetPosition(), m_player.GetVelocity()};
			const auto enemyVelocity = enemy.flightModel.GetVelocity();
			const auto direction = EnemyGunDirection(enemy.flightModel.GetPosition(), enemyVelocity, player,
				m_enemyGunSettings.muzzleSpeed, enemy.slot, enemy.gunPattern.GetShotIndex());
			if (!IsWithinAngle(
					enemy.flightModel.GetForward(), direction, enemy.aiParameters.aimAngleDegrees)) {
				continue;
			}
			// 2発分の空きがないときは発射せず、片方だけの斉射を避ける。
			if (m_hostileProjectiles.GetCount() + 2 > m_enemyGunSettings.maxProjectiles) continue;
			const auto directions =
				SpreadEnemyGunDirections(direction, enemy.slot, enemy.gunPattern.GetShotIndex());
			bool fired = false;
			for (const auto& shotDirection : directions) {
				XMFLOAT3 velocity;
				XMStoreFloat3(&velocity,
					XMLoadFloat3(&enemyVelocity) +
						XMLoadFloat3(&shotDirection) * m_enemyGunSettings.muzzleSpeed);
				fired |= m_hostileProjectiles.FireSingle(m_scene, enemy.flightModel.GetPosition(), velocity,
					AttackSource::EnemyAircraft, kAircraftGunDamage);
			}
			if (fired) {
				const float interval = m_mission.GetPhase() == MissionPhase::Commander
					? m_commanderGunInterval
					: m_enemyGunSettings.fireInterval;
				enemy.weaponCooldownSeconds = interval;
				enemy.gunPattern.NotifyShot();
				m_combat.enemyFireWarningSeconds = kEnemyFireWarningDurationSeconds;
				const auto enemyPosition = enemy.flightModel.GetPosition();
				const auto playerForward = m_player.GetForward();
				const auto toEnemy = XMLoadFloat3(&enemyPosition) - XMLoadFloat3(&player.center);
				if (XMVectorGetX(XMVector3Dot(toEnemy, XMLoadFloat3(&playerForward))) < 0) {
					m_combat.enemyRearFireWarningSeconds = kEnemyFireWarningDurationSeconds;
				}
			}
		}
	}
} // namespace Game
