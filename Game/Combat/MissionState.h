#pragma once
#include <cstddef>
#include <initializer_list>
#include <cmath>
#include <stdexcept>
#include "Game/Weapons/BombFlight.h"

namespace Game {
	enum class MissionPhase { Patrol, CarrierAttack, FinalFighters, Commander, Complete, Failed };
	struct MissionSettings {
		size_t patrolCount = 2, finalFighterCount = 2;
		float bombReloadSeconds = 10;
		float commanderHealthMultiplier = 1.5f;
		float enemyGunSpeed = 1800;
		float enemyGunInterval = 0.18f, commanderGunInterval = 0.10f;
		float aaRange = 6500, aaInterval = 0.8f;
		size_t maxHostileProjectiles = 128;
		// Hornetモデルで最大面積の水平面がある高さ（モデル全高に対する比率）。
		float deckHeightFraction = 0.445f, deckWidthFraction = 0.75f, deckLengthFraction = 0.9f;
		void Validate() const {
			if (patrolCount < 1 || patrolCount > 8 || finalFighterCount < 1 || finalFighterCount > 8 ||
				maxHostileProjectiles < 4 || maxHostileProjectiles > 512)
				throw std::invalid_argument("Mission aircraft/projectile counts outside valid ranges");
			for (float value : {bombReloadSeconds, commanderHealthMultiplier, enemyGunSpeed, enemyGunInterval,
					 commanderGunInterval, aaRange, aaInterval, deckHeightFraction, deckWidthFraction,
					 deckLengthFraction})
				if (!std::isfinite(value) || value <= 0)
					throw std::invalid_argument("Mission settings must be positive and finite");
			if (deckHeightFraction > 1 || deckWidthFraction > 1 || deckLengthFraction > 1)
				throw std::invalid_argument("Deck fractions must be in (0,1]");
			if (bombReloadSeconds < 1 || bombReloadSeconds > BombTuning::lifetimeSeconds)
				throw std::invalid_argument("bombReloadSeconds must be in [1,120]");
		}
	};

	// Only live aircraft and confirmed deck hits drive progression; no rendering dependencies.
	class MissionState {
	  public:
		MissionPhase GetPhase() const {
			return m_phase;
		}
		bool IsDeckDisabled() const {
			return m_deckDisabled;
		}
		void ConfirmDeckHit() {
			if (m_phase == MissionPhase::CarrierAttack)
				m_deckDisabled = true;
		}
		bool Advance(size_t remainingAircraft, bool playerAlive) {
			if (m_phase == MissionPhase::Complete || m_phase == MissionPhase::Failed)
				return false;
			const auto before = m_phase;
			// 勝利演出中に自機が消える結果を避けるため、同フレームの相打ちは敗北を優先する。
			if (!playerAlive)
				m_phase = MissionPhase::Failed;
			else if (m_phase == MissionPhase::Patrol && remainingAircraft == 0)
				m_phase = MissionPhase::CarrierAttack;
			else if (m_phase == MissionPhase::CarrierAttack && m_deckDisabled)
				m_phase = MissionPhase::FinalFighters;
			else if (m_phase == MissionPhase::FinalFighters && remainingAircraft == 0)
				m_phase = MissionPhase::Commander;
			else if (m_phase == MissionPhase::Commander && remainingAircraft == 0)
				m_phase = MissionPhase::Complete;
			return before != m_phase;
		}

	  private:
		MissionPhase m_phase = MissionPhase::Patrol;
		bool m_deckDisabled = false;
	};
} // namespace Game
