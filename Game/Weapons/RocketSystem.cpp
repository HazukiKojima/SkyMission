#include "RocketSystem.h"
#include "Game/Combat/ProjectileCollision.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;
	namespace {
		constexpr float kStepSeconds = 1.0f / 120.0f;
		constexpr float kTrailIntervalSeconds = RocketEffectSettings::emissionIntervalSeconds;
		XMMATRIX RotationBetween(FXMVECTOR from, FXMVECTOR to, float maximumRadians = XM_PI) {
			const float angle = std::acos(std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f));
			if (angle < 1e-6f)
				return XMMatrixIdentity();
			auto axis = XMVector3Cross(from, to);
			if (XMVectorGetX(XMVector3LengthSq(axis)) < 1e-10f)
				axis = XMVector3Cross(from,
					std::abs(XMVectorGetY(from)) < 0.9f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0));
			return XMMatrixRotationAxis(axis, std::min(angle, maximumRadians));
		}
	} // namespace
	void RocketSystem::Configure(
		const RocketSettings& settings, std::shared_ptr<Engine::Model> model, const XMFLOAT3& modelForward) {
		ValidateRocketSettings(settings);
		if (!model)
			throw std::invalid_argument("rocket model is required");
		if (!m_rockets.empty())
			throw std::logic_error("Cannot reconfigure active rockets");
		const float length = XMVectorGetX(XMVector3Length(XMLoadFloat3(&modelForward)));
		if (!std::isfinite(length) || length < 0.001f)
			throw std::invalid_argument("modelForward must be finite and nonzero");
		m_settings = settings;
		m_model = std::move(model);
		XMStoreFloat3(&m_modelForward, XMVector3Normalize(XMLoadFloat3(&modelForward)));
		if (const auto bounds = m_model->GetLocalBounds()) {
			const auto extent = XMLoadFloat3(&bounds->maximum) - XMLoadFloat3(&bounds->minimum);
			m_tailDistance =
				XMVectorGetX(XMVector3Dot(XMVectorAbs(XMLoadFloat3(&m_modelForward)), extent)) * 0.5f;
		}
		m_rockets.reserve(settings.maxRockets);
	}
	bool RocketSystem::Fire(Engine::Scene& scene, const XMFLOAT3& muzzle, const XMFLOAT4X4& aircraftWorld,
		const XMFLOAT3& forward, const XMFLOAT3& aircraftVelocity, Engine::ObjectId targetId) {
		if (!m_model || m_rockets.size() >= m_settings.maxRockets)
			return false;
		RocketState rocket;
		XMStoreFloat3(
			&rocket.position, XMVector3TransformCoord(XMLoadFloat3(&muzzle), XMLoadFloat4x4(&aircraftWorld)));
		XMStoreFloat3(
			&rocket.velocity, XMLoadFloat3(&aircraftVelocity) + XMLoadFloat3(&forward) * m_settings.speed);
		if (rocket.position.y <= 0 || XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&rocket.velocity))) < 1e-6f)
			return false;
		rocket.targetId = targetId;
		rocket.remainingLifetime = m_settings.lifetimeSeconds;
		rocket.objectId = scene.AddObject(m_model, MakeWorld(rocket));
		m_rockets.push_back(rocket);
		return true;
	}
	XMFLOAT3 RocketSystem::Steer(
		const XMFLOAT3& velocity, const XMFLOAT3& desiredDirection, float maximumRadians) {
		const auto value = XMLoadFloat3(&velocity), desired = XMLoadFloat3(&desiredDirection);
		if (XMVectorGetX(XMVector3LengthSq(desired)) < 1e-8f ||
			XMVectorGetX(XMVector3LengthSq(value)) < 1e-8f)
			return velocity;
		XMFLOAT3 result;
		XMStoreFloat3(&result,
			XMVector3TransformNormal(value,
				RotationBetween(XMVector3Normalize(value), XMVector3Normalize(desired), maximumRadians)));
		return result;
	}
	bool RocketSystem::Advance(RocketState& rocket, float deltaSeconds,
		std::span<const CollisionTarget> collisions, std::span<const CombatTarget> targets) {
		float elapsed = 0;
		while (elapsed < deltaSeconds && rocket.remainingLifetime > 0) {
			const float step = std::min({kStepSeconds, deltaSeconds - elapsed, rocket.remainingLifetime});
			if (step <= 0)
				break;
			const auto target = std::find_if(targets.begin(), targets.end(),
				[&](const CombatTarget& item) { return item.id == rocket.targetId && item.health > 0; });
			if (rocket.targetId != Engine::InvalidObjectId && target == targets.end())
				rocket.targetId = Engine::InvalidObjectId;
			if (target != targets.end()) {
				auto center = XMLoadFloat3(&target->center);
				const auto box = std::find_if(collisions.begin(), collisions.end(),
					[&](const CollisionTarget& item) { return item.id == target->id; });
				if (box != collisions.end()) {
					const auto movement = XMVectorSet(box->world._41 - box->previousPosition.x,
						box->world._42 - box->previousPosition.y, box->world._43 - box->previousPosition.z,
						0);
					center -= movement * (1 - elapsed / deltaSeconds);
				}
				XMFLOAT3 desired;
				XMStoreFloat3(&desired, center - XMLoadFloat3(&rocket.position));
				rocket.velocity = Steer(
					rocket.velocity, desired, XMConvertToRadians(m_settings.turnDegreesPerSecond) * step);
			}
			const auto start = rocket.position, startTail = Tail(rocket);
			float valid = step;
			bool sea = rocket.position.y <= 0;
			if (rocket.velocity.y < 0 && rocket.position.y + rocket.velocity.y * step <= 0) {
				valid = std::max(0.0f, -rocket.position.y / rocket.velocity.y);
				sea = true;
			}
			XMFLOAT3 end;
			XMStoreFloat3(&end, XMLoadFloat3(&start) + XMLoadFloat3(&rocket.velocity) * valid);
			std::optional<float> first;
			Engine::ObjectId hit = Engine::InvalidObjectId;
			for (const auto& box : collisions) {
				const auto contact = IntersectMovingTarget(
					start, end, box, elapsed / deltaSeconds, (elapsed + valid) / deltaSeconds);
				if (contact && (!first || *contact < *first || (*contact == *first && box.id < hit))) {
					first = contact;
					hit = box.id;
				}
			}
			if (first)
				valid *= *first;
			XMStoreFloat3(&rocket.position, XMLoadFloat3(&start) + XMLoadFloat3(&rocket.velocity) * valid);
			const auto endTail = Tail(rocket);
			// フレーム数ではなく時間間隔で煙を放出し、低FPSでも軌跡の密度を保つ
			while (rocket.trailCountdown <= valid + 1e-7f) {
				const float fraction = valid > 0 ? std::clamp(rocket.trailCountdown / valid, 0.0f, 1.0f) : 0;
				XMFLOAT3 point;
				XMStoreFloat3(
					&point, XMVectorLerp(XMLoadFloat3(&startTail), XMLoadFloat3(&endTail), fraction));
				m_trail.push_back({point, std::max(0.0f, deltaSeconds - elapsed - rocket.trailCountdown)});
				rocket.trailCountdown += kTrailIntervalSeconds;
			}
			rocket.trailCountdown -= valid;
			if (first || sea) {
				XMFLOAT3 direction;
				XMStoreFloat3(&direction, XMVector3Normalize(XMLoadFloat3(&rocket.velocity)));
				m_hits.push_back({hit, elapsed + valid, rocket.position, direction, ProjectileKind::Rocket});
				return false;
			}
			rocket.remainingLifetime -= step;
			elapsed += step;
		}
		return rocket.remainingLifetime > 1e-6f;
	}
	void RocketSystem::Update(Engine::Scene& scene, float deltaSeconds,
		std::span<const CollisionTarget> collisions, std::span<const CombatTarget> targets) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0)
			return;
		const float step = std::min(deltaSeconds, 0.25f);
		std::erase_if(m_rockets, [&](RocketState& rocket) {
			if (!scene.FindObject(rocket.objectId))
				return true;
			if (!Advance(rocket, step, collisions, targets)) {
				scene.RemoveObject(rocket.objectId);
				return true;
			}
			scene.FindObject(rocket.objectId)->worldTransform = MakeWorld(rocket);
			return false;
		});
	}
	XMFLOAT4X4 RocketSystem::MakeWorld(const RocketState& rocket) const {
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world,
			RotationBetween(
				XMLoadFloat3(&m_modelForward), XMVector3Normalize(XMLoadFloat3(&rocket.velocity))) *
				XMMatrixTranslation(rocket.position.x, rocket.position.y, rocket.position.z));
		return world;
	}
	XMFLOAT3 RocketSystem::Tail(const RocketState& rocket) const {
		XMFLOAT3 result;
		XMStoreFloat3(&result,
			XMLoadFloat3(&rocket.position) -
				XMVector3Normalize(XMLoadFloat3(&rocket.velocity)) * m_tailDistance);
		return result;
	}
	std::vector<RocketExhaust> RocketSystem::GetExhausts() const {
		std::vector<RocketExhaust> result;
		result.reserve(m_rockets.size());
		for (const auto& rocket : m_rockets) {
			XMFLOAT3 direction;
			XMStoreFloat3(&direction, XMVector3Normalize(XMLoadFloat3(&rocket.velocity)));
			result.push_back({Tail(rocket), direction});
		}
		return result;
	}
} // namespace Game
