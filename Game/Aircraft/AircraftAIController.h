#pragma once

#include "AircraftFlightModel.h"
#include "Game/Combat/EnemyGunBehavior.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <optional>

namespace Game {
	struct AircraftAIParameters {
		float directionChangeSeconds = 5.0f;
		float altitudeChangeSeconds = 12.0f;
		float lowAltitude = 100.0f;
		float highAltitude = 400.0f;
		float pitchLookAheadDistance = 800.0f;
		float turnGain = 1.5f;
		float pitchGain = 1.5f;
		float speedMatchGain = 0.015f;
		float highGManeuverIntervalSeconds = 24.0f;
		float highGManeuverDurationSeconds = 3.0f;
		float reactionSeconds = 0.3f;
		float maneuverSeconds = 1.8f;
		float disengageDistance = 350.0f;
		float threatDistance = 700.0f;
		float attackDistance = 3000.0f;
		float aimAngleDegrees = 3.0f;
		float aimHoldSeconds = 0.4f;
		float openingGraceSeconds = 3.0f;
		float aimErrorDegrees = 0.65f;
		float cruiseSpeed = 340.0f;
		float attackSpeed = 260.0f;
		float escapeSpeed = 520.0f;
		float reengageDistance = 2500.0f;
		float attackRunSeconds = 7.0f;
		float separationLookAheadSeconds = 0.8f;
		float tailDistance = 1000.0f;
		float flankDistance = 650.0f;
		float pursuitPredictionSeconds = 1.5f;
		float maximumDisengageSeconds = 3.0f;
		float repositionSeconds = 12.0f;
	};
	enum class AircraftCombatMode { Pursuit, Attack, Disengage, Evade, Recover, Reengage, ReturnToCarrier };
	struct AircraftAITarget {
		DirectX::XMFLOAT3 position{}, velocity{}, forward{0, 0, -1};
	};
	struct AircraftAIDecision {
		AircraftInput flight;
		bool fire = false;
		bool locking = false;
		float lockProgress = 0;
	};

