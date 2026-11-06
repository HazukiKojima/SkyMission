#include "BombSystem.h"
#include "ProjectilePose.h"
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
		if (!m_bombs.empty()) throw std::logic_error("Cannot configure active bombs");
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

	bool BombSystem::TryDrop(Engine::Scene& scene, const XMFLOAT3& position, const XMFLOAT3& velocity) {
		if (!m_model || m_cooldownSeconds > 0) return false;
		const auto flight = ReleaseFlight(position, velocity);
		m_bombs.push_back({scene.AddObject(m_model,
							   BuildProjectileTransform(flight.position, flight.velocity, m_modelForward)),
			flight});
		m_cooldownSeconds = m_reloadSeconds;
		return true;
	}

	void BombSystem::Update(Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& deck) {
		if (!std::isfinite(dt) || dt <= 0) return;
		dt = std::min(dt, 0.25f);
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
					m_hits.push_back({.target = deck->id,
						.frameSeconds = valid * *contact,
						.position = point,
						.direction = {0, -1, 0},
						.kind = ProjectileKind::Bomb,
						.source = AttackSource::Player,
						.damage = 1});
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
				object->worldTransform =
					BuildProjectileTransform(bomb.flight.position, bomb.flight.velocity, m_modelForward);
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
