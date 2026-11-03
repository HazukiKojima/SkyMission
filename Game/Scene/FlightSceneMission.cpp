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
		if (carrier)
			m_carrierId = id;
		else {
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
		for (size_t i = 0; i < count; ++i) {
			const float lateral = (static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * 900;
			XMFLOAT3 position = opening
				? XMFLOAT3{player.x + lateral, player.y + 100, player.z - 4500}
				: XMFLOAT3{m_airCombatPosition.x + away.z * lateral, m_airCombatPosition.y + 150,
					  m_airCombatPosition.z - away.x * lateral};
			auto ai = m_missionEnemyAI;
			ai.disengageDistance = 350;
			ai.threatDistance = 1400;
			ai.attackDistance = 2800;
			ai.aimAngleDegrees = 5;
			ai.aimHoldSeconds = 0.12f;
			ai.tailDistance = 1000;
			ai.flankDistance = 700;
			ai.reengageDistance = 3500;
			ai.separationLookAheadSeconds = 0.8f;
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
		m_bombCooldown = std::max(0.0f, m_bombCooldown - dt);
		const auto deck = GetDeckTarget();
		std::erase_if(m_bombs, [&](Bomb& bomb) {
			const auto start = bomb.flight.position;
			const auto seaTime = bomb.flight.TimeToHeight(0);
			const float valid = seaTime ? std::min(dt, *seaTime) : dt;
			bomb.flight.Advance(valid);
			bomb.life -= dt;
			if (deck && m_mission.GetPhase() == MissionPhase::CarrierAttack && !m_mission.IsDeckDisabled()) {
				if (const auto contact = IntersectMovingTarget(start, bomb.flight.position, *deck, 0, 1)) {
					XMFLOAT3 point;
					XMStoreFloat3(&point,
						XMVectorLerp(XMLoadFloat3(&start), XMLoadFloat3(&bomb.flight.position), *contact));
					m_bombHits.push_back({m_carrierId, valid * *contact, point, {0, -1, 0},
						ProjectileKind::Bomb, AttackSource::Player, 1});
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
		if (input.bombPressCount > 0 && m_bombCooldown <= 0 &&
			m_mission.GetPhase() == MissionPhase::CarrierAttack && !m_mission.IsDeckDisabled()) {
			auto position = m_player.GetPosition();
			position.y -= 12;
			XMFLOAT4X4 transform;
			XMStoreFloat4x4(&transform, XMMatrixTranslation(position.x, position.y, position.z));
			m_bombs.push_back(
				{m_scene.AddObject(m_bombModel, transform), {position, m_player.GetVelocity()}});
			m_bombCooldown = m_missionSettings.bombReloadSeconds;
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
			const auto aim = SolveGunAim(
				enemy.flightModel.GetPosition(), enemyVelocity, player, m_missionSettings.enemyGunSpeed, 5);
			if (!aim ||
				!IsWithinAngle(
					enemy.flightModel.GetForward(), aim->direction, enemy.aiParameters.aimAngleDegrees))
				continue;
			XMFLOAT3 velocity;
			XMStoreFloat3(&velocity,
				XMLoadFloat3(&enemyVelocity) +
					XMLoadFloat3(&aim->direction) * m_missionSettings.enemyGunSpeed);
			if (m_hostileProjectiles.FireSingle(
					m_scene, enemy.flightModel.GetPosition(), velocity, AttackSource::EnemyAircraft, 5))
				enemy.missileCooldown = m_mission.GetPhase() == MissionPhase::Commander
					? m_missionSettings.commanderGunInterval
					: m_missionSettings.enemyGunInterval;
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
