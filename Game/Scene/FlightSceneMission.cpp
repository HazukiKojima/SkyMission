#include "FlightScene.h"
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/ProjectileCollision.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;

	void FlightScene::ConfigureMission(const MissionSettings& settings,
		std::shared_ptr<Engine::Model> enemyModel, float enemyScale, const AircraftFlightParameters& flight,
		const AircraftAIParameters& ai, std::shared_ptr<Engine::Model> bombModel) {
		settings.Validate();
		if (!enemyModel || !bombModel || !m_enemies.empty() || m_initialState)
			throw std::invalid_argument(
				"Configure mission with loaded models before adding enemies/updating");
		m_missionSettings = settings;
		m_mission = MissionState(settings.carrierHealth);
		m_missionEnemyModel = std::move(enemyModel);
		m_bombModel = std::move(bombModel);
		m_missionEnemyFlight = flight;
		m_missionEnemyAI = ai;
		m_missionEnemyScale = enemyScale;
		m_missionEnabled = true;
		SpawnMissionAircraft(settings.patrolCount, false, true);
	}

	void FlightScene::RegisterMissionShip(Engine::ObjectId id, bool carrier) {
		const auto* ship = m_scene.FindObject(id);
		if (!ship || !ship->model->GetLocalBounds())
			throw std::invalid_argument("Mission ship must have loaded bounds");
		if (carrier) {
			m_carrierId = id;
			m_defenseCenter = GetCarrierCenter();
		} else {
			m_escortIds.push_back(id);
			m_aaCooldowns.push_back(2 + static_cast<float>(m_escortIds.size()) * 0.4f);
		}
	}

	std::optional<CollisionTarget> FlightScene::GetDeckTarget() {
		const auto* carrier = m_scene.FindObject(m_carrierId);
		if (!carrier || !carrier->model->GetLocalBounds())
			return std::nullopt;
		auto bounds = *carrier->model->GetLocalBounds();
		const auto center = bounds.Center();
		const float halfX =
			(bounds.maximum.x - bounds.minimum.x) * m_missionSettings.deckWidthFraction * 0.5f;
		const float halfZ =
			(bounds.maximum.z - bounds.minimum.z) * m_missionSettings.deckLengthFraction * 0.5f;
		const float height =
			bounds.minimum.y + (bounds.maximum.y - bounds.minimum.y) * m_missionSettings.deckHeightFraction;
		const float thickness = (bounds.maximum.y - bounds.minimum.y) * 0.015f;
		bounds.minimum = {center.x - halfX, height - thickness, center.z - halfZ};
		bounds.maximum = {center.x + halfX, height + thickness, center.z + halfZ};
		return CollisionTarget{m_carrierId, bounds,
			{carrier->worldTransform._41, carrier->worldTransform._42, carrier->worldTransform._43},
			carrier->worldTransform};
	}

	void FlightScene::SpawnMissionAircraft(size_t count, bool commander, bool opening) {
		const auto player = m_player.GetPosition();
		XMFLOAT3 away{0, 0, 1};
		if (const auto* carrier = m_scene.FindObject(m_carrierId)) {
			away = {player.x - carrier->worldTransform._41, 0, player.z - carrier->worldTransform._43};
			if (std::hypot(away.x, away.z) < 1)
				away = {0, 0, 1};
			XMStoreFloat3(&away, XMVector3Normalize(XMLoadFloat3(&away)));
		}
		m_airCombatPosition = {
			player.x + away.x * 8500, std::max(1000.0f, player.y), player.z + away.z * 8500};
		if (m_defenseCenter)
			m_airCombatPosition = {m_defenseCenter->x + away.x * 6500,
				std::max(1000.0f, player.y), m_defenseCenter->z + away.z * 6500};
		const auto nose = m_player.GetForward();
		XMFLOAT3 approach{nose.x, 0, nose.z};
		if (std::hypot(approach.x, approach.z) < 0.001f) approach = {0, 0, -1};
		XMStoreFloat3(&approach, XMVector3Normalize(XMLoadFloat3(&approach)));
		static constexpr float depth[] = {-600, 1000, -1300, 500, -200, 1500};
		static constexpr float altitude[] = {150, 650, -250, 1000, 350, -100};
		for (size_t i = 0; i < count; ++i) {
			const float lateral = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * 1500;
			const size_t formationSlot = i % std::size(depth);
			const float height = std::max(m_missionEnemyAI.lowAltitude + 350,
				(opening ? player.y : m_airCombatPosition.y) + altitude[formationSlot]);
			const float range = 5500 + depth[formationSlot];
			// Stagger all three axes instead of launching a single flat row.
			XMFLOAT3 position = opening
				? XMFLOAT3{player.x + approach.x * range - approach.z * lateral, height,
					player.z + approach.z * range + approach.x * lateral}
				: XMFLOAT3{m_airCombatPosition.x + away.z * lateral - away.x * depth[formationSlot] * 0.5f,
					height, m_airCombatPosition.z - away.x * lateral - away.z * depth[formationSlot] * 0.5f};
			auto ai = m_missionEnemyAI;
			// More firing opportunities without changing the shared flight policy.
			ai.aimAngleDegrees = 10;
			ai.aimHoldSeconds = 0.08f;
			ai.openingGraceSeconds += EnemyVariation(m_enemySequence, 0, 79) * 1.5f;
			const auto id = AddEnemyAircraft(m_missionEnemyModel, position, m_missionEnemyScale,
				m_missionEnemyFlight, ai,
				m_combatSettings.enemyHealth * (commander ? m_missionSettings.commanderHealthMultiplier : 1),
				commander ? "SQUADRON LEADER" : (opening ? "PATROL " : "FIGHTER ") + std::to_string(i + 1),
				opening);
			if (!opening) {
				// Arrive from the fleet side, heading out to the marked combat area.
				auto& enemy = m_enemies.back();
				enemy.flightModel.SetHeading(away);
				if (auto* object = m_scene.FindObject(id))
					object->worldTransform = enemy.flightModel.GetWorldTransform();
			}
		}
	}

	void FlightScene::UpdateMission(float dt) {
		m_missionMessageSeconds = std::max(0.0f, m_missionMessageSeconds - dt);
		if (m_mission.Advance(
				m_enemies.size(), m_combat.playerHealth > 0 && m_scene.FindObject(m_playerId))) {
			m_missionMessageSeconds = 6;
			switch (m_mission.GetPhase()) {
			case MissionPhase::FinalFighters:
				SpawnMissionAircraft(m_missionSettings.finalFighterCount, false);
				break;
			case MissionPhase::Commander:
				SpawnMissionAircraft(1, true);
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
		UpdateCarrierSmoke(dt);
	}

	std::optional<XMFLOAT3> FlightScene::GetCarrierCenter() {
		const auto* carrier = m_scene.FindObject(m_carrierId);
		if (!carrier || !carrier->model->GetLocalBounds())
			return std::nullopt;
		const auto center = carrier->model->GetLocalBounds()->Center();
		XMFLOAT3 point;
		XMStoreFloat3(
			&point, XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&carrier->worldTransform)));
		return point;
	}

	void FlightScene::UpdateCarrierSmoke(float dt) {
		if (!m_missionEnabled || !m_mission.IsDeckDisabled())
			return;
		m_deckSmokeSeconds -= dt;
		if (m_deckSmokeSeconds <= 0) {
			if (const auto center = GetCarrierCenter())
				m_effects.SpawnDeckSmoke(*center);
			m_deckSmokeSeconds = CarrierEffectSettings::smokeIntervalSeconds;
		}
	}

	void FlightScene::UpdateBombs(const CombatInput& input, float dt) {
		if (m_bombCooldown > 0) {
			m_bombCooldown = std::max(0.0f, m_bombCooldown - dt);
			if (m_bombCooldown <= 0) m_bombAmmo = 2;
		}
		std::optional<CollisionTarget> deck;
		if (const auto* carrier = m_scene.FindObject(m_carrierId); carrier && carrier->model->GetLocalBounds())
			deck = CollisionTarget{m_carrierId, *carrier->model->GetLocalBounds(),
				{carrier->worldTransform._41, carrier->worldTransform._42, carrier->worldTransform._43},
				carrier->worldTransform};
		std::erase_if(m_bombs, [&](Bomb& bomb) {
			const auto start = bomb.flight.position;
			if (bomb.targetId == m_carrierId && !m_mission.IsDeckDisabled()) {
				if (const auto center = GetCarrierCenter()) {
					XMFLOAT3 desired;
					XMStoreFloat3(&desired, XMLoadFloat3(&*center) - XMLoadFloat3(&start));
					bomb.flight.velocity = RocketSystem::Steer(bomb.flight.velocity, desired, XM_PIDIV2 * dt);
				} else bomb.targetId = Engine::InvalidObjectId;
			}
			const auto seaTime = bomb.flight.TimeToHeight(0);
			const float valid = std::min({dt, bomb.life, seaTime.value_or(dt)});
			bomb.flight.Advance(valid);
			bomb.life -= dt;
			bomb.smokeSeconds -= dt;
			if (bomb.smokeSeconds <= 0) {
				m_effects.SpawnRocketSmoke(bomb.flight.position, 0);
				bomb.smokeSeconds = 0.06f;
			}
			if (deck && m_mission.GetPhase() == MissionPhase::CarrierAttack && !m_mission.IsDeckDisabled()) {
				if (const auto contact = IntersectMovingTarget(start, bomb.flight.position, *deck, 0, 1)) {
					XMFLOAT3 point;
					XMStoreFloat3(&point,
						XMVectorLerp(XMLoadFloat3(&start), XMLoadFloat3(&bomb.flight.position), *contact));
					m_bombHits.push_back({m_carrierId, valid * *contact, point, {0, -1, 0},
						ProjectileKind::Rocket, AttackSource::Player, m_missionSettings.bombDamage});
					m_scene.RemoveObject(bomb.id);
					return true;
				}
			}
			if (bomb.flight.position.y <= 0.001f || bomb.life <= 0) {
				m_effects.SpawnExplosion(bomb.flight.position, {});
				m_scene.RemoveObject(bomb.id);
				return true;
			}
			if (auto* object = m_scene.FindObject(bomb.id)) {
				const auto& p = bomb.flight.position;
				const auto& v = bomb.flight.velocity;
				// The existing ordnance asset points along local +X.
				XMStoreFloat4x4(&object->worldTransform,
					XMMatrixRotationZ(std::atan2(v.y, std::hypot(v.x, v.z))) *
						XMMatrixRotationY(std::atan2(-v.z, v.x)) * XMMatrixTranslation(p.x, p.y, p.z));
			}
			return false;
		});
		if (m_bombAmmo > 0 && m_bombCooldown <= 0 && m_scene.FindObject(m_playerId) &&
			m_mission.GetPhase() == MissionPhase::CarrierAttack && !m_mission.IsDeckDisabled()) {
			const unsigned count = std::min(input.bombPressCount, m_bombAmmo);
			for (unsigned i = 0; i < count; ++i) {
				auto position = m_player.GetPosition();
				const auto forward = m_player.GetForward(), inherited = m_player.GetVelocity();
				const float side = m_bombAmmo == 2 ? -14.0f : 14.0f;
				position.x += -forward.z * side;
				position.y -= 12;
				position.z += forward.x * side;
				XMFLOAT3 velocity;
				XMStoreFloat3(&velocity, XMLoadFloat3(&inherited) +
					XMLoadFloat3(&forward) * m_missionSettings.carrierMissileSpeed);
				XMFLOAT4X4 transform;
				XMStoreFloat4x4(&transform,
					XMMatrixRotationZ(std::atan2(velocity.y, std::hypot(velocity.x, velocity.z))) *
					XMMatrixRotationY(std::atan2(-velocity.z, velocity.x)) *
					XMMatrixTranslation(position.x, position.y, position.z));
				const auto targetId = m_targeting.GetState() == LockState::Locked &&
					m_targeting.GetSelected() == m_carrierId ? m_carrierId : Engine::InvalidObjectId;
				m_bombs.push_back({m_scene.AddObject(m_bombModel, transform), {position, velocity},
					8, 0, targetId});
				--m_bombAmmo;
				++m_combat.audio.missileLaunches;
			}
			if (count > 0 && m_bombAmmo == 0) m_bombCooldown = m_missionSettings.bombReloadSeconds;
		}
	}

	void FlightScene::UpdateMissionWeapons(float dt) {
		std::vector<CollisionTarget> playerTargets;
		if (const auto* player = m_scene.FindObject(m_playerId); player && player->model->GetLocalBounds())
			playerTargets.push_back({m_playerId, *player->model->GetLocalBounds(), m_previousPlayerPosition,
				player->worldTransform});
		m_hostileProjectiles.Update(m_scene, dt, playerTargets);
		for (auto& enemy : m_enemies) {
			if (!enemy.fire || enemy.missileCooldown > 0)
				continue;
			const CombatTarget player{m_playerId, m_player.GetPosition(), m_player.GetVelocity()};
			const auto enemyVelocity = enemy.flightModel.GetVelocity();
			const auto direction = EnemyGunDirection(enemy.flightModel.GetPosition(), enemyVelocity,
				player, m_missionSettings.enemyGunSpeed, enemy.slot, enemy.gunPattern.GetShotIndex());
			if (!IsWithinAngle(enemy.flightModel.GetForward(), direction, enemy.aiParameters.aimAngleDegrees))
				continue;
			XMFLOAT3 velocity;
			XMStoreFloat3(&velocity, XMLoadFloat3(&enemyVelocity) +
				XMLoadFloat3(&direction) * m_missionSettings.enemyGunSpeed);
			if (m_hostileProjectiles.FireSingle(
					m_scene, enemy.flightModel.GetPosition(), velocity, AttackSource::EnemyAircraft, 5)) {
				const float interval = m_mission.GetPhase() == MissionPhase::Commander
					? m_missionSettings.commanderGunInterval : m_missionSettings.enemyGunInterval;
				enemy.missileCooldown = enemy.gunPattern.Interval(interval, enemy.slot);
				enemy.gunPattern.NotifyShot();
			}
		}
		for (size_t i = 0; i < m_escortIds.size(); ++i) {
			m_aaCooldowns[i] = std::max(0.0f, m_aaCooldowns[i] - dt);
			const auto* ship = m_scene.FindObject(m_escortIds[i]);
			if (!ship || m_aaCooldowns[i] > 0)
				continue;
			XMFLOAT3 origin{
				ship->worldTransform._41, ship->worldTransform._42 + 220, ship->worldTransform._43};
			const auto playerPosition = m_player.GetPosition();
			const auto delta = XMLoadFloat3(&playerPosition) - XMLoadFloat3(&origin);
			if (XMVectorGetX(XMVector3Length(delta)) > m_missionSettings.aaRange)
				continue;
			// Limited prediction and alternating dispersion leave room to dodge visible tracers.
			const float travel = XMVectorGetX(XMVector3Length(delta)) / 1800;
			const auto playerVelocity = m_player.GetVelocity();
			const auto point = XMLoadFloat3(&playerPosition) +
				XMLoadFloat3(&playerVelocity) * std::min(travel, 2.0f) +
				XMVectorSet(std::sin(m_battleSeconds * 1.3f + float(i)) * 100, 50, 0, 0);
			XMFLOAT3 velocity;
			XMStoreFloat3(&velocity, XMVector3Normalize(point - XMLoadFloat3(&origin)) * 1800);
			if (m_hostileProjectiles.FireSingle(m_scene, origin, velocity, AttackSource::AntiAircraft, 8))
				m_aaCooldowns[i] = m_missionSettings.aaInterval;
		}
		auto tracers = m_effects.GetSnapshot().gunTracers;
		auto hostileTracers = m_hostileProjectiles.TakeTracers();
		tracers.insert(tracers.end(), hostileTracers.begin(), hostileTracers.end());
		m_effects.SetGunTracers(std::move(tracers));
	}
} // namespace Game
