#include "FlightMission.h"
#include "Game/Combat/EnemyGunBehavior.h"
#include "Game/Combat/ProjectileCollision.h"
#include <algorithm>
#include <cmath>
namespace Game {
	using namespace DirectX;

	void FlightMission::Configure(const MissionSettings& settings, std::shared_ptr<Engine::Model> enemyModel,
		float enemyScale, const AircraftFlightParameters& flight, const AircraftAIParameters& ai) {
		settings.Validate();
		if (!enemyModel) {
			throw std::invalid_argument("Mission requires an enemy model");
		}
		m_missionSettings = settings;
		m_mission = MissionState(settings.carrierHealth);
		m_reinforcementElapsedSeconds = -1;
		m_missionEnemyModel = std::move(enemyModel);
		m_missionEnemyFlight = flight;
		m_missionEnemyAI = ai;
		m_missionEnemyScale = enemyScale;
	}
	bool FlightMission::Advance(size_t remaining, bool alive, float dt) {
		m_missionMessageSeconds = std::max(0.0f, m_missionMessageSeconds - dt);
		if (alive && m_mission.GetPhase() == MissionPhase::CarrierAttack && m_mission.IsDeckDisabled()) {
			if (m_reinforcementElapsedSeconds < 0)
				m_reinforcementElapsedSeconds = 0;
			else
				m_reinforcementElapsedSeconds += dt;
			if (m_reinforcementElapsedSeconds + 1e-6f < 4.0f) return false;
		}
		const bool changed = m_mission.Advance(remaining, alive);
		if (changed) {
			m_missionMessageSeconds = m_missionSettings.missionMessageDurationSeconds;
		}
		return changed;
	}
	void FlightMission::RegisterShip(Engine::Scene& scene, Engine::ObjectId id, bool carrier) {
		const auto* ship = scene.FindObject(id);
		if (!ship || !ship->model->GetLocalBounds()) {
			throw std::invalid_argument("Mission ship must have loaded bounds");
		}
		if (carrier) {
			m_carrierId = id;
			m_defenseCenter = GetCarrierCenter(scene);
		} else {
			const float openingDelay = m_missionSettings.escortOpeningDelaySeconds +
				static_cast<float>(m_escorts.size() + 1) * m_missionSettings.escortOpeningStaggerSeconds;
			m_escorts.push_back({.objectId = id, .cooldownSeconds = openingDelay});
		}
	}
	std::optional<CollisionTarget> FlightMission::GetDeckTarget(Engine::Scene& scene) const {
		const auto* carrier = scene.FindObject(m_carrierId);
		if (!carrier || !carrier->model->GetLocalBounds()) return std::nullopt;
		auto bounds = *carrier->model->GetLocalBounds();
		const auto center = bounds.Center();
		const float halfX =
			(bounds.maximum.x - bounds.minimum.x) * m_missionSettings.deckWidthFraction * 0.5f;
		const float halfZ =
			(bounds.maximum.z - bounds.minimum.z) * m_missionSettings.deckLengthFraction * 0.5f;
		const float height =
			bounds.minimum.y + (bounds.maximum.y - bounds.minimum.y) * m_missionSettings.deckHeightFraction;
		const float thickness =
			(bounds.maximum.y - bounds.minimum.y) * m_missionSettings.deckHalfThicknessFraction;
		bounds.minimum = {center.x - halfX, height - thickness, center.z - halfZ};
		bounds.maximum = {center.x + halfX, height + thickness, center.z + halfZ};
		return CollisionTarget{m_carrierId, bounds,
			{carrier->worldTransform._41, carrier->worldTransform._42, carrier->worldTransform._43},
			carrier->worldTransform};
	}
	std::vector<FlightMission::AircraftSpawn> FlightMission::BuildWave(Engine::Scene& scene,
		const DirectX::XMFLOAT3& player, size_t count, SquadronKind kind, float enemyHealth,
		const XMFLOAT3& forward, size_t slotBase) {
		const bool opening = kind == SquadronKind::Patrol;
		const bool commander = kind == SquadronKind::Commander;
		std::vector<AircraftSpawn> spawns;
		XMFLOAT3 away{0, 0, 1};
		if (const auto* carrier = scene.FindObject(m_carrierId)) {
			away = {player.x - carrier->worldTransform._41, 0, player.z - carrier->worldTransform._43};
			if (std::hypot(away.x, away.z) < 1) {
				away = {0, 0, 1};
			}
			XMStoreFloat3(&away, XMVector3Normalize(XMLoadFloat3(&away)));
		}
		GetDefenseCenter(scene);
		m_airCombatPosition = {player.x + away.x * m_missionSettings.combatAreaDistanceMeters,
			std::max(m_missionSettings.minimumCombatAltitudeMeters, player.y),
			player.z + away.z * m_missionSettings.combatAreaDistanceMeters};
		if (m_defenseCenter) {
			m_airCombatPosition = {
				m_defenseCenter->x + away.x * m_missionSettings.carrierCombatAreaDistanceMeters,
				std::max(m_missionSettings.minimumCombatAltitudeMeters, player.y),
				m_defenseCenter->z + away.z * m_missionSettings.carrierCombatAreaDistanceMeters};
		}
		XMFLOAT3 approach{forward.x, 0, forward.z};
		if (std::hypot(approach.x, approach.z) < 0.001f) {
			approach = {0, 0, -1};
		}
		XMStoreFloat3(&approach, XMVector3Normalize(XMLoadFloat3(&approach)));
		for (size_t i = 0; i < count; ++i) {
			const auto position = CalculateWavePosition(player, count, i, opening, approach, away);
			const auto ai = BuildWaveAI(slotBase + i);
			spawns.push_back({.model = m_missionEnemyModel,
				.position = position,
				.scale = m_missionEnemyScale,
				.flight = m_missionEnemyFlight,
				.ai = ai,
				.health = enemyHealth * (commander ? m_missionSettings.commanderHealthMultiplier : 1),
				.name = commander
					? m_text.Text("敵編隊長機")
					: (opening ? m_text.Text("哨戒機 ") : m_text.Text("敵戦闘機 ")) + std::to_string(i + 1),
				.facePlayer = opening,
				.heading = opening ? std::nullopt : std::optional<XMFLOAT3>{away}});
		}
		return spawns;
	}
	XMFLOAT3 FlightMission::CalculateWavePosition(const XMFLOAT3& player, size_t count, size_t index,
		bool opening, const XMFLOAT3& approach, const XMFLOAT3& away) const {
		const float lateralOffsetMeters = (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) *
			m_missionSettings.formationSpacingMeters;
		const size_t formationSlot = index % std::size(m_missionSettings.depthOffsetsMeters);
		const float altitudeMeters =
			std::max(m_missionEnemyAI.lowAltitude + m_missionSettings.formationClearanceMeters,
				(opening ? player.y : m_airCombatPosition.y) +
					m_missionSettings.altitudeOffsetsMeters[formationSlot]);
		const float approachDistanceMeters = m_missionSettings.patrolApproachDistanceMeters +
			m_missionSettings.depthOffsetsMeters[formationSlot];
		// 前後・左右・高度をずらし、単一の平面に並ばないようにする
		return opening
			? XMFLOAT3{player.x + approach.x * approachDistanceMeters - approach.z * lateralOffsetMeters,
				  altitudeMeters,
				  player.z + approach.z * approachDistanceMeters + approach.x * lateralOffsetMeters}
			: XMFLOAT3{m_airCombatPosition.x + away.z * lateralOffsetMeters -
					  away.x * m_missionSettings.depthOffsetsMeters[formationSlot] * 0.5f,
				  altitudeMeters,
				  m_airCombatPosition.z - away.x * lateralOffsetMeters -
					  away.z * m_missionSettings.depthOffsetsMeters[formationSlot] * 0.5f};
	}
	AircraftAIParameters FlightMission::BuildWaveAI(size_t formationSlot) const {
		auto ai = m_missionEnemyAI;
		// 射撃条件はJSONを使い、初動の待ち時間だけ機体ごとにずらす
		ai.openingGraceSeconds +=
			EnemyVariation(formationSlot, 0, 79) * m_missionSettings.maximumOpeningDelaySeconds;
		return ai;
	}
	std::optional<XMFLOAT3> FlightMission::GetCarrierCenter(Engine::Scene& scene) const {
		const auto* carrier = scene.FindObject(m_carrierId);
		if (!carrier || !carrier->model->GetLocalBounds()) return std::nullopt;
		const auto center = carrier->model->GetLocalBounds()->Center();
		XMFLOAT3 result;
		XMStoreFloat3(&result,
			XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&carrier->worldTransform)));
		return result;
	}
	void FlightMission::UpdateDeckSmoke(Engine::Scene& scene, CombatEffectSystem& effects, float dt) {
		if (m_mission.IsDeckDisabled()) {
			m_deckSmokeSeconds -= dt;
			if (m_deckSmokeSeconds <= 0) {
				if (const auto center = GetCarrierCenter(scene)) {
					effects.SpawnDeckSmoke(*center);
				}
				m_deckSmokeSeconds = effects.GetSettings().carrier.smokeIntervalSeconds;
			}
		}
	}
	void FlightMission::UpdateAntiAircraftGuns(Engine::Scene& scene, const AircraftFlightModel& player,
		ProjectileSystem& projectiles, float battleSeconds, float dt) {
		for (size_t i = 0; i < m_escorts.size(); ++i) {
			m_escorts[i].cooldownSeconds = std::max(0.0f, m_escorts[i].cooldownSeconds - dt);
			const auto* ship = scene.FindObject(m_escorts[i].objectId);
			if (!ship || m_escorts[i].cooldownSeconds > 0) continue;
			XMFLOAT3 origin{ship->worldTransform._41,
				ship->worldTransform._42 + m_missionSettings.antiAircraftMuzzleHeightMeters,
				ship->worldTransform._43};
			const auto playerPosition = player.GetPosition();
			const auto delta = XMLoadFloat3(&playerPosition) - XMLoadFloat3(&origin);
			const float distanceMeters = XMVectorGetX(XMVector3Length(delta));
			if (distanceMeters > m_missionSettings.aaRange) continue;
			// 予測時間を制限し照準を揺らすことで、見える曳光弾を回避する余地を残す
			const float travelSeconds =
				distanceMeters / m_missionSettings.antiAircraftMuzzleSpeedMetersPerSecond;
			const auto playerVelocity = player.GetVelocity();
			const auto predictedDisplacement = XMLoadFloat3(&playerVelocity) *
				std::min(travelSeconds, m_missionSettings.antiAircraftMaximumLeadSeconds);
			const float dispersionPhaseRadians =
				battleSeconds * m_missionSettings.antiAircraftDispersionRateRadiansPerSecond + float(i);
			const auto dispersionOffset =
				XMVectorSet(std::sin(dispersionPhaseRadians) * m_missionSettings.antiAircraftDispersionMeters,
					m_missionSettings.antiAircraftAimOffsetMeters, 0, 0);
			const auto point = XMLoadFloat3(&playerPosition) + predictedDisplacement + dispersionOffset;
			XMFLOAT3 velocity;
			XMStoreFloat3(&velocity,
				XMVector3Normalize(point - XMLoadFloat3(&origin)) *
					m_missionSettings.antiAircraftMuzzleSpeedMetersPerSecond);
			if (projectiles.FireSingle(scene, origin, velocity, AttackSource::AntiAircraft,
					m_missionSettings.antiAircraftDamage)) {
				m_escorts[i].cooldownSeconds = m_missionSettings.aaInterval;
			}
		}
	}
} // namespace Game