	class AircraftAIController {
	  public:
		AircraftCombatMode GetMode() const {
			return m_mode;
		}
		// A shot resets acquisition, but does not interrupt the attack run.
		void NotifyWeaponFired() { m_aimTime = 0; }
		AircraftAIDecision ReadCombatInput(const AircraftFlightModel& aircraft,
			const AircraftAITarget& target, const DirectX::XMFLOAT3& aimDirection, float dt,
			const AircraftAIParameters& p, size_t slot, bool attackAllowed, bool weaponReady = true,
			std::optional<DirectX::XMFLOAT3> defenseCenter = std::nullopt, bool opportunisticGun = false) {
			using namespace DirectX;
			AircraftAIDecision result;
			if (!std::isfinite(dt) || dt <= 0) return result;
			dt = std::min(dt, 0.25f);
			m_combatTime += dt;
			m_modeSeconds += dt;
			const auto& style = kCombatStyles[slot % std::size(kCombatStyles)];
			const auto position = aircraft.GetPosition(), forward = aircraft.GetForward();
			const auto velocity = aircraft.GetVelocity();
			const auto delta = XMLoadFloat3(&target.position) - XMLoadFloat3(&position);
			const float distance = XMVectorGetX(XMVector3Length(delta));
			const auto toTarget = distance > 0.001f ? delta / distance : XMLoadFloat3(&forward);
			const auto relativeVelocity = XMLoadFloat3(&velocity) - XMLoadFloat3(&target.velocity);
			const float relativeSquared = XMVectorGetX(XMVector3LengthSq(relativeVelocity));
			const float passTime = relativeSquared > 0.001f
				? std::max(0.0f, XMVectorGetX(XMVector3Dot(delta, relativeVelocity)) / relativeSquared) : 0;
			const float passDistance = XMVectorGetX(XMVector3Length(delta - relativeVelocity * passTime));
			const float closestTime = relativeSquared > 0.001f
				? std::clamp(XMVectorGetX(XMVector3Dot(delta, relativeVelocity)) / relativeSquared,
					0.0f, p.separationLookAheadSeconds) : 0;
			const float closest = XMVectorGetX(XMVector3Length(delta - relativeVelocity * closestTime));
			const bool needsSpacing = distance < p.disengageDistance ||
				(closestTime > 0 && closest < p.disengageDistance);
			const float yaw = std::atan2(forward.x, -forward.z);
			XMVECTOR home = XMVectorZero();
			if (defenseCenter) {
				home = XMLoadFloat3(&*defenseCenter) - XMLoadFloat3(&position);
				const float homeDistance = std::hypot(XMVectorGetX(home), XMVectorGetZ(home));
				if (homeDistance > kDefenseOuterRadius) m_returning = true;
				else if (homeDistance <= kDefenseInnerRadius) m_returning = false;
			} else m_returning = false;
			const auto enter = [&](AircraftCombatMode mode) {
				m_mode = mode;
				m_modeSeconds = 0;
				m_collisionAvoidance = false;
				m_spacingBreak = false;
			};
			const auto chooseTurn = [&](XMVECTOR direction) {
				const float error = std::remainder(std::atan2(XMVectorGetX(direction),
					-XMVectorGetZ(direction)) - yaw, XM_2PI);
				m_flankSide = std::abs(error) > 0.05f ? std::copysign(1.0f, error)
					: (slot % 2 == 0 ? 1.0f : -1.0f);
			};
			const auto beginPass = [&] {
				enter(AircraftCombatMode::Disengage);
				m_flankSide = slot % 2 == 0 ? 1.0f : -1.0f;
				if (defenseCenter && std::hypot(XMVectorGetX(home), XMVectorGetZ(home)) > 10000)
					chooseTurn(home);
				const auto maneuver = m_passSequence++;
				m_passMinimum = kMinimumPassSeconds + style.passDelay + EnemyVariation(slot, maneuver, 83) * 0.9f;
				m_passMaximum = std::min(kMaximumPassSeconds, 7.0f + float(slot % 6) * 0.38f +
					EnemyVariation(slot, maneuver, 89) * 0.6f);
				static constexpr float legs[] = {3200, 4700, 3900, 6000, 5200, 6800};
				m_passLeg = (legs[slot % std::size(legs)] + EnemyVariation(slot, maneuver, 97) * 900)
					* (p.tailDistance / 4500.0f);
				m_passStart = position;
				m_reentrySeconds = 1.2f + EnemyVariation(slot, maneuver, 103) * 1.8f;
				const auto targetForward = XMLoadFloat3(&target.forward);
				auto targetRight = XMVector3Cross(targetForward, XMVectorSet(0, 1, 0, 0));
				if (XMVectorGetX(XMVector3LengthSq(targetRight)) < 0.001f) targetRight = XMVectorSet(1, 0, 0, 0);
				static constexpr float lanes[] = {-900, 700, -1400, 1200, -500, 1700};
				XMStoreFloat3(&m_reentryOffset,
					XMVector3Normalize(targetRight) * lanes[slot % std::size(lanes)] *
						(0.8f + EnemyVariation(slot, maneuver, 107) * 0.4f) +
					targetForward * (float(slot % 3) * 600 - 500) + XMVectorSet(0, style.climb * 2000, 0, 0));
				m_turnBurstSeconds = style.highGSeconds * (0.8f + EnemyVariation(slot, maneuver, 101) * 0.4f);
				auto escape = XMVector3TransformNormal(XMLoadFloat3(&forward),
					XMMatrixRotationY(-(style.exitAngle + 0.12f + EnemyVariation(slot, maneuver, 109) * 0.35f) * m_flankSide));
				// A straight-through attack must pass beside the player, not through their position.
				if (passTime > 0 && passDistance < std::min(1800.0f, p.disengageDistance * 0.65f)) {
					const auto right = XMVectorSet(-forward.z, 0, forward.x, 0);
					const float lateral = XMVectorGetX(XMVector3Dot(delta, right));
					const float side = std::abs(lateral) > 100 ? std::copysign(-1.0f, lateral) : m_flankSide;
					const float angle = XMConvertToRadians(55 + float(slot % 6) * 5 +
						EnemyVariation(slot, maneuver, 113) * 5);
					escape = XMLoadFloat3(&forward) * std::cos(angle) + right * (side * std::sin(angle));
					m_spacingBreak = true;
				}
				const float preferredClimb = style.climb + (EnemyVariation(slot, maneuver, 127) - 0.5f) * 0.08f;
				const float climb = position.y < p.lowAltitude + 350
					? std::max(0.0f, preferredClimb) : preferredClimb;
				escape = XMVectorSetY(escape, std::clamp(forward.y + climb, -0.25f, 0.35f));
				XMStoreFloat3(&m_escapeDirection, XMVector3Normalize(escape));
			};
			if (position.y < p.lowAltitude) enter(AircraftCombatMode::Recover);
			else if (m_returning) {
				if (m_mode != AircraftCombatMode::ReturnToCarrier) {
					enter(AircraftCombatMode::ReturnToCarrier);
					chooseTurn(home);
				}
			} else if (m_mode == AircraftCombatMode::Recover || m_mode == AircraftCombatMode::ReturnToCarrier) {
				enter(AircraftCombatMode::Reengage);
				chooseTurn(delta);
			} else if (m_mode == AircraftCombatMode::Disengage) {
				const float traveled = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&position) - XMLoadFloat3(&m_passStart),
					XMLoadFloat3(&m_escapeDirection)));
				if ((m_modeSeconds >= m_passMinimum && traveled >= m_passLeg && distance >= p.disengageDistance * (1.42f + float(slot % 6) * 0.06f)) || m_modeSeconds >= m_passMaximum) {
					enter(AircraftCombatMode::Reengage);
					chooseTurn(delta + XMLoadFloat3(&m_reentryOffset));
				}
			} else if (m_mode == AircraftCombatMode::Reengage) {
				const float ahead = XMVectorGetX(XMVector3Dot(toTarget, XMLoadFloat3(&forward)));
				if (needsSpacing && distance < p.disengageDistance && ahead > 0.3f) {
					beginPass();
				} else if (ahead > 0.85f && m_modeSeconds >= m_reentrySeconds * 0.5f) {
					if (needsSpacing) {
						beginPass();
					} else enter(AircraftCombatMode::Pursuit);
				}
			} else if (needsSpacing) {
				// Commit to a straight exit, with a stable climb/bank preference per aircraft.
				beginPass();
			} else {
				m_mode = attackAllowed && distance <= p.attackDistance
					? AircraftCombatMode::Attack : AircraftCombatMode::Pursuit;
			}
			const bool imminentCollision = closestTime > 0 &&
				closest < std::min(1000.0f, p.disengageDistance * 0.34f) &&
				distance < std::min(3500.0f, p.disengageDistance * 1.2f);
			if (!imminentCollision) m_collisionAvoidance = false;
			m_avoidanceRecheck -= dt;
			if (imminentCollision && m_mode != AircraftCombatMode::Recover &&
				(!m_collisionAvoidance || m_avoidanceRecheck <= 0)) {
				// Collision avoidance may interrupt a straight pass or the return turn.
				const auto right = XMVectorSet(-forward.z, 0, forward.x, 0);
				const float lateral = XMVectorGetX(XMVector3Dot(delta, right));
				const float side = std::abs(lateral) > 1 ? std::copysign(-1.0f, lateral) : m_flankSide;
				XMStoreFloat3(&m_escapeDirection, XMVector3Normalize(
					XMLoadFloat3(&forward) * 0.35f + right * side + XMVectorSet(0, 0.35f, 0, 0)));
				m_collisionAvoidance = true;
				m_avoidanceRecheck = 0.35f;
			}
			const float prediction = std::min(p.pursuitPredictionSeconds,
				distance / std::max(1.0f, aircraft.GetSpeed()));
			XMVECTOR steering = delta + XMLoadFloat3(&target.velocity) * prediction;
			if (m_mode == AircraftCombatMode::Disengage) steering = XMLoadFloat3(&m_escapeDirection);
			else if (m_mode == AircraftCombatMode::ReturnToCarrier) {
				steering = home;
				steering = XMVectorSetY(steering, std::max(p.highAltitude, position.y) - position.y);
			} else if (m_mode == AircraftCombatMode::Recover)
				steering = XMVectorSet(forward.x, 0.7f, forward.z, 0);
			else if (m_mode == AircraftCombatMode::Attack || m_mode == AircraftCombatMode::Reengage) {
				steering = XMLoadFloat3(&aimDirection);
				if (m_mode == AircraftCombatMode::Reengage) {
					const float weight = std::max(0.0f, 1 - m_modeSeconds / m_reentrySeconds);
					steering = steering * std::max(1.0f, distance) + XMLoadFloat3(&m_reentryOffset) * weight;
				}
			}
			if (m_collisionAvoidance && m_mode != AircraftCombatMode::Recover)
				steering = XMLoadFloat3(&m_escapeDirection);
			if (XMVectorGetX(XMVector3LengthSq(steering)) < 0.001f) steering = XMLoadFloat3(&forward);
			XMStoreFloat3(&m_steering, XMVector3Normalize(steering));
			float yawError = std::remainder(std::atan2(m_steering.x, -m_steering.z) - yaw, XM_2PI);
			if ((m_mode == AircraftCombatMode::Reengage || m_mode == AircraftCombatMode::ReturnToCarrier)
				&& !m_collisionAvoidance && yawError * m_flankSide < 0 && std::abs(yawError) > 0.35f)
				yawError += m_flankSide * XM_2PI;
			result.flight.autoLevelRoll = true;
			result.flight.turn = std::abs(yawError) > 0.15f
				? std::clamp(yawError * p.turnGain, -1.0f, 1.0f) : 0;
			if (!m_collisionAvoidance && m_mode != AircraftCombatMode::Recover &&
				m_mode != AircraftCombatMode::ReturnToCarrier)
				result.flight.turn *= style.turnScale;
			result.flight.yaw = std::abs(yawError) <= 0.6f
				? std::clamp(-yawError * p.turnGain * 2, -1.0f, 1.0f) : 0;
			const float pitch = std::atan2(forward.y, std::hypot(forward.x, forward.z));
			const float desiredPitch = std::clamp(std::atan2(m_steering.y,
				std::hypot(m_steering.x, m_steering.z)), -0.6f, 0.6f);
			result.flight.pitch = std::clamp((desiredPitch - pitch) * p.pitchGain, -1.0f, 1.0f);
			float targetSpeed = p.cruiseSpeed;
			if (m_mode == AircraftCombatMode::Disengage ||
				(m_mode == AircraftCombatMode::Pursuit && distance > p.reengageDistance) ||
				(m_mode == AircraftCombatMode::Attack && distance > p.reengageDistance))
				targetSpeed = p.escapeSpeed;
			if (m_mode == AircraftCombatMode::Disengage)
				targetSpeed = std::max(targetSpeed,
					XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.velocity))) + p.disengageDistance / kMinimumPassSeconds);
			targetSpeed = std::min(targetSpeed, aircraft.GetMaximumSpeed());
			result.flight.throttle = std::clamp((targetSpeed - aircraft.GetSpeed()) * p.speedMatchGain, -1.0f, 1.0f);
			// A short reentry burst adds pressure; never sustain high G in a close orbit.
			result.flight.highGTurn = (imminentCollision && m_mode != AircraftCombatMode::Recover) ||
				(m_mode == AircraftCombatMode::Disengage && m_spacingBreak && m_modeSeconds < 1.8f) ||
				(m_mode == AircraftCombatMode::Reengage && m_modeSeconds < (m_passSequence > 0 ? m_turnBurstSeconds : style.highGSeconds) &&
				 distance >= p.disengageDistance && std::abs(yawError) > 0.7f);
			// Finish gun bursts while breaking away; the movement separation threshold stays unchanged.
			const float minimumFireDistance = opportunisticGun
				? std::min(2200.0f, p.disengageDistance) : p.disengageDistance;
			const bool safeShot = attackAllowed && weaponReady && !m_collisionAvoidance &&
				distance >= minimumFireDistance && distance <= p.attackDistance &&
				(m_mode == AircraftCombatMode::Attack || m_mode == AircraftCombatMode::Disengage ||
				 m_mode == AircraftCombatMode::Reengage || m_mode == AircraftCombatMode::ReturnToCarrier) &&
				XMVectorGetX(XMVector3Dot(XMLoadFloat3(&forward), XMLoadFloat3(&aimDirection))) >=
					std::cos(XMConvertToRadians(p.aimAngleDegrees)) && m_combatTime >= p.openingGraceSeconds;
			const bool aligned = safeShot && !m_returning;
			m_aimTime = aligned ? std::min(p.aimHoldSeconds, m_aimTime + dt) : 0;
			result.locking = aligned;
			result.lockProgress = m_aimTime / p.aimHoldSeconds;
			result.fire = (aligned && m_aimTime >= p.aimHoldSeconds) || (m_returning && opportunisticGun && safeShot);
			return result;
		}
		AircraftInput ReadInput(const AircraftFlightModel& aircraft, float targetSpeed, float deltaSeconds,
			const AircraftAIParameters& parameters) {
			AircraftInput input;
			if (deltaSeconds <= 0.0f) {
				return input;
			}
			m_elapsedSeconds += std::min(deltaSeconds, 0.25f);

			const DirectX::XMFLOAT3 position = aircraft.GetPosition();
			const DirectX::XMFLOAT3 heading = aircraft.GetHeading();
			const float currentYaw = std::atan2(heading.x, -heading.z);
			if (!m_initialized) {
				m_targetYaw = currentYaw;
				m_targetAltitude = parameters.highAltitude;
				m_nextDirectionChange = m_elapsedSeconds + parameters.directionChangeSeconds;
				m_nextAltitudeChange = m_elapsedSeconds + parameters.altitudeChangeSeconds;
				m_initialized = true;
			}

			static constexpr float directionChanges[] = {DirectX::XM_PIDIV2, -DirectX::XM_PIDIV2,
				DirectX::XM_PI, -DirectX::XM_PIDIV4, DirectX::XM_PIDIV2, DirectX::XM_PIDIV4, -DirectX::XM_PI,
				-DirectX::XM_PIDIV2};
			if (m_elapsedSeconds >= m_nextDirectionChange) {
				m_targetYaw = std::remainder(
					currentYaw + directionChanges[m_directionChangeIndex % std::size(directionChanges)],
					DirectX::XM_2PI);
				++m_directionChangeIndex;
				m_nextDirectionChange += parameters.directionChangeSeconds;
			}
			if (m_elapsedSeconds >= m_nextAltitudeChange) {
				m_targetAltitude = m_targetAltitude == parameters.highAltitude ? parameters.lowAltitude
																			   : parameters.highAltitude;
				m_nextAltitudeChange += parameters.altitudeChangeSeconds;
			}

			const float yawError = std::remainder(m_targetYaw - currentYaw, DirectX::XM_2PI);
			input.turn = std::clamp(yawError * parameters.turnGain, -1.0f, 1.0f);
			const DirectX::XMFLOAT3 forward = aircraft.GetForward();
			const float currentPitch =
				std::atan2(forward.y, std::sqrt(forward.x * forward.x + forward.z * forward.z));
			const float altitudeError = m_targetAltitude - position.y;
			const float targetPitch =
				std::clamp(std::atan2(altitudeError, parameters.pitchLookAheadDistance), -0.45f, 0.45f);
			input.pitch = std::clamp((targetPitch - currentPitch) * parameters.pitchGain, -1.0f, 1.0f);
			input.throttle =
				std::clamp((targetSpeed - aircraft.GetSpeed()) * parameters.speedMatchGain, -1.0f, 1.0f);

			const float maneuverPhase = std::fmod(m_elapsedSeconds, parameters.highGManeuverIntervalSeconds);
			input.highGTurn =
				maneuverPhase < parameters.highGManeuverDurationSeconds && std::abs(input.turn) > 0.3f;
			if (input.highGTurn) {
				input.turn = std::copysign(std::max(std::abs(input.turn), 0.85f), input.turn);
			}
			return input;
		}

	  private:
		struct CombatStyle {
			float passDelay, turnScale, climb, exitAngle, highGSeconds;
		};
		// Stable aircraft identities avoid synchronized maneuvers and frame-by-frame noise.
		static constexpr CombatStyle kCombatStyles[] = {
			{0.0f, 1.00f,  0.00f, 0.00f, 2.5f},
			{0.5f, 0.92f,  0.10f, 0.10f, 2.0f},
			{1.1f, 0.98f, -0.04f, 0.14f, 2.8f},
			{0.2f, 0.88f,  0.16f, 0.07f, 2.2f},
			{0.8f, 0.94f, -0.09f, 0.13f, 3.0f},
			{1.4f, 0.90f,  0.06f, 0.18f, 1.8f},
		};
		static constexpr float kMinimumPassSeconds = 3.0f, kMaximumPassSeconds = 9.0f;
		static constexpr float kDefenseOuterRadius = 12000.0f, kDefenseInnerRadius = 8000.0f;
		AircraftCombatMode m_mode = AircraftCombatMode::Pursuit;
		DirectX::XMFLOAT3 m_steering{0, 0, -1};
		DirectX::XMFLOAT3 m_escapeDirection{0, 0, -1};
		float m_flankSide = 1;
		bool m_returning = false, m_collisionAvoidance = false, m_spacingBreak = false;
		float m_combatTime = 0, m_modeSeconds = 0;
		float m_aimTime = 0;
		float m_passMinimum = 3, m_passMaximum = 9, m_passLeg = 4500, m_turnBurstSeconds = 2.5f;
		float m_reentrySeconds = 1.5f, m_avoidanceRecheck = 0;
		DirectX::XMFLOAT3 m_passStart{}, m_reentryOffset{};
		uint32_t m_passSequence = 0;
		float m_elapsedSeconds = 0.0f;
		float m_nextDirectionChange = 0.0f;
		float m_nextAltitudeChange = 0.0f;
		float m_targetYaw = 0.0f;
		float m_targetAltitude = 0.0f;
		std::size_t m_directionChangeIndex = 0;
		bool m_initialized = false;
	};
} // namespace Game
