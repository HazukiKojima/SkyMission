#include "ProjectileSystem.h"
#include <algorithm>
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/ProjectileCollision.h"
#include "Game/Effects/CombatEffectSettings.h"

namespace Game {
	using namespace DirectX;

	void ProjectileSystem::Configure(
		const GunSettings& settings, std::shared_ptr<Engine::Model> model, const XMFLOAT3& modelForward) {
		ValidateGunSettings(settings);
		if (!model)
			throw std::invalid_argument("ammunition model is required");
		if (!m_projectiles.empty())
			throw std::logic_error("Cannot reconfigure active projectiles");
		const auto source = XMLoadFloat3(&modelForward);
		const float length = XMVectorGetX(XMVector3Length(source));
		if (!std::isfinite(length) || length < 0.001f)
			throw std::invalid_argument("modelForward must be a finite nonzero direction");
		const auto from = XMVector3Normalize(source);
		const auto to = XMVectorSet(0, 0, -1, 0);
		const float dot = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
		XMMATRIX rotation = XMMatrixIdentity();
		if (dot < -0.9999f)
			rotation = XMMatrixRotationY(XM_PI);
		else if (dot < 0.9999f)
			rotation = XMMatrixRotationAxis(XMVector3Cross(from, to), std::acos(dot));
		XMStoreFloat4x4(&m_modelRotation, rotation);
		m_model = std::move(model);
		m_settings = settings;
		m_projectiles.reserve(settings.maxProjectiles);
		m_volleySequence = 0;
		m_shotEvents.clear();
		m_tracers.clear();
	}

	std::vector<GunShotEvent> ProjectileSystem::TakeShotEvents() {
		auto events = std::move(m_shotEvents);
		m_shotEvents.clear();
		return events;
	}

	std::vector<GunTracerSegment> ProjectileSystem::TakeTracers() {
		auto tracers = std::move(m_tracers);
		m_tracers.clear();
		return tracers;
	}

	void ProjectileSystem::AddTracer(const Projectile& projectile, const XMFLOAT3& head) {
		if (!projectile.isTracer || m_tracers.size() >= GunEffectSettings::maxTracerCount)
			return;
		const auto velocity = XMLoadFloat3(&projectile.velocity);
		const float speed = XMVectorGetX(XMVector3Length(velocity));
		const float distance =
			XMVectorGetX(XMVector3Length(XMLoadFloat3(&head) - XMLoadFloat3(&projectile.birthPosition)));
		const float length = std::min(
			{distance, speed * GunEffectSettings::tracerSeconds, GunEffectSettings::tracerMaxLength});
		if (!std::isfinite(length) || length <= 0 || speed <= 0)
			return;
		GunTracerSegment segment;
		segment.head = head;
		XMStoreFloat3(&segment.tail, XMLoadFloat3(&head) - velocity * (length / speed));
		m_tracers.push_back(segment);
	}

	void ProjectileSystem::Advance(Projectile& projectile, float dt) {
		projectile.position.x += projectile.velocity.x * dt;
		projectile.position.y += projectile.velocity.y * dt;
		projectile.position.z += projectile.velocity.z * dt;
		projectile.remainingLifetime -= dt;
	}

	bool ProjectileSystem::MoveAndCollide(Projectile& projectile, float duration, float frameOffset,
		float frameSeconds, std::span<const CollisionTarget> targets) {
		if (projectile.position.y <= 0 || projectile.remainingLifetime <= 0)
			return false;
		float valid = std::min(duration, projectile.remainingLifetime);
		if (projectile.velocity.y < 0)
			valid = std::min(valid, -projectile.position.y / projectile.velocity.y);
		const auto start = projectile.position;
		Advance(projectile, valid);
		std::optional<float> first;
		Engine::ObjectId hit = Engine::InvalidObjectId;
		for (const auto& target : targets) {
			const float begin = frameSeconds > 0 ? frameOffset / frameSeconds : 1;
			const float end = frameSeconds > 0 ? (frameOffset + valid) / frameSeconds : 1;
			const auto contact = IntersectMovingTarget(start, projectile.position, target, begin, end);
			if (contact && (!first || *contact < *first || (*contact == *first && target.id < hit))) {
				first = contact;
				hit = target.id;
			}
		}
		if (first) {
			XMFLOAT3 position;
			XMStoreFloat3(
				&position, XMVectorLerp(XMLoadFloat3(&start), XMLoadFloat3(&projectile.position), *first));
			XMFLOAT3 direction;
			XMStoreFloat3(&direction, XMVector3Normalize(XMLoadFloat3(&projectile.velocity)));
			m_hits.push_back({.target = hit,
				.frameSeconds = frameOffset + *first * valid,
				.position = position,
				.direction = direction,
				.kind = ProjectileKind::Gun,
				.source = projectile.source,
				.damage = projectile.damage});
			// 接触点を終端にして、敵を貫く光の筋を残さない
			AddTracer(projectile, position);
			return false;
		}
		AddTracer(projectile, projectile.position);
		return valid >= duration && projectile.remainingLifetime > 0 && projectile.position.y > 0;
	}

