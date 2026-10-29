#pragma once
#include "Engine/Scene/Scene.h"
#include "WeaponDefinition.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {
	class ProjectileSystem {
	public:
		void Configure(const GunSettings& settings, std::shared_ptr<Engine::Model> model,
			const DirectX::XMFLOAT3& modelForward);
		void Update(Engine::Scene& scene, float dt, std::span<const CollisionTarget> targets = {});
		void FireVolley(Engine::Scene& scene, const std::array<DirectX::XMFLOAT3, 4>& muzzles,
			const DirectX::XMFLOAT4X4& aircraftWorld, const DirectX::XMFLOAT4X4& aircraftPose,
			const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& aircraftVelocity, float age,
			const AimAssistance& assistance = {}, std::span<const CollisionTarget> targets = {}, float frameSeconds = 0);
		std::vector<HitEvent> TakeHits() { auto hits = std::move(m_hits); m_hits.clear(); return hits; }
		size_t GetCount() const { return m_projectiles.size(); }
	private:
		struct Projectile {
			Engine::ObjectId objectId;
			DirectX::XMFLOAT3 position, velocity;
			float remainingLifetime;
		};
		static void Advance(Projectile& projectile, float dt);
		bool MoveAndCollide(Projectile& projectile, float duration, float frameOffset, float frameSeconds,
			std::span<const CollisionTarget> targets);
		GunSettings m_settings;
		std::shared_ptr<Engine::Model> m_model;
		DirectX::XMFLOAT4X4 m_modelRotation;
		std::vector<Projectile> m_projectiles;
		std::vector<HitEvent> m_hits;
	};
}
