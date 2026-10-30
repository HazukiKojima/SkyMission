#include "ProjectileSystem.h"
#include <algorithm>
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/ProjectileCollision.h"

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

	bool ProjectileSystem::MoveAndCollide(Projectile& projectile, float duration, float frameOffset,
        float frameSeconds, std::span<const CollisionTarget> targets) {
        if (projectile.position.y <= 0 || projectile.remainingLifetime <= 0) return false;
        float valid = std::min(duration, projectile.remainingLifetime);
        if (projectile.velocity.y < 0) valid = std::min(valid, -projectile.position.y / projectile.velocity.y);
        const auto start = projectile.position;
        Advance(projectile, valid);
        std::optional<float> first;
        Engine::ObjectId hit = Engine::InvalidObjectId;
        for (const auto& target : targets) {
            const float begin = frameSeconds > 0 ? frameOffset/frameSeconds : 1;
            const float end = frameSeconds > 0 ? (frameOffset+valid)/frameSeconds : 1;
            const auto contact = IntersectMovingTarget(start, projectile.position, target, begin, end);
            if (contact && (!first || *contact < *first || (*contact == *first && target.id < hit))) {
                first = contact; hit = target.id;
            }
        }
        if (first) {
            XMFLOAT3 position;
            XMStoreFloat3(&position, XMVectorLerp(XMLoadFloat3(&start), XMLoadFloat3(&projectile.position), *first));
            m_hits.push_back({hit, frameOffset + *first*valid, position}); return false;
        }
        return valid >= duration && projectile.remainingLifetime > 0 && projectile.position.y > 0;
    }

    void ProjectileSystem::Update(Engine::Scene& scene, float dt, std::span<const CollisionTarget> targets) {
        if (!std::isfinite(dt) || dt <= 0) return;
        dt = std::min(dt, 0.25f);
        std::erase_if(m_projectiles, [&](Projectile& projectile) {
            const bool alive = MoveAndCollide(projectile, dt, 0, dt, targets);
            auto* object = scene.FindObject(projectile.objectId);
            if (!alive || !object) { scene.RemoveObject(projectile.objectId); return true; }
            object->worldTransform._41 = projectile.position.x;
            object->worldTransform._42 = projectile.position.y;
            object->worldTransform._43 = projectile.position.z;
            return false;
        });
    }

    bool ProjectileSystem::FireVolley(Engine::Scene& scene, const std::array<XMFLOAT3, 4>& muzzles,
        const XMFLOAT4X4& aircraftWorld, const XMFLOAT4X4& aircraftPose,
        const XMFLOAT3& forward, const XMFLOAT3& aircraftVelocity, float age,
        const AimAssistance& assistance, std::span<const CollisionTarget> targets, float frameSeconds) {
        if (!m_model || m_settings.maxProjectiles - m_projectiles.size() < 4) return false;
        auto aircraftRotation = XMLoadFloat4x4(&aircraftPose);
        aircraftRotation.r[3] = XMVectorSet(0, 0, 0, 1);
        for (const auto& muzzle : muzzles) {
            Projectile projectile{};
            XMStoreFloat3(&projectile.position, XMVector3TransformCoord(XMLoadFloat3(&muzzle), XMLoadFloat4x4(&aircraftWorld)));
            XMFLOAT3 direction = forward;
            if (assistance.target) {
                const auto aim = SolveGunAim(projectile.position, aircraftVelocity, *assistance.target,
                    m_settings.muzzleSpeed, m_settings.lifetime);
                if (aim && IsWithinAngle(forward, aim->direction, assistance.angleDegrees)) direction = aim->direction;
            }
            XMStoreFloat3(&projectile.velocity, XMLoadFloat3(&aircraftVelocity) + XMLoadFloat3(&direction)*m_settings.muzzleSpeed);
            projectile.remainingLifetime = m_settings.lifetime;
            if (!MoveAndCollide(projectile, age, std::max(0.0f, frameSeconds-age), frameSeconds, targets)) continue;
            // ロールを維持しつつ、弾モデルの機首方向を補正後の射線へ合わせる
            const auto from = XMLoadFloat3(&forward), to = XMLoadFloat3(&direction);
            const float dot = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
            auto correction = XMMatrixIdentity();
            if (dot < 0.999999f) correction = XMMatrixRotationAxis(XMVector3Cross(from, to), std::acos(dot));
            XMFLOAT4X4 transform;
            XMStoreFloat4x4(&transform, XMLoadFloat4x4(&m_modelRotation)*aircraftRotation*correction*
                XMMatrixTranslation(projectile.position.x, projectile.position.y, projectile.position.z));
            projectile.objectId = scene.AddObject(m_model, transform);
            m_projectiles.push_back(projectile);
        }
        return true;
    }
}
