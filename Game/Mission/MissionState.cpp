#include "MissionState.h"
#include "Game/Weapons/Bomb/BombFlight.h"

namespace Game {
	void MissionSettings::LoadTuning(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("combatAreaDistanceMeters", combatAreaDistanceMeters);
		r.Number("carrierCombatAreaDistanceMeters", carrierCombatAreaDistanceMeters);
		r.Number("minimumCombatAltitudeMeters", minimumCombatAltitudeMeters);
		r.Number("patrolApproachDistanceMeters", patrolApproachDistanceMeters);
		r.Number("formationClearanceMeters", formationClearanceMeters);
		r.Number("maximumOpeningDelaySeconds", maximumOpeningDelaySeconds, 0);
		r.Number("antiAircraftMaximumLeadSeconds", antiAircraftMaximumLeadSeconds);
		r.Number("antiAircraftDispersionMeters", antiAircraftDispersionMeters, 0);
		r.Number("antiAircraftAimOffsetMeters", antiAircraftAimOffsetMeters, 0);
		r.Number("antiAircraftDispersionRateRadiansPerSecond", antiAircraftDispersionRateRadiansPerSecond);
		r.Number("formationSpacingMeters", formationSpacingMeters);
		r.Number("deckHalfThicknessFraction", deckHalfThicknessFraction);
		r.Number("antiAircraftMuzzleSpeedMetersPerSecond", antiAircraftMuzzleSpeedMetersPerSecond);
		r.Number("antiAircraftDamage", antiAircraftDamage);
		r.Number("antiAircraftMuzzleHeightMeters", antiAircraftMuzzleHeightMeters);
		r.Number("escortOpeningDelaySeconds", escortOpeningDelaySeconds, 0);
		r.Number("escortOpeningStaggerSeconds", escortOpeningStaggerSeconds, 0);
		r.Number("missionMessageDurationSeconds", missionMessageDurationSeconds);
		r.Vector("playerStartPosition", playerStartPosition);
		r.Array("depthOffsetsMeters", depthOffsetsMeters);
		r.Array("altitudeOffsetsMeters", altitudeOffsetsMeters);
		r.Require(deckHalfThicknessFraction <= 1, "deckHalfThicknessFraction", "must be at most 1");
	}

	void MissionSettings::Validate() const {
		if (patrolCount < 1 || patrolCount > kMaximumFightersPerWave ||
			finalFighterCount < 1 || finalFighterCount > kMaximumFightersPerWave ||
			maxHostileProjectiles < kMinimumHostileProjectileCapacity ||
			maxHostileProjectiles > kMaximumHostileProjectileCapacity)
			throw std::invalid_argument("Mission aircraft/projectile counts outside valid ranges");

		for (float value : {carrierHealth, bombDamage, bombReloadSeconds, carrierMissileSpeed,
				 commanderHealthMultiplier, enemyGunSpeed, enemyGunInterval, commanderGunInterval, aaRange,
				 aaInterval, deckHeightFraction, deckWidthFraction, deckLengthFraction})
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument("Mission settings must be positive and finite");
		if (deckHeightFraction > 1 || deckWidthFraction > 1 || deckLengthFraction > 1)
			throw std::invalid_argument("Deck fractions must be in (0,1]");
		if (bombReloadSeconds < kMinimumOrdnanceReloadSeconds ||
			bombReloadSeconds > kMaximumOrdnanceReloadSeconds)
			throw std::invalid_argument("bombReloadSeconds must be in [1,120]");
	}

	bool MissionState::ApplyCarrierDamage(float damage) {
		if (m_phase != MissionPhase::CarrierAttack || IsDeckDisabled() || !std::isfinite(damage) ||
			damage <= 0)
			return false;

		m_carrierHealth = std::max(0.0f, m_carrierHealth - damage);
		return true;
	}

	bool MissionState::Advance(size_t remainingAircraft, bool playerAlive) {
		if (m_phase == MissionPhase::Complete || m_phase == MissionPhase::Failed) return false;

		// 自機の撃墜を優先して一段階だけ任務を進める
		const auto before = m_phase;

		if (!playerAlive)
			m_phase = MissionPhase::Failed;
		else if (m_phase == MissionPhase::Patrol && remainingAircraft == 0)
			m_phase = MissionPhase::CarrierAttack;
		else if (m_phase == MissionPhase::CarrierAttack && IsDeckDisabled())
			m_phase = MissionPhase::FinalFighters;
		else if (m_phase == MissionPhase::FinalFighters && remainingAircraft == 0)
			m_phase = MissionPhase::Complete;
		else if (m_phase == MissionPhase::Commander && remainingAircraft == 0)
			m_phase = MissionPhase::Complete;
		return before != m_phase;
	}
} // namespace Game
