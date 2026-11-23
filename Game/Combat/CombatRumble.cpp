#include "CombatRumble.h"
#include "CombatTypes.h"
#include "Game/Aircraft/AircraftFlightModel.h"
#include <algorithm>
#include <cmath>

namespace Game {
	void CombatRumble::Configure(const RumbleSettings& settings, float playerMaximumSpeed) {
		m_settings = settings;
		m_playerMaximumSpeed = playerMaximumSpeed;
		Reset();
	}

	void CombatRumble::Reset() {
		m_gunRumbleSeconds = m_missileRumbleSeconds = m_damageRumbleSeconds = 0;
		m_gunRumblePhaseSeconds = 0;
		m_gunRumblePlayed = false;
		m_killRumbleSeconds = 0;
		m_carrierRumbleSeconds = 0;
		m_accelerationRumble = m_brakingRumble = 0;
	}

	RumbleMotors CombatRumble::Update(float deltaSeconds, bool active, const AircraftInput& flight,
		const CombatSnapshot& combat, bool playerTookDamage) {
		if (!active || combat.audio.restarted) {
			Reset();
			return {};
		}

		// 機銃の開始振動は撃ち始めに一度だけ出し、射撃をやめると次の開始を受け付ける
		m_gunRumbleSeconds = std::max(0.0f, m_gunRumbleSeconds - deltaSeconds);
		if (combat.firedVolleys && !m_gunRumblePlayed) {
			m_gunRumbleSeconds = m_settings.gunStart.durationSeconds;
			m_gunRumblePlayed = true;
		}
		if (!combat.audio.gunFiring) m_gunRumblePlayed = false;
		m_missileRumbleSeconds = combat.audio.missileLaunches ? m_settings.missileLaunch.durationSeconds :
			std::max(0.0f, m_missileRumbleSeconds - deltaSeconds);
		m_damageRumbleSeconds = playerTookDamage ? m_settings.playerHit.durationSeconds :
			std::max(0.0f, m_damageRumbleSeconds - deltaSeconds);
		m_killRumbleSeconds = combat.enemyAircraftDestroyed ? m_settings.enemyDestroyed.durationSeconds :
			std::max(0.0f, m_killRumbleSeconds - deltaSeconds);
		const float carrierRumbleDuration = m_settings.carrierDestroyed.durationSeconds;
		m_carrierRumbleSeconds = combat.audio.shipExplosions ? carrierRumbleDuration :
			std::max(0.0f, m_carrierRumbleSeconds - deltaSeconds);
		const float carrierRumble = carrierRumbleDuration > 0 ? m_carrierRumbleSeconds / carrierRumbleDuration : 0.0f;

		const bool flying = combat.audio.flying && combat.result == CombatResult::Active && !combat.cinematic.active;
		const bool highG = flying && flight.highGTurn &&
			std::max(std::abs(flight.turn), std::abs(flight.yaw)) > 0.05f;
		const float acceleration = std::max(0.0f, flight.throttle);
		const float braking = std::max(0.0f, -flight.throttle);
		const float speedRatio = std::clamp(combat.playerSpeedMetersPerSecond / m_playerMaximumSpeed, 0.0f, 1.0f);
		// 加速の低い振動はゆっくり立ち上げ、減速時は速度に応じた細かい振動を返す。
		m_accelerationRumble = flying ?
			std::lerp(m_accelerationRumble, acceleration,
				1.0f - std::exp(-m_settings.acceleration.responsePerSecond * deltaSeconds)) : 0.0f;
		m_brakingRumble = flying ?
			std::lerp(m_brakingRumble, braking * speedRatio,
				1.0f - std::exp(-m_settings.braking.responsePerSecond * deltaSeconds)) : 0.0f;
		const float engineRumble = m_accelerationRumble > 0.01f ? m_accelerationRumble : 0.0f;
		const float airflowRumble = m_brakingRumble > 0.01f ? m_brakingRumble : 0.0f;
		const bool gunStartPulse = m_gunRumbleSeconds > 0;
		const float gunRumbleInterval = m_settings.gunRepeatIntervalSeconds;
		// 開始振動が終わったら、連射が続く間は各周期の末尾で短く振動させる
		const bool repeatGunRumble = m_gunRumblePlayed && !gunStartPulse;
		m_gunRumblePhaseSeconds = repeatGunRumble ?
			std::fmod(m_gunRumblePhaseSeconds + deltaSeconds, gunRumbleInterval) : 0.0f;
		const bool gunRepeatPulse = repeatGunRumble &&
			m_gunRumblePhaseSeconds >= gunRumbleInterval - m_settings.gunRepeat.durationSeconds;
		const RumbleMotors gun = gunStartPulse ? m_settings.gunStart.motors :
			(gunRepeatPulse ? m_settings.gunRepeat.motors : RumbleMotors{});
		const RumbleMotors turn = highG ? m_settings.highG : RumbleMotors{};
		const RumbleMotors missile = m_missileRumbleSeconds > 0 ? m_settings.missileLaunch.motors : RumbleMotors{};
		const RumbleMotors damage = m_damageRumbleSeconds > 0 ? m_settings.playerHit.motors : RumbleMotors{};
		const RumbleMotors kill = m_killRumbleSeconds > 0 ? m_settings.enemyDestroyed.motors : RumbleMotors{};
		RumbleMotors output;
		// 効果が重なったときはモーターごとに最も強い振動を採用する
		const auto mix = [&](RumbleMotors motors, float lowScale = 1.0f, float highScale = 1.0f) {
			output.lowFrequency = std::max(output.lowFrequency, motors.lowFrequency * lowScale);
			output.highFrequency = std::max(output.highFrequency, motors.highFrequency * highScale);
		};
		mix(gun);
		mix(turn);
		mix(missile);
		mix(damage);
		mix(kill);
		mix(m_settings.acceleration.motors, engineRumble, engineRumble);
		mix(m_settings.braking.motors, airflowRumble, airflowRumble);
		mix(m_settings.carrierDestroyed.motors, carrierRumble, carrierRumble * carrierRumble);
		return output;
	}
} // namespace Game
