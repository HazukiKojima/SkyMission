#include "BombSystem.h"
#include "Game/Combat/ProjectileCollision.h"
#include <stdexcept>

namespace Game {
	using namespace DirectX;

	void BombSystem::Configure(
		float reloadSeconds, std::shared_ptr<Engine::Model> model, const XMFLOAT3& modelForward) {
		if (!std::isfinite(reloadSeconds) || reloadSeconds < 1 ||
			reloadSeconds > BombTuning::lifetimeSeconds || !model)
			throw std::invalid_argument("Bomb requires a model and reload interval in [1,120]");
		const auto forward = XMLoadFloat3(&modelForward);
		const float length = XMVectorGetX(XMVector3Length(forward));
		if (!std::isfinite(length) || length < 0.001f)
			throw std::invalid_argument("Bomb modelForward must be finite and nonzero");
		if (!m_bombs.empty() || !m_missiles.empty())
			throw std::logic_error("Cannot configure active bombs");
		m_missileMode = false;
		m_ammo = 2;
		m_model = std::move(model);
		XMStoreFloat3(&m_modelForward, XMVector3Normalize(forward));
		m_reloadSeconds = reloadSeconds;
		m_cooldownSeconds = 0;
		m_hits.clear();
		m_missImpacts.clear();
	}

	BombFlight BombSystem::ReleaseFlight(XMFLOAT3 position, const XMFLOAT3& velocity) {
		position.y -= BombTuning::releaseOffsetMeters;
		return {position, velocity};
	}

	size_t BombSystem::RequiredModelCapacity(float reloadSeconds) {
		if (!std::isfinite(reloadSeconds) || reloadSeconds < 1 || reloadSeconds > BombTuning::lifetimeSeconds)
			throw std::invalid_argument("Bomb reload interval must be in [1,120]");
		return static_cast<size_t>(std::ceil(BombTuning::lifetimeSeconds / reloadSeconds)) + 2;
	}

	XMFLOAT4X4 BombSystem::BuildTransform(const BombFlight& flight) const {
		const auto from = XMLoadFloat3(&m_modelForward);
		const auto velocity = XMLoadFloat3(&flight.velocity);
		const auto to =
			XMVectorGetX(XMVector3LengthSq(velocity)) > 0.0001f ? XMVector3Normalize(velocity) : from;
		const float dot = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
		auto rotation = XMMatrixIdentity();
		if (dot < -0.9999f) {
			const auto reference =
				std::abs(m_modelForward.y) < 0.9f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
			rotation = XMMatrixRotationAxis(XMVector3Cross(from, reference), XM_PI);
		} else if (dot < 0.9999f)
			rotation = XMMatrixRotationAxis(XMVector3Cross(from, to), std::acos(dot));
		const auto& p = flight.position;
		XMFLOAT4X4 transform;
		XMStoreFloat4x4(&transform, rotation * XMMatrixTranslation(p.x, p.y, p.z));
		return transform;
	}

	bool BombSystem::TryDrop(Engine::Scene& scene, const XMFLOAT3& position, const XMFLOAT3& velocity) {
		if (!m_model || m_cooldownSeconds > 0)
			return false;
		const auto flight = ReleaseFlight(position, velocity);
		m_bombs.push_back({scene.AddObject(m_model, BuildTransform(flight)), flight});
		m_cooldownSeconds = m_reloadSeconds;
		return true;
	}

	void BombSystem::Update(Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& deck) {
		if (!std::isfinite(dt) || dt <= 0)
			return;
		dt = std::min(dt, 0.25f);
		if (m_missileMode) { UpdateMissiles(scene, dt, deck); return; }
		m_cooldownSeconds = std::max(0.0f, m_cooldownSeconds - dt);
		std::erase_if(m_bombs, [&](Bomb& bomb) {
			const auto start = bomb.flight.position;
			const auto seaTime = bomb.flight.TimeToHeight(0);
			const float valid = seaTime ? std::min(dt, *seaTime) : dt;
			bomb.flight.Advance(valid);
			bomb.remainingSeconds -= dt;
			if (deck) {
				if (const auto contact = IntersectMovingTarget(start, bomb.flight.position, *deck, 0, 1)) {
					XMFLOAT3 point;
					XMStoreFloat3(&point,
						XMVectorLerp(XMLoadFloat3(&start), XMLoadFloat3(&bomb.flight.position), *contact));
					m_hits.push_back({deck->id, valid * *contact, point, {0, -1, 0}, ProjectileKind::Bomb,
						AttackSource::Player, 1});
					scene.RemoveObject(bomb.id);
					return true;
				}
			}
			if (bomb.flight.position.y <= 0.001f || bomb.remainingSeconds <= 0) {
				m_missImpacts.push_back(bomb.flight.position);
				scene.RemoveObject(bomb.id);
				return true;
			}
			if (auto* object = scene.FindObject(bomb.id))
				object->worldTransform = BuildTransform(bomb.flight);
			return false;
		});
	}