	void ProjectileSystem::Update(Engine::Scene& scene, float dt, std::span<const CollisionTarget> targets) {
		if (!std::isfinite(dt) || dt <= 0)
			return;
		dt = std::min(dt, 0.25f);
		// 前フレームの通知と、消滅した弾の描画区間を持ち越さない
		m_shotEvents.clear();
		m_tracers.clear();
		std::erase_if(m_projectiles, [&](Projectile& projectile) {
			const bool alive = MoveAndCollide(projectile, dt, 0, dt, targets);
			auto* object = scene.FindObject(projectile.objectId);
			if (!alive || !object) {
				scene.RemoveObject(projectile.objectId);
				return true;
			}
			object->worldTransform._41 = projectile.position.x;
			object->worldTransform._42 = projectile.position.y;
			object->worldTransform._43 = projectile.position.z;
			return false;
		});
	}

	bool ProjectileSystem::FireSingle(Engine::Scene& scene, const XMFLOAT3& position,
		const XMFLOAT3& velocity, AttackSource source, float damage) {
		if (!m_model || m_projectiles.size() >= m_settings.maxProjectiles)
			return false;
		const auto velocityVector = XMLoadFloat3(&velocity);
		const float speedSquared = XMVectorGetX(XMVector3LengthSq(velocityVector));
		if (!std::isfinite(speedSquared) || speedSquared < 1e-8f)
			return false;
		const auto direction = XMVector3Normalize(velocityVector);
		const auto forward = XMVectorSet(0, 0, -1, 0);
		const float dot = std::clamp(XMVectorGetX(XMVector3Dot(forward, direction)), -1.0f, 1.0f);
		auto rotation = XMMatrixIdentity();
		if (dot < -0.999999f)
			rotation = XMMatrixRotationY(XM_PI);
		else if (dot < 0.999999f)
			rotation = XMMatrixRotationAxis(XMVector3Cross(forward, direction), std::acos(dot));
		Projectile projectile;
		projectile.position = projectile.birthPosition = position;
		projectile.velocity = velocity;
		projectile.remainingLifetime = m_settings.lifetime;
		projectile.isTracer = true;
		projectile.source = source;
		projectile.damage = damage;
		XMFLOAT4X4 transform;
		XMStoreFloat4x4(&transform,
			XMLoadFloat4x4(&m_modelRotation) * rotation *
				XMMatrixTranslation(position.x, position.y, position.z));
		projectile.objectId = scene.AddObject(m_model, transform);
		m_projectiles.push_back(projectile);
		return true;
	}

	bool ProjectileSystem::FireVolley(Engine::Scene& scene, const GunVolley& volley) {
		if (!m_model || m_settings.maxProjectiles - m_projectiles.size() < 4)
			return false;
		auto aircraftRotation = XMLoadFloat4x4(&volley.aircraftPose);
		aircraftRotation.r[3] = XMVectorSet(0, 0, 0, 1);
		for (size_t muzzleIndex = 0; muzzleIndex < volley.muzzles.size(); ++muzzleIndex) {
			const auto& muzzle = volley.muzzles[muzzleIndex];
			Projectile projectile{};
			XMStoreFloat3(&projectile.position,
				XMVector3TransformCoord(XMLoadFloat3(&muzzle), XMLoadFloat4x4(&volley.aircraftWorld)));
			XMFLOAT3 direction = volley.forward;
			if (volley.assistance.target) {
				const auto aim = SolveGunAim(projectile.position, volley.aircraftVelocity,
					*volley.assistance.target, m_settings.muzzleSpeed, m_settings.lifetime);
				if (aim && IsWithinAngle(volley.forward, aim->direction, volley.assistance.angleDegrees))
					direction = aim->direction;
			}
			XMStoreFloat3(&projectile.velocity,
				XMLoadFloat3(&volley.aircraftVelocity) + XMLoadFloat3(&direction) * m_settings.muzzleSpeed);
			projectile.remainingLifetime = m_settings.lifetime;
			projectile.damage = volley.damage;
			projectile.birthPosition = projectile.position;
			// L1・R2とL2・R1を交互に光らせ、4門が同時に消える周期を作らない
			const unsigned phase = muzzleIndex == 0 || muzzleIndex == 3 ? 0u : 1u;
			projectile.isTracer = (m_volleySequence + phase) % 2 == 0;
			m_shotEvents.push_back({muzzleIndex, projectile.position, volley.aircraftVelocity, direction,
				volley.ageSeconds, m_volleySequence});
			if (!MoveAndCollide(projectile, volley.ageSeconds,
					std::max(0.0f, volley.frameSeconds - volley.ageSeconds), volley.frameSeconds,
					volley.targets))
				continue;
			// ロールを維持しつつ、弾モデルの機首方向を補正後の射線へ合わせる
			const auto from = XMLoadFloat3(&volley.forward);
			const auto to = XMLoadFloat3(&direction);
			const float dot = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
			auto correction = XMMatrixIdentity();
			if (dot < 0.999999f)
				correction = XMMatrixRotationAxis(XMVector3Cross(from, to), std::acos(dot));
			XMFLOAT4X4 transform;
			XMStoreFloat4x4(&transform,
				XMLoadFloat4x4(&m_modelRotation) * aircraftRotation * correction *
					XMMatrixTranslation(projectile.position.x, projectile.position.y, projectile.position.z));
			projectile.objectId = scene.AddObject(m_model, transform);
			m_projectiles.push_back(projectile);
		}
		++m_volleySequence;
		return true;
	}
} // namespace Game
