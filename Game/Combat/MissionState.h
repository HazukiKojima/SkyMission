#pragma once
#include <cstddef>
#include <algorithm>
#include <initializer_list>
#include <cmath>
#include <stdexcept>

namespace Game {
	enum class MissionPhase { Patrol, CarrierAttack, FinalFighters, Commander, Complete, Failed };
	struct MissionSettings {
		size_t patrolCount = 2, finalFighterCount = 2;
		float bombReloadSeconds = 10;
		float carrierHealth = 300, bombDamage = 100;
		float commanderHealthMultiplier = 1.5f;
		float enemyGunSpeed = 1800;
		float enemyGunInterval = 0.18f, commanderGunInterval = 0.10f;
		float aaRange = 6500, aaInterval = 0.8f;
		size_t maxHostileProjectiles = 128;
		float deckHeightFraction = 0.445f, deckWidthFraction = 0.75f, deckLengthFraction = 0.9f;
		void Validate() const {
			if (patrolCount < 1 || patrolCount > 8 || finalFighterCount < 1 || finalFighterCount > 8 ||
				maxHostileProjectiles < 4 || maxHostileProjectiles > 512)
				throw std::invalid_argument("Mission aircraft/projectile counts outside valid ranges");
			for (float value : {carrierHealth, bombDamage, bombReloadSeconds, commanderHealthMultiplier,
					 enemyGunSpeed, enemyGunInterval, commanderGunInterval, aaRange, aaInterval,
					 deckHeightFraction, deckWidthFraction, deckLengthFraction})
				if (!std::isfinite(value) || value <= 0)
					throw std::invalid_argument("Mission settings must be positive and finite");
			if (deckHeightFraction > 1 || deckWidthFraction > 1 || deckLengthFraction > 1)
				throw std::invalid_argument("Deck fractions must be in (0,1]");
			if (bombReloadSeconds < 1 || bombReloadSeconds > 120)
				throw std::invalid_argument("bombReloadSeconds must be in [1,120]");
		}
	};

	// 母艦の耐久値と敵機の残数で進行を管理し、描画には依存しない。
	class MissionState {
	  public:
		explicit MissionState(float carrierHealth = 300)
			: m_carrierHealth(carrierHealth), m_carrierMaxHealth(carrierHealth) {
			if (!std::isfinite(carrierHealth) || carrierHealth <= 0)
				throw std::invalid_argument("Carrier health must be positive and finite");
		}
		float GetCarrierHealth() const {
			return m_carrierHealth;
		}
		float GetCarrierMaxHealth() const {
			return m_carrierMaxHealth;
		}
		MissionPhase GetPhase() const {
			return m_phase;
		}
		bool IsDeckDisabled() const {
			return m_carrierHealth <= 0;
		}
		bool ApplyCarrierDamage(float damage) {
			if (m_phase != MissionPhase::CarrierAttack || IsDeckDisabled() || !std::isfinite(damage) ||
				damage <= 0)
				return false;
			m_carrierHealth = std::max(0.0f, m_carrierHealth - damage);
			return true;
		}
		bool Advance(size_t remainingAircraft, bool playerAlive) {
			if (m_phase == MissionPhase::Complete || m_phase == MissionPhase::Failed)
				return false;
			const auto before = m_phase;
			if (!playerAlive)
				m_phase = MissionPhase::Failed;
			else if (m_phase == MissionPhase::Patrol && remainingAircraft == 0)
				m_phase = MissionPhase::CarrierAttack;
			else if (m_phase == MissionPhase::CarrierAttack && IsDeckDisabled())
				m_phase = MissionPhase::FinalFighters;
			else if (m_phase == MissionPhase::FinalFighters && remainingAircraft == 0)
				m_phase = MissionPhase::Commander;
			else if (m_phase == MissionPhase::Commander && remainingAircraft == 0)
				m_phase = MissionPhase::Complete;
			return before != m_phase;
		}

	  private:
		MissionPhase m_phase = MissionPhase::Patrol;
		float m_carrierHealth, m_carrierMaxHealth;
	};
} // namespace Game
