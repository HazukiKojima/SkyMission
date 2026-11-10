#include "AircraftAIController.h"
#include "Game/Combat/EnemyGunBehavior.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace Game {
	namespace {
		struct CombatStyle {
			float passDelay, turnScale, climb, exitAngle, highGSeconds;
		};
	} // namespace

	AircraftAIDecision AircraftAIController::ReadCombatInput(const AircraftFlightModel& aircraft,
		const AircraftAITarget& target, const DirectX::XMFLOAT3& aimDirection, float dt,
		const AircraftAIParameters& p, size_t slot, bool attackAllowed, bool weaponReady,
		std::optional<DirectX::XMFLOAT3> defenseCenter, bool opportunisticGun) {
		using namespace DirectX;
		AircraftAIDecision result;
		if (!std::isfinite(dt) || dt <= 0) return result;
		dt = std::min(dt, 0.25f);
		m_combatTime += dt;
		m_modeSeconds += dt;
		const auto position = aircraft.GetPosition(), forward = aircraft.GetForward();
		const auto velocity = aircraft.GetVelocity();
		const auto delta = XMLoadFloat3(&target.position) - XMLoadFloat3(&position);
		const float distance = XMVectorGetX(XMVector3Length(delta));
		const auto toTarget = distance > 0.001f ? delta / distance : XMLoadFloat3(&forward);
		const float targetAhead = XMVectorGetX(XMVector3Dot(toTarget, XMLoadFloat3(&forward)));
		const bool targetOnTail =
			targetAhead < m_tuning.rearTargetDotThreshold && distance <= p.reengageDistance;
		const auto relativeVelocity = XMLoadFloat3(&velocity) - XMLoadFloat3(&target.velocity);
		const float relativeSquared = XMVectorGetX(XMVector3LengthSq(relativeVelocity));
		const float closestTime = relativeSquared > 0.001f
			? std::clamp(XMVectorGetX(XMVector3Dot(delta, relativeVelocity)) / relativeSquared, 0.0f,
				  p.separationLookAheadSeconds)
			: 0;
		const float closest = XMVectorGetX(XMVector3Length(delta - relativeVelocity * closestTime));
		const bool needsSpacing =
			distance < p.disengageDistance || (closestTime > 0 && closest < p.disengageDistance);
		const float yaw = std::atan2(forward.x, -forward.z);
		XMVECTOR home = XMVectorZero();
		if (defenseCenter) {
			home = XMLoadFloat3(&*defenseCenter) - XMLoadFloat3(&position);
			const float homeDistance = std::hypot(XMVectorGetX(home), XMVectorGetZ(home));
			if (homeDistance > m_tuning.defenseOuterRadius)
				m_returning = true;
			else if (homeDistance <= m_tuning.defenseInnerRadius)
				m_returning = false;
		} else
			m_returning = false;
		if (position.y < p.lowAltitude)
			EnterMode(AircraftCombatMode::Recover);
		else if (m_returning) {
			if (m_mode != AircraftCombatMode::ReturnToCarrier) {
				EnterMode(AircraftCombatMode::ReturnToCarrier);
				ChooseTurn(yaw, home, slot);
			}
		} else if (m_mode == AircraftCombatMode::Recover || m_mode == AircraftCombatMode::ReturnToCarrier) {
			EnterMode(AircraftCombatMode::Reengage);
			ChooseTurn(yaw, delta, slot);
		} else if (m_mode == AircraftCombatMode::Disengage) {
			const float traveled = XMVectorGetX(XMVector3Dot(
				XMLoadFloat3(&position) - XMLoadFloat3(&m_passStart), XMLoadFloat3(&m_escapeDirection)));
			const float safeDistance = p.disengageDistance * (1.42f + float(slot % 6) * 0.06f);
			const bool spacingComplete =
				m_modeSeconds >= m_passMinimum && traveled >= m_passLeg && distance >= safeDistance;
			const bool escapeTimedOut = m_modeSeconds >= m_passMaximum;
			if (spacingComplete || escapeTimedOut) {
				EnterMode(AircraftCombatMode::Reengage);
				ChooseTurn(yaw, delta + XMLoadFloat3(&m_reentryOffset), slot);
			}
		} else if (m_mode == AircraftCombatMode::Reengage) {
			if (!needsSpacing && targetAhead > 0.85f && m_modeSeconds >= m_reentrySeconds * 0.5f)
				EnterMode(AircraftCombatMode::Pursuit);
			else if (m_modeSeconds >= m_tuning.maximumContinuousTurnSeconds)
				BeginDisengagement(aircraft, target, p, slot, defenseCenter);
		} else if (needsSpacing || targetOnTail ||
			m_continuousTurnSeconds >= m_tuning.maximumContinuousTurnSeconds) {
			// 機体ごとの上昇・旋回傾向を維持したまま直進離脱へ移る
			BeginDisengagement(aircraft, target, p, slot, defenseCenter);
		} else {
			m_mode = attackAllowed && distance <= p.attackDistance ? AircraftCombatMode::Attack
																   : AircraftCombatMode::Pursuit;
		}
		const bool imminentCollision = closestTime > 0 &&
			closest < std::min(1000.0f, p.disengageDistance * 0.34f) &&
			distance < std::min(3500.0f, p.disengageDistance * 1.2f);
		if (!imminentCollision) m_collisionAvoidance = false;
		m_avoidanceRecheck -= dt;
		if (imminentCollision && m_mode != AircraftCombatMode::Recover &&
			(!m_collisionAvoidance || m_avoidanceRecheck <= 0)) {
			// 衝突回避は直進離脱中や母艦への帰還旋回中にも優先する
			const auto right = XMVectorSet(-forward.z, 0, forward.x, 0);
			const float lateral = XMVectorGetX(XMVector3Dot(delta, right));
			const float side = std::abs(lateral) > 1 ? std::copysign(-1.0f, lateral) : m_flankSide;
			XMStoreFloat3(&m_escapeDirection,
				XMVector3Normalize(
					XMLoadFloat3(&forward) * 0.35f + right * side + XMVectorSet(0, 0.35f, 0, 0)));
			m_collisionAvoidance = true;
			m_avoidanceRecheck = 0.35f;
		}
		const float prediction =
			std::min(p.pursuitPredictionSeconds, distance / std::max(1.0f, aircraft.GetSpeed()));
		XMVECTOR steering = delta + XMLoadFloat3(&target.velocity) * prediction;
		// 離脱時間が上限に達しても、安全距離に戻るまでは敵へ旋回しない
		if (m_mode == AircraftCombatMode::Disengage ||
			(m_mode == AircraftCombatMode::Reengage && needsSpacing))
			steering = XMLoadFloat3(&m_escapeDirection);
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
		result.flight = BuildFlightInput(aircraft, target, p, slot, distance, imminentCollision);
		// 追尾で曲がり続けた場合も、次の直線区間へ切り替える
		m_continuousTurnSeconds = std::abs(result.flight.turn) > 0.3f || std::abs(result.flight.yaw) > 0.3f
			? m_continuousTurnSeconds + dt
			: 0;
		// 離脱中も短い連射を完了させるが、移動の離脱距離は変えない
		const float minimumFireDistance =
			opportunisticGun ? std::min(2200.0f, p.disengageDistance) : p.disengageDistance;
		const bool safeShot = attackAllowed && weaponReady && !m_collisionAvoidance &&
			distance >= minimumFireDistance && distance <= p.attackDistance &&
			(m_mode == AircraftCombatMode::Attack || m_mode == AircraftCombatMode::Disengage ||
				m_mode == AircraftCombatMode::Reengage || m_mode == AircraftCombatMode::ReturnToCarrier) &&
			XMVectorGetX(XMVector3Dot(XMLoadFloat3(&forward), XMLoadFloat3(&aimDirection))) >=
				std::cos(XMConvertToRadians(p.aimAngleDegrees)) &&
			m_combatTime >= p.openingGraceSeconds;
		const bool aligned = safeShot && !m_returning;
		m_aimTime = aligned ? std::min(p.aimHoldSeconds, m_aimTime + dt) : 0;
		result.locking = aligned;
		result.lockProgress = m_aimTime / p.aimHoldSeconds;
		result.fire =
			(aligned && m_aimTime >= p.aimHoldSeconds) || (m_returning && opportunisticGun && safeShot);
		return result;
	}

	void AircraftAIController::EnterMode(AircraftCombatMode mode) {
		m_mode = mode;
		m_modeSeconds = 0;
		m_continuousTurnSeconds = 0;
		m_collisionAvoidance = false;
		m_spacingBreak = false;
	}
	void AircraftAIController::ChooseTurn(float yaw, DirectX::FXMVECTOR direction, size_t slot) {
		using namespace DirectX;
		const float error =
			std::remainder(std::atan2(XMVectorGetX(direction), -XMVectorGetZ(direction)) - yaw, XM_2PI);
		constexpr float kTurnDirectionThresholdRadians = 0.05f;
		if (std::abs(error) > kTurnDirectionThresholdRadians) {
			m_flankSide = std::copysign(1.0f, error);
		} else {
			m_flankSide = slot % 2 == 0 ? 1.0f : -1.0f;
		}
	}
	void AircraftAIController::BeginDisengagement(const AircraftFlightModel& aircraft,
		const AircraftAITarget& target, const AircraftAIParameters& p, size_t slot,
		std::optional<DirectX::XMFLOAT3> defenseCenter) {
		using namespace DirectX;
		const size_t styleIndex = slot % m_tuning.stylePassDelay.size();
		const CombatStyle style{m_tuning.stylePassDelay[styleIndex], m_tuning.styleTurnScale[styleIndex],
			m_tuning.styleClimb[styleIndex], m_tuning.styleExitAngle[styleIndex],
			m_tuning.styleHighGSeconds[styleIndex]};
		const auto position = aircraft.GetPosition();
		const auto forward = aircraft.GetForward();
		const auto velocity = aircraft.GetVelocity();
		const float yaw = std::atan2(forward.x, -forward.z);
		const auto delta = XMLoadFloat3(&target.position) - XMLoadFloat3(&position);
		const auto relativeVelocity = XMLoadFloat3(&velocity) - XMLoadFloat3(&target.velocity);
		const float relativeSquared = XMVectorGetX(XMVector3LengthSq(relativeVelocity));
		const auto home =
			defenseCenter ? XMLoadFloat3(&*defenseCenter) - XMLoadFloat3(&position) : XMVectorZero();
		const float passTime = relativeSquared > 0.001f
			? std::max(0.0f, XMVectorGetX(XMVector3Dot(delta, relativeVelocity)) / relativeSquared)
			: 0;
		const float passDistance = XMVectorGetX(XMVector3Length(delta - relativeVelocity * passTime));

		EnterMode(AircraftCombatMode::Disengage);
		m_flankSide = slot % 2 == 0 ? 1.0f : -1.0f;
		if (defenseCenter && std::hypot(XMVectorGetX(home), XMVectorGetZ(home)) > 10000)
			ChooseTurn(yaw, home, slot);
		const auto maneuver = m_passSequence++;
		m_passMinimum =
			m_tuning.minimumPassSeconds + style.passDelay + EnemyVariation(slot, maneuver, 83) * 0.9f;
		m_passMaximum = std::min(m_tuning.maximumPassSeconds,
			7.0f + float(slot % 6) * 0.38f + EnemyVariation(slot, maneuver, 89) * 0.6f);
		m_passLeg = (m_tuning.passLegsMeters[slot % std::size(m_tuning.passLegsMeters)] +
						EnemyVariation(slot, maneuver, 97) * 900) *
			(p.tailDistance / 4500.0f);
		m_passStart = position;
		m_reentrySeconds = 1.2f + EnemyVariation(slot, maneuver, 103) * 1.8f;
		const auto targetForward = XMLoadFloat3(&target.forward);
		auto targetRight = XMVector3Cross(targetForward, XMVectorSet(0, 1, 0, 0));
		if (XMVectorGetX(XMVector3LengthSq(targetRight)) < 0.001f) targetRight = XMVectorSet(1, 0, 0, 0);
		XMStoreFloat3(&m_reentryOffset,
			XMVector3Normalize(targetRight) *
					m_tuning.reentryLanesMeters[slot % std::size(m_tuning.reentryLanesMeters)] *
					(0.8f + EnemyVariation(slot, maneuver, 107) * 0.4f) +
				targetForward * (float(slot % 3) * 600 - 500) + XMVectorSet(0, style.climb * 2000, 0, 0));
		m_turnBurstSeconds = style.highGSeconds * (0.8f + EnemyVariation(slot, maneuver, 101) * 0.4f);
		auto escape = XMVector3TransformNormal(XMLoadFloat3(&forward),
			XMMatrixRotationY(
				-(style.exitAngle + 0.12f + EnemyVariation(slot, maneuver, 109) * 0.35f) * m_flankSide));
		// 直進離脱ではプレイヤーの位置を貫かず、機体の脇を通過する
		if (passTime > 0 && passDistance < std::min(1800.0f, p.disengageDistance * 0.65f)) {
			const auto right = XMVectorSet(-forward.z, 0, forward.x, 0);
			const float lateral = XMVectorGetX(XMVector3Dot(delta, right));
			const float side = std::abs(lateral) > 100 ? std::copysign(-1.0f, lateral) : m_flankSide;
			const float angle =
				XMConvertToRadians(55 + float(slot % 6) * 5 + EnemyVariation(slot, maneuver, 113) * 5);
			escape = XMLoadFloat3(&forward) * std::cos(angle) + right * (side * std::sin(angle));
			m_spacingBreak = true;
		}
		const float preferredClimb = style.climb + (EnemyVariation(slot, maneuver, 127) - 0.5f) * 0.08f;
		const float climb =
			position.y < p.lowAltitude + 350 ? std::max(0.0f, preferredClimb) : preferredClimb;
		// 離脱を繰り返しても、現在の上昇角へ上昇指示を積み増さない
		escape = XMVectorSetY(escape, std::clamp(climb, -0.18f, 0.18f));
		XMStoreFloat3(&m_escapeDirection, XMVector3Normalize(escape));
	}

	AircraftInput AircraftAIController::BuildFlightInput(const AircraftFlightModel& aircraft,
		const AircraftAITarget& target, const AircraftAIParameters& p, size_t slot, float distance,
		bool imminentCollision) const {
		using namespace DirectX;
		AircraftInput input;
		const auto position = aircraft.GetPosition();
		const auto forward = aircraft.GetForward();
		const float yaw = std::atan2(forward.x, -forward.z);
		const size_t styleIndex = slot % m_tuning.stylePassDelay.size();
		const CombatStyle style{m_tuning.stylePassDelay[styleIndex], m_tuning.styleTurnScale[styleIndex],
			m_tuning.styleClimb[styleIndex], m_tuning.styleExitAngle[styleIndex],
			m_tuning.styleHighGSeconds[styleIndex]};
		const float yawError = std::remainder(std::atan2(m_steering.x, -m_steering.z) - yaw, XM_2PI);
		// 目標が旋回側を横切っても一周を強制せず、現在位置から短い方向へ向き直る
		input.autoLevelRoll = true;
		input.turn = std::abs(yawError) > 0.15f ? std::clamp(yawError * p.turnGain, -1.0f, 1.0f) : 0;
		if (!m_collisionAvoidance && m_mode != AircraftCombatMode::Recover &&
			m_mode != AircraftCombatMode::ReturnToCarrier)
			input.turn *= style.turnScale;
		input.yaw = std::abs(yawError) <= 0.6f ? std::clamp(-yawError * p.turnGain * 2, -1.0f, 1.0f) : 0;
		const float pitch = std::atan2(forward.y, std::hypot(forward.x, forward.z));
		float desiredPitch =
			std::clamp(std::atan2(m_steering.y, std::hypot(m_steering.x, m_steering.z)), -0.6f, 0.6f);
		if (m_mode != AircraftCombatMode::Recover) {
			// 自機を追う高度は確保しつつ、離脱・回避で戦闘高度から上がり続けるのを抑える
			const float ceiling = std::max(p.highAltitude, target.position.y + 300.0f);
			const float altitudePitch = std::clamp(
				std::atan2(ceiling - position.y, std::max(1.0f, p.pitchLookAheadDistance)), -0.25f, 0.6f);
			desiredPitch = std::min(desiredPitch, altitudePitch);
		}
		input.pitch = std::clamp((desiredPitch - pitch) * p.pitchGain, -1.0f, 1.0f);
		// 安全距離に戻るまでの再接近待ちでは、巡航速度へ減速しない
		const bool separating = m_mode == AircraftCombatMode::Disengage ||
			(m_mode == AircraftCombatMode::Reengage && distance < p.disengageDistance);
		float targetSpeed = p.cruiseSpeed;
		if (separating || (m_mode == AircraftCombatMode::Pursuit && distance > p.reengageDistance) ||
			(m_mode == AircraftCombatMode::Attack && distance > p.reengageDistance))
			targetSpeed = p.escapeSpeed;
		if (separating)
			targetSpeed = std::max(targetSpeed,
				XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.velocity))) +
					p.disengageDistance / m_tuning.minimumPassSeconds);
		targetSpeed = std::min(targetSpeed, aircraft.GetMaximumSpeed());
		input.throttle = std::clamp((targetSpeed - aircraft.GetSpeed()) * p.speedMatchGain, -1.0f, 1.0f);
		// 再接近時だけ短くハイG旋回し、近距離で旋回し続けるのを防ぐ
		input.highGTurn = (imminentCollision && m_mode != AircraftCombatMode::Recover) ||
			(m_mode == AircraftCombatMode::Disengage && m_spacingBreak && m_modeSeconds < 1.8f) ||
			(m_mode == AircraftCombatMode::Reengage &&
				m_modeSeconds < (m_passSequence > 0 ? m_turnBurstSeconds : style.highGSeconds) &&
				distance >= p.disengageDistance && std::abs(yawError) > 0.7f);
		return input;
	}

	AircraftInput AircraftAIController::ReadInput(const AircraftFlightModel& aircraft, float targetSpeed,
		float deltaSeconds, const AircraftAIParameters& parameters) {
		AircraftInput input;
		if (deltaSeconds <= 0.0f) return input;
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

		if (m_elapsedSeconds >= m_nextDirectionChange) {
			m_targetYaw = std::remainder(currentYaw +
					m_tuning.directionChangesRadians[m_directionChangeIndex %
						std::size(m_tuning.directionChangesRadians)],
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
} // namespace Game
