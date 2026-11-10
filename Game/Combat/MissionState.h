#pragma once
#include "Game/Settings/SettingsReader.h"
#include <cstddef>
#include <algorithm>
#include <initializer_list>
#include <cmath>
#include <stdexcept>

namespace Game {
	enum class MissionPhase { Patrol, CarrierAttack, FinalFighters, Commander, Complete, Failed };
	struct MissionSettings {
		size_t patrolCount = 4;
		size_t finalFighterCount = 4;
		float bombReloadSeconds = 4;
		float carrierMissileSpeed = 6000;
		float carrierHealth = 300;
		float bombDamage = 150;
		float commanderHealthMultiplier = 1.5f;
		float enemyGunSpeed = 7500;
		float enemyGunInterval = 0.10f;
		float commanderGunInterval = 0.08f;
		float aaRange = 6500;
		float aaInterval = 0.8f;
		size_t maxHostileProjectiles = 512;
		float deckHeightFraction = 0.445f;
		float deckWidthFraction = 0.75f;
		float deckLengthFraction = 0.9f;
		float combatAreaDistanceMeters = 8500.0f;
		float carrierCombatAreaDistanceMeters = 6500.0f;
		float minimumCombatAltitudeMeters = 1000.0f;
		float patrolApproachDistanceMeters = 11000.0f;
		float formationClearanceMeters = 350.0f;
		float maximumOpeningDelaySeconds = 1.5f;
		float antiAircraftMaximumLeadSeconds = 2.0f;
		float antiAircraftDispersionMeters = 100.0f;
		float antiAircraftAimOffsetMeters = 50.0f;
		float antiAircraftDispersionRateRadiansPerSecond = 1.3f;
		float formationSpacingMeters = 1500.0f;
		float deckHalfThicknessFraction = 0.015f;
		float antiAircraftMuzzleSpeedMetersPerSecond = 1800.0f;
		float antiAircraftDamage = 8.0f;
		float antiAircraftMuzzleHeightMeters = 220.0f;
		float escortOpeningDelaySeconds = 2.0f;
		float escortOpeningStaggerSeconds = 0.4f;
		float missionMessageDurationSeconds = 6.0f;
		DirectX::XMFLOAT3 playerStartPosition{0, 1500, 0};
		std::array<float, 6> depthOffsetsMeters{-600, 1000, -1300, 500, -200, 1500};
		std::array<float, 6> altitudeOffsetsMeters{150, 650, -250, 1000, 350, -100};
		void LoadTuning(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("combatAreaDistanceMeters", combatAreaDistanceMeters, 0.000001f);
			r.Number("carrierCombatAreaDistanceMeters", carrierCombatAreaDistanceMeters, 0.000001f);
			r.Number("minimumCombatAltitudeMeters", minimumCombatAltitudeMeters, 0.000001f);
			r.Number("patrolApproachDistanceMeters", patrolApproachDistanceMeters, 0.000001f);
			r.Number("formationClearanceMeters", formationClearanceMeters, 0.000001f);
			r.Number("maximumOpeningDelaySeconds", maximumOpeningDelaySeconds, 0);
			r.Number("antiAircraftMaximumLeadSeconds", antiAircraftMaximumLeadSeconds, 0.000001f);
			r.Number("antiAircraftDispersionMeters", antiAircraftDispersionMeters, 0);
			r.Number("antiAircraftAimOffsetMeters", antiAircraftAimOffsetMeters, 0);
			r.Number("antiAircraftDispersionRateRadiansPerSecond", antiAircraftDispersionRateRadiansPerSecond,
				0.000001f);
			r.Number("formationSpacingMeters", formationSpacingMeters, 0.000001f);
			r.Number("deckHalfThicknessFraction", deckHalfThicknessFraction, 0.000001f);
			r.Number(
				"antiAircraftMuzzleSpeedMetersPerSecond", antiAircraftMuzzleSpeedMetersPerSecond, 0.000001f);
			r.Number("antiAircraftDamage", antiAircraftDamage, 0.000001f);
			r.Number("antiAircraftMuzzleHeightMeters", antiAircraftMuzzleHeightMeters, 0.000001f);
			r.Number("escortOpeningDelaySeconds", escortOpeningDelaySeconds, 0);
			r.Number("escortOpeningStaggerSeconds", escortOpeningStaggerSeconds, 0);
			r.Number("missionMessageDurationSeconds", missionMessageDurationSeconds, 0.000001f);
			r.Vector("playerStartPosition", playerStartPosition);
			r.Array("depthOffsetsMeters", depthOffsetsMeters);
			r.Array("altitudeOffsetsMeters", altitudeOffsetsMeters);
			r.Require(deckHalfThicknessFraction <= 1, "deckHalfThicknessFraction", "must be at most 1");
		}
		void Validate() const {
			if (patrolCount < 1 || patrolCount > 8 || finalFighterCount < 1 || finalFighterCount > 8 ||
				maxHostileProjectiles < 4 || maxHostileProjectiles > 512)
				throw std::invalid_argument("Mission aircraft/projectile counts outside valid ranges");
			for (float value : {carrierHealth, bombDamage, bombReloadSeconds, carrierMissileSpeed,
					 commanderHealthMultiplier, enemyGunSpeed, enemyGunInterval, commanderGunInterval,
					 aaRange, aaInterval, deckHeightFraction, deckWidthFraction, deckLengthFraction})
				if (!std::isfinite(value) || value <= 0)
					throw std::invalid_argument("Mission settings must be positive and finite");
			if (deckHeightFraction > 1 || deckWidthFraction > 1 || deckLengthFraction > 1)
				throw std::invalid_argument("Deck fractions must be in (0,1]");
			if (bombReloadSeconds < 1 || bombReloadSeconds > 120)
				throw std::invalid_argument("bombReloadSeconds must be in [1,120]");
		}
	};

	// 母艦の耐久値と敵機の残数で進行を管理し、描画には依存しない
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
		// 母艦攻撃フェーズ中の有効なダメージだけ適用し、適用した場合にtrueを返す
		bool ApplyCarrierDamage(float damage) {
			if (m_phase != MissionPhase::CarrierAttack || IsDeckDisabled() || !std::isfinite(damage) ||
				damage <= 0)
				return false;
			m_carrierHealth = std::max(0.0f, m_carrierHealth - damage);
			return true;
		}
		void ConfirmDeckHit() {
			ApplyCarrierDamage(m_carrierHealth);
		}
		// 1回の呼び出しで最大1段階進め、フェーズが変化した場合にtrueを返す
		// 完了・失敗は終端状態。自機撃墜は敵残数より優先して失敗へ移る
		bool Advance(size_t remainingAircraft, bool playerAlive) {
			if (m_phase == MissionPhase::Complete || m_phase == MissionPhase::Failed) return false;
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
		float m_carrierHealth;
		float m_carrierMaxHealth;
	};
} // namespace Game
