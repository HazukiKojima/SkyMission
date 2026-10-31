#pragma once

#include "AircraftFlightModel.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

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
		float disengageDistance = 200.0f;
		float threatDistance = 700.0f;
		float attackDistance = 900.0f;
		float aimAngleDegrees = 3.0f;
		float aimHoldSeconds = 0.4f;
		float openingGraceSeconds = 3.0f;
		float aimErrorDegrees = 0.65f;
		float cruiseSpeed = 340.0f;
		float attackSpeed = 260.0f;
		float escapeSpeed = 520.0f;
		float reengageDistance = 3200.0f;
		float attackRunSeconds = 7.0f;
		float separationLookAheadSeconds = 3.5f;
	};
	enum class AircraftCombatMode { Pursuit, Attack, Disengage, Evade, Recover };
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
		AircraftAIDecision ReadCombatInput(const AircraftFlightModel& aircraft,
			const AircraftAITarget& target, const DirectX::XMFLOAT3& aimDirection, float dt,
			const AircraftAIParameters& p, size_t slot, bool attackAllowed, bool weaponReady = true) {
			using namespace DirectX;
			AircraftAIDecision result;
			if (!std::isfinite(dt) || dt <= 0)
				return result;
			dt = std::min(dt, 0.25f);
			m_combatTime += dt;
			m_reaction -= dt;
			m_modeRemaining -= dt;
			const auto position = aircraft.GetPosition();
			const auto forward = aircraft.GetForward();
			const auto delta = XMLoadFloat3(&target.position) - XMLoadFloat3(&position);
			const float distance = XMVectorGetX(XMVector3Length(delta));
			const auto toTarget = distance > 0.001f ? delta / distance : XMLoadFloat3(&forward);
			const float ahead = XMVectorGetX(XMVector3Dot(toTarget, XMLoadFloat3(&forward)));
			if (m_mode == AircraftCombatMode::Attack)
				m_attackTime += dt;
			else
				m_attackTime = 0;
			const auto velocity = aircraft.GetVelocity();
			const float closingSpeed = std::max(0.0f,
				XMVectorGetX(
					XMVector3Dot(XMLoadFloat3(&velocity) - XMLoadFloat3(&target.velocity), toTarget)));
			const bool avoidApproach =
				distance < p.disengageDistance + closingSpeed * p.separationLookAheadSeconds;
			const bool threatened = distance < p.threatDistance && ahead < -0.35f &&
				XMVectorGetX(XMVector3Dot(-toTarget, XMLoadFloat3(&target.forward))) > 0.8f;
			if (!m_combatInitialized) {
				m_reaction = p.reactionSeconds * (1.0f + float(slot) * 0.4f);
				m_steering = forward;
				m_combatInitialized = true;
			}
			// 低空回復は反応待ちより優先し、その他の判断には遅延と保持時間を設ける。
			if (position.y < p.lowAltitude || avoidApproach || m_reaction <= 0) {
				m_reaction = p.reactionSeconds;
				if (position.y < p.lowAltitude) {
					m_mode = AircraftCombatMode::Recover;
					m_modeRemaining = p.maneuverSeconds;
				} else if (m_mode == AircraftCombatMode::Disengage &&
					(distance < p.reengageDistance || m_modeRemaining > 0)) {
					// 攻撃後は十分に距離を開けてから再進入する。
				} else if (avoidApproach || m_modeRemaining <= 0) {
					const auto previous = m_mode;
					m_mode = avoidApproach || m_attackTime >= p.attackRunSeconds
						? AircraftCombatMode::Disengage
						: threatened								   ? AircraftCombatMode::Evade
						: attackAllowed && distance < p.attackDistance ? AircraftCombatMode::Attack
																	   : AircraftCombatMode::Pursuit;
					if (m_mode == AircraftCombatMode::Evade)
						m_modeRemaining = p.maneuverSeconds;
					if (m_mode == AircraftCombatMode::Disengage && previous != m_mode) {
						m_modeRemaining = p.maneuverSeconds;
						const float side = slot % 2 == 0 ? 1.0f : -1.0f;
						XMStoreFloat3(&m_escapeDirection,
							XMVector3Normalize(-toTarget + XMVectorSet(side * 0.35f, 0.2f, 0, 0)));
					}
				}
				XMVECTOR steering = XMLoadFloat3(&aimDirection);
				const float side = slot % 2 == 0 ? 1.0f : -1.0f;
				if (m_mode == AircraftCombatMode::Recover)
					steering = XMVectorSet(forward.x, 0.7f, forward.z, 0);
				else if (m_mode == AircraftCombatMode::Disengage)
					steering = XMLoadFloat3(&m_escapeDirection);
				else if (m_mode == AircraftCombatMode::Evade)
					steering = XMVectorSet(
						forward.x + side * forward.z * 2, 0.35f, forward.z - side * forward.x * 2, 0);
				else if (!attackAllowed)
					steering = delta + XMVectorSet(side * 450, 120 + float(slot) * 40, 0, 0);
				const float error =
					XMConvertToRadians(p.aimErrorDegrees) * std::sin(m_combatTime * 1.7f + float(slot) * 2);
				XMStoreFloat3(&m_steering,
					XMVector3Normalize(XMVector3TransformNormal(steering, XMMatrixRotationY(error))));
			}
			result.flight.autoLevelRoll = true;
			const float yaw = std::atan2(forward.x, -forward.z);
			const float yawError = std::remainder(std::atan2(m_steering.x, -m_steering.z) - yaw, XM_2PI);
			result.flight.turn =
				std::abs(yawError) > 0.35f ? std::clamp(yawError * p.turnGain, -1.0f, 1.0f) : 0;
			result.flight.yaw =
				std::abs(yawError) <= 0.6f ? std::clamp(-yawError * p.turnGain * 2, -1.0f, 1.0f) : 0;
			const float pitch = std::atan2(forward.y, std::hypot(forward.x, forward.z));
			const float desiredPitch =
				std::clamp(std::atan2(m_steering.y, std::hypot(m_steering.x, m_steering.z)), -0.6f, 0.6f);
			result.flight.pitch = std::clamp((desiredPitch - pitch) * p.pitchGain, -1.0f, 1.0f);
			const float targetSpeed = std::min(aircraft.GetMaximumSpeed(),
				m_mode == AircraftCombatMode::Disengage
					? std::max(p.escapeSpeed,
						  XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.velocity))) + 250.0f)
					: m_mode == AircraftCombatMode::Attack ? p.attackSpeed
														   : p.cruiseSpeed);
			result.flight.throttle =
				std::clamp((targetSpeed - aircraft.GetSpeed()) * p.speedMatchGain, -1.0f, 1.0f);
			result.flight.highGTurn =
				m_mode == AircraftCombatMode::Evade && std::abs(result.flight.turn) > 0.3f;
			const bool aligned = attackAllowed && weaponReady && m_mode == AircraftCombatMode::Attack &&
				distance <= p.attackDistance &&
				XMVectorGetX(XMVector3Dot(XMLoadFloat3(&forward), XMLoadFloat3(&aimDirection))) >=
					std::cos(XMConvertToRadians(p.aimAngleDegrees)) &&
				m_combatTime >= p.openingGraceSeconds;
			m_aimTime = aligned ? std::min(p.aimHoldSeconds, m_aimTime + dt) : 0;
			result.locking = aligned;
			result.lockProgress = m_aimTime / p.aimHoldSeconds;
			result.fire = aligned && m_aimTime >= p.aimHoldSeconds;
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
		AircraftCombatMode m_mode = AircraftCombatMode::Pursuit;
		DirectX::XMFLOAT3 m_steering{0, 0, -1};
		DirectX::XMFLOAT3 m_escapeDirection{0, 0, -1};
		float m_attackTime = 0;
		float m_combatTime = 0, m_reaction = 0, m_modeRemaining = 0;
		float m_aimTime = 0;
		bool m_combatInitialized = false;
		float m_elapsedSeconds = 0.0f;
		float m_nextDirectionChange = 0.0f;
		float m_nextAltitudeChange = 0.0f;
		float m_targetYaw = 0.0f;
		float m_targetAltitude = 0.0f;
		std::size_t m_directionChangeIndex = 0;
		bool m_initialized = false;
	};
} // namespace Game
