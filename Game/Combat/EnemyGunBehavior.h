#pragma once
#include "Game/Settings/GameplaySettings.h"
#include "GunAimSolver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Game {
	inline constexpr float EnemyGunLifetimeSeconds = 3.0f;

	float EnemyVariation(size_t slot, uint32_t event, uint32_t salt);

	class EnemyGunPattern {
	  public:
		bool Update(float dt, size_t slot, const EnemyGunTuning& tuning = {});

		uint32_t GetShotIndex() const {
			return m_shots;
		}

		void NotifyShot() {
			++m_shots;
		}

		float Interval(float base, size_t slot) const {
			return base * (0.85f + EnemyVariation(slot, m_shots, 37) * 0.4f);
		}

	  private:
		float m_remaining = 0;
		uint32_t m_sequence = 0, m_shots = 0;
		bool m_initialized = false, m_burst = false;
	};

	DirectX::XMFLOAT3 EnemyGunDirection(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& velocity, const CombatTarget& target, float speed, size_t slot,
		uint32_t shot, const EnemyGunTuning& tuning = {});

	std::array<DirectX::XMFLOAT3, 2> SpreadEnemyGunDirections(
		const DirectX::XMFLOAT3& center, size_t slot, uint32_t shot, const EnemyGunTuning& tuning = {});
} // namespace Game
