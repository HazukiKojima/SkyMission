#pragma once
#include "BombFlight.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {

	class BombSystem {
	  public:
		void Configure(
			float reloadSeconds, std::shared_ptr<Engine::Model> model, const DirectX::XMFLOAT3& modelForward);
		void ConfigureMissiles(float speed, float damage);
		bool TryLaunch(Engine::Scene& scene, const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& forward, Engine::ObjectId target);
		unsigned GetAmmo() const { return m_ammo; }
		std::vector<DirectX::XMFLOAT3> TakeTrail() { auto result = std::move(m_trail); m_trail.clear(); return result; }
		void Update(Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& deck);
		bool TryDrop(Engine::Scene& scene, const DirectX::XMFLOAT3& aircraftPosition,
			const DirectX::XMFLOAT3& aircraftVelocity);
		static BombFlight ReleaseFlight(DirectX::XMFLOAT3 position, const DirectX::XMFLOAT3& velocity);
		static size_t RequiredModelCapacity(float reloadSeconds);
		float GetCooldownSeconds() const {
			return m_cooldownSeconds;
		}
		size_t GetCount() const {
			return m_bombs.size() + m_missiles.size();
		}
		std::vector<HitEvent> TakeHits();
		std::vector<DirectX::XMFLOAT3> TakeMissImpacts();

	  private:
		struct Missile {
			Engine::ObjectId id, target;
			CarrierMissileFlight flight;
			float remaining = 8, smokeSeconds = 0;
		};
		void UpdateMissiles(Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& hull);
		bool m_missileMode = false;
		float m_missileSpeed = 6000, m_missileDamage = 150;
		unsigned m_ammo = 2;
		std::vector<Missile> m_missiles;
		std::vector<DirectX::XMFLOAT3> m_trail;
		struct Bomb {
			Engine::ObjectId id;
			BombFlight flight;
			float remainingSeconds = BombTuning::lifetimeSeconds;
		};
		DirectX::XMFLOAT4X4 BuildTransform(const BombFlight& flight) const;
		std::shared_ptr<Engine::Model> m_model;
		DirectX::XMFLOAT3 m_modelForward{1, 0, 0};
		float m_reloadSeconds = 10, m_cooldownSeconds = 0;
		std::vector<Bomb> m_bombs;
		std::vector<HitEvent> m_hits;
		std::vector<DirectX::XMFLOAT3> m_missImpacts;
	};
} // namespace Game
