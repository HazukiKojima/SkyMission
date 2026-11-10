#pragma once
#include "Game/Settings/GameplaySettings.h"
#include "BombFlight.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {
	// 母艦への誘導ミサイルを管理する。2発を使い切った後に再装填する
	class CarrierMissileSystem {
	  public:
		void ConfigureSettings(const MissileSettings& settings) {
			m_settings = settings;
		}
		void Configure(
			float reloadSeconds, std::shared_ptr<Engine::Model> model, const DirectX::XMFLOAT3& modelForward);
		void ConfigureFlight(float speed, float damage);
		// 次の弾の発射位置と初速を返す。再装填中は装填後の左側を予測する
		CarrierMissileFlight BuildLaunchFlight(const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& forward) const;
		bool TryLaunch(Engine::Scene& scene, const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& forward, Engine::ObjectId target);
		void Update(Engine::Scene& scene, float deltaSeconds, const std::optional<CollisionTarget>& hull);
		unsigned GetAmmo() const {
			return m_ammo;
		}
		float GetCooldownSeconds() const {
			return m_cooldownSeconds;
		}
		size_t GetCount() const {
			return m_missiles.size();
		}
		static size_t RequiredModelCapacity(float reloadSeconds, const MissileSettings& settings = {});
		// 通知は取り出すと消費される
		std::vector<HitEvent> TakeHits();
		std::vector<DirectX::XMFLOAT3> TakeMissImpacts();
		std::vector<DirectX::XMFLOAT3> TakeTrail() {
			auto result = std::move(m_trail);
			m_trail.clear();
			return result;
		}

	  private:
		struct Missile {
			Engine::ObjectId id;
			Engine::ObjectId target;
			CarrierMissileFlight flight;
			float remainingSeconds = CarrierMissileFlight::lifetimeSeconds;
			float smokeSeconds = 0;
		};
		MissileSettings m_settings;
		float m_missileSpeed = 6000;
		float m_missileDamage = 150;
		unsigned m_ammo = 2;
		std::vector<Missile> m_missiles;
		std::vector<DirectX::XMFLOAT3> m_trail;
		std::shared_ptr<Engine::Model> m_model;
		DirectX::XMFLOAT3 m_modelForward{1, 0, 0};
		float m_reloadSeconds = 4;
		float m_cooldownSeconds = 0;
		std::vector<HitEvent> m_hits;
		std::vector<DirectX::XMFLOAT3> m_missImpacts;
	};
} // namespace Game
