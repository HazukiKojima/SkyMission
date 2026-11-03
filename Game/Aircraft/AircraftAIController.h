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
	enum class AircraftCombatMode { Pursuit, Attack, Disengage, Evade, Recover };
	struct AircraftAITarget {
		DirectX::XMFLOAT3 position{}, velocity{}, forward{0, 0, -1};
	};
	struct AircraftAIDecision {
		AircraftInput flight;
		bool fire = false;
		bool aiming = false;
		float aimProgress = 0;
	};

	class AircraftAIController {
	  public:
		AircraftCombatMode GetMode() const {
			return m_mode;
		}
		// 実際に発射できたときだけ、次の判断で短い横離脱に入る
		void NotifyWeaponFired() {
			m_breakPending = true;
			m_aimTime = 0;
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
			m_evasionCooldown -= dt;
			m_separationCooldown -= dt;
			const auto position = aircraft.GetPosition();
			const auto forward = aircraft.GetForward();
			const auto velocity = aircraft.GetVelocity();
			const auto playerVelocity = XMLoadFloat3(&target.velocity);
			const float playerSpeed = XMVectorGetX(XMVector3Length(playerVelocity));
			const auto playerForwardVector = XMLoadFloat3(&target.forward);
			const auto playerForward = XMVectorGetX(XMVector3LengthSq(playerForwardVector)) > 0.0001f
				? XMVector3Normalize(playerForwardVector)
				: XMVectorSet(0, 0, -1, 0);
			auto right = XMVector3Cross(playerForward, XMVectorSet(0, 1, 0, 0));
			right = XMVectorGetX(XMVector3LengthSq(right)) > 0.0001f ? XMVector3Normalize(right)
																	 : XMVectorSet(1, 0, 0, 0);
			const auto delta = XMLoadFloat3(&target.position) - XMLoadFloat3(&position);
			const float distance = XMVectorGetX(XMVector3Length(delta));
			const auto toTarget = distance > 0.001f ? delta / distance : playerForward;
			const float rearAlignment = XMVectorGetX(XMVector3Dot(toTarget, playerForward));
			const bool behind = rearAlignment >= 0.5f;
			const float ahead = XMVectorGetX(XMVector3Dot(toTarget, XMLoadFloat3(&forward)));
			const auto relativeVelocity = XMLoadFloat3(&velocity) - playerVelocity;
			const float closingSpeed = std::max(0.0f, XMVectorGetX(XMVector3Dot(relativeVelocity, toTarget)));
			const float relativeSpeedSquared = XMVectorGetX(XMVector3LengthSq(relativeVelocity));
			const float closestTime = relativeSpeedSquared > 0.001f
				? std::clamp(XMVectorGetX(XMVector3Dot(delta, relativeVelocity)) / relativeSpeedSquared, 0.0f,
					  p.separationLookAheadSeconds)
				: 0;
			const float closestDistance =
				XMVectorGetX(XMVector3Length(delta - relativeVelocity * closestTime));
			const float maximumAvoidDistance = p.tailDistance * 1.5f;
			const float minimumSeparation = std::min(maximumAvoidDistance, p.disengageDistance);
			const float avoidDistance = std::min(
				maximumAvoidDistance, minimumSeparation + closingSpeed * p.separationLookAheadSeconds);
			const bool needsSpacing = (distance < minimumSeparation ||
				(distance < avoidDistance && closestDistance < minimumSeparation));
			const bool avoidApproach = needsSpacing && m_separationCooldown <= 0;
			const bool threatened = distance < p.threatDistance && ahead < -0.35f && rearAlignment < -0.8f;
			const auto tailDelta = delta - playerForward * p.tailDistance;
			const float tailError = XMVectorGetX(XMVector3Length(tailDelta));
			if (!m_combatInitialized) {
				m_reaction = p.reactionSeconds * (1.0f + float(slot) * 0.4f);
				m_steering = forward;
				m_flankSide = slot % 2 == 0 ? 1.0f : -1.0f;
				m_bestTailError = tailError;
				m_combatInitialized = true;
			}
			// 有利な位置を保っているときは反転せず、進展がない場合だけ回り込み側を変える
			if ((behind && distance <= p.attackDistance) || tailError < m_bestTailError - 50) {
				m_bestTailError = tailError;
				m_noProgressTime = 0;
			} else if (m_mode == AircraftCombatMode::Pursuit) {
				m_noProgressTime += dt;
				if (m_noProgressTime >= p.repositionSeconds) {
					m_flankSide = -m_flankSide;
					m_bestTailError = tailError;
					m_noProgressTime = 0;
					m_reaction = 0;
				}
			}
			const bool breakRequested = m_breakPending;
			m_breakPending = false;
			if (position.y < p.lowAltitude || avoidApproach || breakRequested ||
				((m_mode == AircraftCombatMode::Disengage || m_mode == AircraftCombatMode::Evade ||
					 m_mode == AircraftCombatMode::Recover) &&
					m_modeRemaining <= 0) ||
				m_reaction <= 0) {
				m_reaction = p.reactionSeconds;
				const auto previous = m_mode;
				if (position.y < p.lowAltitude) {
					m_mode = AircraftCombatMode::Recover;
					m_modeRemaining = p.maneuverSeconds;
				} else if (m_mode == AircraftCombatMode::Disengage && m_modeRemaining > 0) {
					// 接近が続いても保持時間を延長しない
				} else if (previous == AircraftCombatMode::Disengage) {
					m_mode = AircraftCombatMode::Pursuit;
					m_separationCooldown = p.maneuverSeconds;
				} else if (breakRequested || avoidApproach) {
					m_mode = AircraftCombatMode::Disengage;
					m_modeRemaining = std::min(p.maneuverSeconds, p.maximumDisengageSeconds);
					m_collisionBreak = avoidApproach && !breakRequested;
					const float awaySide = XMVectorGetX(XMVector3Dot(delta, right));
					const float side = m_collisionBreak && std::abs(awaySide) > 1
						? (awaySide > 0 ? -1.0f : 1.0f)
						: m_flankSide;
					XMStoreFloat3(&m_escapeDirection,
						XMVector3Normalize(
							XMLoadFloat3(&forward) * 0.6f + right * side + XMVectorSet(0, 0.1f, 0, 0)));
				} else if ((previous == AircraftCombatMode::Evade ||
							   previous == AircraftCombatMode::Recover) &&
					m_modeRemaining > 0) {
				} else if (threatened && m_evasionCooldown <= 0) {
					m_mode = AircraftCombatMode::Evade;
					m_modeRemaining = std::min(p.maneuverSeconds, p.maximumDisengageSeconds);
					m_evasionCooldown = p.maneuverSeconds * 3;
				} else {
					m_mode = attackAllowed && !needsSpacing && distance >= minimumSeparation &&
							distance <= p.attackDistance && ahead > 0.3f
						? AircraftCombatMode::Attack
						: AircraftCombatMode::Pursuit;
				}
				const float prediction = std::min(
					p.pursuitPredictionSeconds, distance / std::max(1.0f, aircraft.GetSpeed() + playerSpeed));
				XMVECTOR steering = tailDelta + playerVelocity * prediction;
				if (!behind)
					steering += right * (m_flankSide * p.flankDistance);
				if (m_mode == AircraftCombatMode::Recover)
					steering = XMVectorSet(forward.x, 0.7f, forward.z, 0);
				else if (m_mode == AircraftCombatMode::Disengage)
					steering = XMLoadFloat3(&m_escapeDirection);
				else if (m_mode == AircraftCombatMode::Evade)
					steering =
						XMLoadFloat3(&forward) + right * (m_flankSide * 2) + XMVectorSet(0, 0.2f, 0, 0);
				else if (m_mode == AircraftCombatMode::Attack)
					steering = XMLoadFloat3(&aimDirection);
				// 安全な距離帯へ戻るまでは、離脱の保持時間が終わっても接近に転じない
				if (m_mode != AircraftCombatMode::Recover && needsSpacing) {
					steering = behind ? playerForward + right * (m_flankSide * 0.25f)
									  : -toTarget + right * (m_flankSide * 0.3f);
				} else if (m_mode == AircraftCombatMode::Pursuit && behind && distance < p.tailDistance) {
					steering = playerForward;
				}
				if (XMVectorGetX(XMVector3LengthSq(steering)) < 0.001f)
					steering = playerForward;
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
			const float speedLimit = distance > p.reengageDistance ? std::max(p.escapeSpeed, playerSpeed)
																   : std::max(p.cruiseSpeed, playerSpeed);
			float targetSpeed = std::clamp(playerSpeed + (distance - p.tailDistance) * 0.35f,
				std::min(p.attackSpeed, speedLimit), speedLimit);
			// 照準へ直進する場合も接近速度を見て早めに制動する
			const float radialSpeed = XMVectorGetX(XMVector3Dot(playerVelocity, toTarget));
			if (m_mode == AircraftCombatMode::Attack)
				targetSpeed = std::min(
					targetSpeed, std::max(p.attackSpeed, radialSpeed + (distance - p.tailDistance) * 0.35f));
			if (avoidApproach || m_mode == AircraftCombatMode::Disengage ||
				(std::abs(yawError) > 0.9f && distance < p.reengageDistance))
				targetSpeed = std::min(targetSpeed, p.attackSpeed);
			if (needsSpacing && !behind)
				targetSpeed = std::max(p.escapeSpeed, playerSpeed + 350);
			targetSpeed = std::min(targetSpeed, aircraft.GetMaximumSpeed());
			result.flight.throttle =
				std::clamp((targetSpeed - aircraft.GetSpeed()) * p.speedMatchGain, -1.0f, 1.0f);
			result.flight.highGTurn = std::abs(result.flight.turn) > 0.3f &&
				(m_mode == AircraftCombatMode::Evade || needsSpacing ||
					(m_mode == AircraftCombatMode::Disengage && m_collisionBreak) ||
					(m_mode == AircraftCombatMode::Pursuit && distance < p.reengageDistance));
			const bool aligned = attackAllowed && weaponReady && !needsSpacing &&
				distance >= minimumSeparation && m_mode == AircraftCombatMode::Attack &&
				distance <= p.attackDistance &&
				XMVectorGetX(XMVector3Dot(XMLoadFloat3(&forward), XMLoadFloat3(&aimDirection))) >=
					std::cos(XMConvertToRadians(p.aimAngleDegrees)) &&
				m_combatTime >= p.openingGraceSeconds;
			m_aimTime = aligned ? std::min(p.aimHoldSeconds, m_aimTime + dt) : 0;
			result.aiming = aligned;
			result.aimProgress = m_aimTime / p.aimHoldSeconds;
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
		float m_flankSide = 1, m_bestTailError = 0, m_noProgressTime = 0;
		float m_evasionCooldown = 0, m_separationCooldown = 0;
		bool m_breakPending = false;
		bool m_collisionBreak = false;
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