	void BombSystem::ConfigureMissiles(float speed, float damage) {
		if (!std::isfinite(speed) || speed <= 0 || !std::isfinite(damage) || damage <= 0)
			throw std::invalid_argument("Missile speed/damage must be positive and finite");
		m_missileMode = true;
		m_missileSpeed = speed;
		m_missileDamage = damage;
	}
	bool BombSystem::TryLaunch(Engine::Scene& scene, const XMFLOAT3& position,
		const XMFLOAT3& velocity, const XMFLOAT3& forward, Engine::ObjectId target) {
		if (!m_model || !m_missileMode || m_ammo == 0 || m_cooldownSeconds > 0) return false;
		auto release = position;
		const float side = m_ammo == 2 ? -14.0f : 14.0f;
		release.x -= forward.z * side;
		release.y -= BombTuning::releaseOffsetMeters;
		release.z += forward.x * side;
		XMFLOAT3 launch;
		XMStoreFloat3(&launch, XMLoadFloat3(&velocity) + XMLoadFloat3(&forward) * m_missileSpeed);
		m_missiles.push_back({scene.AddObject(m_model, BuildTransform({release, launch})), target, {release, launch}});
		if (--m_ammo == 0) m_cooldownSeconds = m_reloadSeconds;
		return true;
	}
	void BombSystem::UpdateMissiles(Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& hull) {
		if (m_cooldownSeconds > 0) {
			m_cooldownSeconds = std::max(0.0f, m_cooldownSeconds - dt);
			if (m_cooldownSeconds <= 0) m_ammo = 2;
		}
		std::erase_if(m_missiles, [&](Missile& missile) {
			const auto start = missile.flight.position;
			if (missile.target != Engine::InvalidObjectId) {
				if (hull && hull->id == missile.target) {
					XMFLOAT3 target;
					const auto center = hull->bounds.Center();
					XMStoreFloat3(&target, XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&hull->world)));
					missile.flight.GuideTo(target, XM_PIDIV2 * dt);
				} else missile.target = Engine::InvalidObjectId;
			}
			const float valid = std::min({dt, missile.remaining, missile.flight.TimeToHeight(0).value_or(dt)});
			missile.flight.Advance(valid);
			missile.remaining -= dt;
			missile.smokeSeconds -= dt;
			if (missile.smokeSeconds <= 0) {
				m_trail.push_back(missile.flight.position);
				missile.smokeSeconds = 0.06f;
			}
			if (hull) {
				if (const auto contact = IntersectMovingTarget(start, missile.flight.position, *hull, 0, 1)) {
					XMFLOAT3 point;
					XMStoreFloat3(&point, XMVectorLerp(XMLoadFloat3(&start), XMLoadFloat3(&missile.flight.position), *contact));
					m_hits.push_back({hull->id, valid * *contact, point, {}, ProjectileKind::Rocket, AttackSource::Player, m_missileDamage});
					scene.RemoveObject(missile.id);
					return true;
				}
			}
			if (missile.flight.position.y <= 0.001f || missile.remaining <= 0) {
				m_missImpacts.push_back(missile.flight.position);
				scene.RemoveObject(missile.id);
				return true;
			}
			if (auto* object = scene.FindObject(missile.id)) object->worldTransform = BuildTransform({missile.flight.position, missile.flight.velocity});
			return false;
		});
	}

	std::vector<HitEvent> BombSystem::TakeHits() {
		auto hits = std::move(m_hits);
		m_hits.clear();
		return hits;
	}
	std::vector<XMFLOAT3> BombSystem::TakeMissImpacts() {
		auto impacts = std::move(m_missImpacts);
		m_missImpacts.clear();
		return impacts;
	}
} // namespace Game
