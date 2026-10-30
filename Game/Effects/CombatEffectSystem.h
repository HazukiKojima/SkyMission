#pragma once
#include "CombatEffectSettings.h"
#include "CombatEffectTypes.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {
	class CombatEffectSystem {
	  public:
		void Update(float deltaSeconds);
		void SpawnImpact(const HitEvent& hit, const DirectX::XMFLOAT3& targetVelocity);
		void SpawnExplosion(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity);
		void SpawnExplosion(
			const Engine::SceneObject& aircraft, const DirectX::XMFLOAT3& velocity, float visualScale);
		const CombatEffectSnapshot& GetSnapshot() const { return m_snapshot; }

	  private:
		void AddExplosion(ExplosionEffect explosion);
		CombatEffectSnapshot m_snapshot;
	};
} // namespace Game
