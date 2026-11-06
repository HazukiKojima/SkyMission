#include "CarrierMissileSystem.h"
#include "ProjectilePose.h"
#include "Game/Combat/ProjectileCollision.h"
#include <stdexcept>

namespace Game {
	using namespace DirectX;
	namespace {
		constexpr float kMaximumReloadSeconds = 120;
		constexpr float kMaximumSimulationStepSeconds = 1.0f / 240;
		constexpr float kMissileSmokeIntervalSeconds = 0.06f;
		constexpr float kMissileTurnRateRadiansPerSecond = XM_PIDIV2;
		constexpr float kMissileReleaseSideOffsetMeters = 14.0f;
	} // namespace

	void CarrierMissileSystem::Configure(
		float reloadSeconds, std::shared_ptr<Engine::Model> model, const XMFLOAT3& modelForward) {
		if (!std::isfinite(reloadSeconds) || reloadSeconds < 1 || reloadSeconds > kMaximumReloadSeconds ||
			!model)
			throw std::invalid_argument("Missile requires a model and reload interval in [1,120]");
		const auto forward = XMLoadFloat3(&modelForward);
		const float length = XMVectorGetX(XMVector3Length(forward));
		if (!std::isfinite(length) || length < 0.001f)
			throw std::invalid_argument("Missile modelForward must be finite and nonzero");
		if (!m_missiles.empty())
			throw std::logic_error("Cannot configure active missiles");
		m_ammo = 2;
		m_model = std::move(model);
		XMStoreFloat3(&m_modelForward, XMVector3Normalize(forward));
		m_reloadSeconds = reloadSeconds;
		m_cooldownSeconds = 0;
		m_hits.clear();
		m_missImpacts.clear();
		m_trail.clear();
	}

	void CarrierMissileSystem::ConfigureFlight(float speed, float damage) {
		if (!std::isfinite(speed) || speed <= 0 || !std::isfinite(damage) || damage <= 0)
			throw std::invalid_argument("Missile speed/damage must be positive and finite");
		m_missileSpeed = speed;
		m_missileDamage = damage;
	}
	size_t CarrierMissileSystem::RequiredModelCapacity(float reloadSeconds) {
		if (!std::isfinite(reloadSeconds) || reloadSeconds < 1 || reloadSeconds > kMaximumReloadSeconds)
			throw std::invalid_argument("Missile reload interval must be in [1,120]");
		return 2 *
			(static_cast<size_t>(std::ceil(CarrierMissileFlight::lifetimeSeconds / reloadSeconds)) + 1);
	}
	CarrierMissileFlight CarrierMissileSystem::BuildLaunchFlight(
		const XMFLOAT3& position, const XMFLOAT3& velocity, const XMFLOAT3& forward) const {
		auto release = position;
		const float side = m_ammo == 1 ? kMissileReleaseSideOffsetMeters : -kMissileReleaseSideOffsetMeters;
		release.x -= forward.z * side;
		release.y -= CarrierMissileFlight::releaseOffsetMeters;
		release.z += forward.x * side;
		XMFLOAT3 launch;
		XMStoreFloat3(&launch, XMLoadFloat3(&velocity) + XMLoadFloat3(&forward) * m_missileSpeed);
		return {release, launch};
	}

	bool CarrierMissileSystem::TryLaunch(Engine::Scene& scene, const XMFLOAT3& position,
		const XMFLOAT3& velocity, const XMFLOAT3& forward, Engine::ObjectId target) {
		if (!m_model || m_ammo == 0 || m_cooldownSeconds > 0)
			return false;
		const auto flight = BuildLaunchFlight(position, velocity, forward);
		m_missiles.push_back({scene.AddObject(m_model,
								  BuildProjectileTransform(flight.position, flight.velocity, m_modelForward)),
			target, flight});
		--m_ammo;
		if (m_ammo == 0)
			m_cooldownSeconds = m_reloadSeconds;
		return true;
	}
	void CarrierMissileSystem::Update(
		Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& hull) {
		if (!std::isfinite(dt) || dt <= 0)
			return;
		dt = std::min(dt, 0.25f);
		if (m_cooldownSeconds > 0) {
			m_cooldownSeconds = std::max(0.0f, m_cooldownSeconds - dt);
			if (m_cooldownSeconds <= 0)
				m_ammo = 2;
		}
		// 低FPSでも一度に大きく旋回・移動せず、短い区間ごとに命中を判定する
		const int stepCount = static_cast<int>(std::ceil(dt / kMaximumSimulationStepSeconds));
		const float stepSeconds = dt / stepCount;
		std::erase_if(m_missiles, [&](Missile& missile) {
			for (int step = 0; step < stepCount; ++step) {
				const float frameOffsetSeconds = step * stepSeconds;
				const auto start = missile.flight.position;
				const float durationSeconds = std::min(stepSeconds, missile.remainingSeconds);
				if (missile.target != Engine::InvalidObjectId) {
					if (hull && hull->id == missile.target) {
						XMFLOAT3 target;
						const auto center = hull->bounds.Center();
						XMStoreFloat3(&target,
							XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&hull->world)));
						missile.flight.GuideTo(target, kMissileTurnRateRadiansPerSecond * durationSeconds);
					} else {
						missile.target = Engine::InvalidObjectId;
					}
				}
				const float validSeconds =
					std::min(durationSeconds, missile.flight.TimeToHeight(0).value_or(durationSeconds));
				missile.flight.Advance(validSeconds);
				missile.remainingSeconds -= validSeconds;
				if (hull) {
					// 動く船体にも、今回進めた区間のフレーム内時刻を渡す
					const float startFraction = frameOffsetSeconds / dt;
					const float endFraction = (frameOffsetSeconds + validSeconds) / dt;
					if (const auto contact = IntersectMovingTarget(
							start, missile.flight.position, *hull, startFraction, endFraction)) {
						XMFLOAT3 point;
						XMStoreFloat3(&point,
							XMVectorLerp(
								XMLoadFloat3(&start), XMLoadFloat3(&missile.flight.position), *contact));
						// ミサイルの命中演出は入射方向を使わない
						m_hits.push_back({.target = hull->id,
							.frameSeconds = frameOffsetSeconds + validSeconds * *contact,
							.position = point,
							.direction = {},
							.kind = ProjectileKind::Rocket,
							.source = AttackSource::Player,
							.damage = m_missileDamage});
						scene.RemoveObject(missile.id);
						return true;
					}
				}
				missile.smokeSeconds -= validSeconds;
				if (missile.smokeSeconds <= 0) {
					m_trail.push_back(missile.flight.position);
					missile.smokeSeconds += kMissileSmokeIntervalSeconds;
				}
				if (missile.flight.position.y <= 0.001f || missile.remainingSeconds <= 0) {
					m_missImpacts.push_back(missile.flight.position);
					scene.RemoveObject(missile.id);
					return true;
				}
			}
			if (auto* object = scene.FindObject(missile.id))
				object->worldTransform = BuildProjectileTransform(
					missile.flight.position, missile.flight.velocity, m_modelForward);
			return false;
		});
	}

	std::vector<HitEvent> CarrierMissileSystem::TakeHits() {
		auto hits = std::move(m_hits);
		m_hits.clear();
		return hits;
	}
	std::vector<XMFLOAT3> CarrierMissileSystem::TakeMissImpacts() {
		auto impacts = std::move(m_missImpacts);
		m_missImpacts.clear();
		return impacts;
	}
} // namespace Game
