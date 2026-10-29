#include "ProjectileSystem.h"
#include <algorithm>

namespace Game {
	using namespace DirectX;

	void ProjectileSystem::Configure(const GunSettings& settings, std::shared_ptr<Engine::Model> model,
		const XMFLOAT3& modelForward) {
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
	}

	void ProjectileSystem::Advance(Projectile& projectile, float dt) {
		projectile.position.x += projectile.velocity.x * dt;
		projectile.position.y += projectile.velocity.y * dt;
		projectile.position.z += projectile.velocity.z * dt;
		projectile.remainingLifetime -= dt;
	}

	void ProjectileSystem::Update(Engine::Scene& scene, float dt) {
		if (!std::isfinite(dt) || dt <= 0)
			return;
		dt = std::min(dt, 0.25f);
		std::erase_if(m_projectiles, [&](Projectile& projectile) {
			Advance(projectile, dt);
			auto* object = scene.FindObject(projectile.objectId);
			if (!object || projectile.remainingLifetime <= 0 || projectile.position.y <= 0) {
				scene.RemoveObject(projectile.objectId);
				return true;
			}
			object->worldTransform._41 = projectile.position.x;
			object->worldTransform._42 = projectile.position.y;
			object->worldTransform._43 = projectile.position.z;
			return false;
		});
	}

	void ProjectileSystem::FireVolley(Engine::Scene& scene, const std::array<XMFLOAT3, 4>& muzzles,
		const XMFLOAT4X4& aircraftWorld, const XMFLOAT4X4& aircraftPose,
		const XMFLOAT3& forward, const XMFLOAT3& aircraftVelocity, float age) {
		if (!m_model || m_settings.maxProjectiles - m_projectiles.size() < 4)
			return;
		const auto velocity = XMLoadFloat3(&aircraftVelocity) + XMLoadFloat3(&forward) * m_settings.muzzleSpeed;
		auto rotation = XMLoadFloat4x4(&aircraftPose);
		rotation.r[3] = XMVectorSet(0, 0, 0, 1);
		for (const auto& muzzle : muzzles) {
			Projectile projectile{};
			XMStoreFloat3(&projectile.position, XMVector3TransformCoord(XMLoadFloat3(&muzzle), XMLoadFloat4x4(&aircraftWorld)));
			XMStoreFloat3(&projectile.velocity, velocity);
			projectile.remainingLifetime = m_settings.lifetime;
			Advance(projectile, age);
			if (projectile.remainingLifetime <= 0 || projectile.position.y <= 0)
				continue;
			XMFLOAT4X4 transform;
			XMStoreFloat4x4(&transform, XMLoadFloat4x4(&m_modelRotation) * rotation *
				XMMatrixTranslation(projectile.position.x, projectile.position.y, projectile.position.z));
			projectile.objectId = scene.AddObject(m_model, transform);
			m_projectiles.push_back(projectile);
		}
	}
}
