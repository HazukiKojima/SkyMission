#include "FlightMission.h"
#include "Game/Combat/ProjectileCollision.h"
#include <algorithm>
#include <cmath>
namespace Game {
	using namespace DirectX;
	namespace {
		constexpr float kCombatAreaDistance = 8500;
		constexpr float kPatrolDistance = 4500;
		constexpr float kFormationSpacing = 900;
		constexpr float kDeckHalfThicknessFraction = 0.015f;
		constexpr float kDeckSmokeIntervalSeconds = 0.12f;
		constexpr float kAntiAircraftMuzzleSpeed = 1800;
		constexpr float kAntiAircraftDamage = 8;
	} // namespace

	void FlightMission::Configure(const MissionSettings& settings, std::shared_ptr<Engine::Model> enemyModel,
		float enemyScale, const AircraftFlightParameters& flight, const AircraftAIParameters& ai) {
		settings.Validate();
		if (!enemyModel)
			throw std::invalid_argument("Mission requires an enemy model");
		m_missionSettings = settings;
		m_missionEnemyModel = std::move(enemyModel);
		m_missionEnemyFlight = flight;
		m_missionEnemyAI = ai;
		m_missionEnemyScale = enemyScale;
	}
	bool FlightMission::Advance(size_t remaining, bool alive, float dt) {
		m_missionMessageSeconds = std::max(0.0f, m_missionMessageSeconds - dt);
		const bool changed = m_mission.Advance(remaining, alive);
		if (changed)
			m_missionMessageSeconds = 6;
		return changed;
	}
	void FlightMission::RegisterShip(Engine::Scene& scene, Engine::ObjectId id, bool carrier) {
		const auto* ship = scene.FindObject(id);
		if (!ship || !ship->model->GetLocalBounds())
			throw std::invalid_argument("Mission ship must have loaded bounds");
		if (carrier)
			m_carrierId = id;
		else {
			const float openingDelay = 2 + static_cast<float>(m_escorts.size() + 1) * 0.4f;
			m_escorts.push_back({id, openingDelay});
		}
	}
	std::optional<CollisionTarget> FlightMission::GetDeckTarget(Engine::Scene& scene) const {
		const auto* carrier = scene.FindObject(m_carrierId);
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
		const float thickness = (bounds.maximum.y - bounds.minimum.y) * kDeckHalfThicknessFraction;
		bounds.minimum = {center.x - halfX, height - thickness, center.z - halfZ};
		bounds.maximum = {center.x + halfX, height + thickness, center.z + halfZ};
		return CollisionTarget{m_carrierId, bounds,
			{carrier->worldTransform._41, carrier->worldTransform._42, carrier->worldTransform._43},
			carrier->worldTransform};
	}
	std::vector<FlightMission::AircraftSpawn> FlightMission::BuildWave(Engine::Scene& scene,
		const DirectX::XMFLOAT3& player, size_t count, SquadronKind kind, float enemyHealth) {
		const bool opening = kind == SquadronKind::Patrol;
		const bool commander = kind == SquadronKind::Commander;
		std::vector<AircraftSpawn> spawns;
		XMFLOAT3 away{0, 0, 1};
		if (const auto* carrier = scene.FindObject(m_carrierId)) {
			away = {player.x - carrier->worldTransform._41, 0, player.z - carrier->worldTransform._43};
			if (std::hypot(away.x, away.z) < 1)
				away = {0, 0, 1};
			XMStoreFloat3(&away, XMVector3Normalize(XMLoadFloat3(&away)));
		}
		m_airCombatPosition = {player.x + away.x * kCombatAreaDistance, std::max(1000.0f, player.y),
			player.z + away.z * kCombatAreaDistance};
		for (size_t i = 0; i < count; ++i) {
			const float lateral =
				(static_cast<float>(i) - static_cast<float>(count - 1) * 0.5f) * kFormationSpacing;
			XMFLOAT3 position = opening
				? XMFLOAT3{player.x + lateral, player.y + 100, player.z - kPatrolDistance}
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
			spawns.push_back({m_missionEnemyModel, position, m_missionEnemyScale, m_missionEnemyFlight, ai,
				enemyHealth * (commander ? m_missionSettings.commanderHealthMultiplier : 1),
				commander ? "SQUADRON LEADER" : (opening ? "PATROL " : "FIGHTER ") + std::to_string(i + 1),
				opening, opening ? std::nullopt : std::optional<XMFLOAT3>{away}});
		}
		return spawns;
	}
	void FlightMission::UpdateDeckSmoke(Engine::Scene& scene, CombatEffectSystem& effects, float dt) {
		if (m_mission.IsDeckDisabled()) {
			m_deckSmokeSeconds -= dt;
			if (m_deckSmokeSeconds <= 0) {
				if (const auto deck = GetDeckTarget(scene)) {
					XMFLOAT3 point;
					const auto center = deck->bounds.Center();
					XMStoreFloat3(
						&point, XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&deck->world)));
					effects.SpawnDeckSmoke(point);
				}
				m_deckSmokeSeconds = kDeckSmokeIntervalSeconds;
			}
		}
	}
	void FlightMission::UpdateAntiAircraftGuns(Engine::Scene& scene, const AircraftFlightModel& player,
		ProjectileSystem& projectiles, float battleSeconds, float dt) {
		for (size_t i = 0; i < m_escorts.size(); ++i) {
			m_escorts[i].cooldownSeconds = std::max(0.0f, m_escorts[i].cooldownSeconds - dt);
			const auto* ship = scene.FindObject(m_escorts[i].objectId);
			if (!ship || m_escorts[i].cooldownSeconds > 0)
				continue;
			XMFLOAT3 origin{
				ship->worldTransform._41, ship->worldTransform._42 + 220, ship->worldTransform._43};
			const auto playerPosition = player.GetPosition();
			const auto delta = XMLoadFloat3(&playerPosition) - XMLoadFloat3(&origin);
			if (XMVectorGetX(XMVector3Length(delta)) > m_missionSettings.aaRange)
				continue;
			// Limited prediction and alternating dispersion leave room to dodge visible tracers.
			const float travel = XMVectorGetX(XMVector3Length(delta)) / kAntiAircraftMuzzleSpeed;
			const auto playerVelocity = player.GetVelocity();
			const auto point = XMLoadFloat3(&playerPosition) +
				XMLoadFloat3(&playerVelocity) * std::min(travel, 2.0f) +
				XMVectorSet(std::sin(battleSeconds * 1.3f + float(i)) * 100, 50, 0, 0);
			XMFLOAT3 velocity;
			XMStoreFloat3(
				&velocity, XMVector3Normalize(point - XMLoadFloat3(&origin)) * kAntiAircraftMuzzleSpeed);
			if (projectiles.FireSingle(
					scene, origin, velocity, AttackSource::AntiAircraft, kAntiAircraftDamage))
				m_escorts[i].cooldownSeconds = m_missionSettings.aaInterval;
		}
	}
} // namespace Game
