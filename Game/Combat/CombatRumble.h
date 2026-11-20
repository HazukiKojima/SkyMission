#pragma once
#include "Game/Settings/RumbleSettings.h"

namespace Game {
	struct AircraftInput;
	struct CombatSnapshot;

	class CombatRumble {
	  public:
		void Configure(const RumbleSettings& settings, float playerMaximumSpeed);
		RumbleMotors Update(float deltaSeconds, bool active, const AircraftInput& flight,
			const CombatSnapshot& combat, bool playerTookDamage);

	  private:
		void Reset();

		RumbleSettings m_settings;
		float m_gunRumbleSeconds = 0;
		float m_gunRumblePhaseSeconds = 0;
		bool m_gunRumblePlayed = false;
		float m_missileRumbleSeconds = 0;
		float m_damageRumbleSeconds = 0;
		float m_killRumbleSeconds = 0;
		float m_carrierRumbleSeconds = 0;
		float m_accelerationRumble = 0;
		float m_brakingRumble = 0;
		float m_playerMaximumSpeed = 1.0f;
	};
} // namespace Game
