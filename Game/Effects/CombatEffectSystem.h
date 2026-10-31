#pragma once
#include "CombatEffectSettings.h"
#include "CombatEffectTypes.h"
#include "Game/Combat/CombatTypes.h"
#include <array>
#include <span>

namespace Game {
	class CombatEffectSystem {
	  public:
		void Update(float deltaSeconds);
		void SpawnGunShot(const GunShotEvent& shot);
		void SetGunMuzzlePose(
			const std::array<DirectX::XMFLOAT3, 4>& positions, const DirectX::XMFLOAT3& forward);
		void ClearGunFlashes();
		void SetGunTracers(std::vector<GunTracerSegment> tracers);
		void SpawnRocketSmoke(const DirectX::XMFLOAT3& position, float age);
		void SetRocketExhausts(std::vector<RocketExhaust> exhausts);
		void SpawnImpact(const HitEvent& hit, const DirectX::XMFLOAT3& targetVelocity);
		void SpawnExplosion(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity);
		void SpawnExplosion(
			const Engine::SceneObject& aircraft, const DirectX::XMFLOAT3& velocity, float visualScale);
		const CombatEffectSnapshot& GetSnapshot() const {
			return m_snapshot;
		}

	  private:
		void AddExplosion(ExplosionEffect explosion);
		CombatEffectSnapshot m_snapshot;
	};
} // namespace Game
