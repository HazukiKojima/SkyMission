#include "PlayerAircraft.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Engine {
	void PlayerAircraft::Update(const AircraftInput& input, float deltaSeconds) {
		if (deltaSeconds <= 0.0f) {
			return;
		}

		float remainingSeconds = (std::min)(deltaSeconds, 0.25f);
		const float targetPitchControl = std::clamp(input.pitch, -1.0f, 1.0f);
		const float targetTurnControl = std::clamp(input.turn, -1.0f, 1.0f);
		const float targetYawControl = std::clamp(input.yaw, -1.0f, 1.0f);
		const float targetThrottleControl = std::clamp(input.throttle, -1.0f, 1.0f);

		while (remainingSeconds > 0.0f) {
			const float step = (std::min)(remainingSeconds, 1.0f / 60.0f);
			remainingSeconds -= step;
			const float controlBlend = 1.0f - std::exp(-m_flightParameters.controlResponse * step);
			m_pitchControl += (targetPitchControl - m_pitchControl) * controlBlend;
			m_turnControl += (targetTurnControl - m_turnControl) * controlBlend;
			m_yawControl += (targetYawControl - m_yawControl) * controlBlend;
			m_throttleControl += (targetThrottleControl - m_throttleControl) * controlBlend;

			const XMFLOAT3 currentForward = GetForward();
			const XMVECTOR forward = XMLoadFloat3(&currentForward);
			XMVECTOR velocity = XMLoadFloat3(&m_velocity);
			const float speed = XMVectorGetX(XMVector3Length(velocity));
			const XMVECTOR travelDirection = speed > 0.001f
				? XMVector3Normalize(velocity)
				: forward;
			const float alignment = std::clamp(XMVectorGetX(XMVector3Dot(forward, travelDirection)), -1.0f, 1.0f);
			const float angleOfAttack = std::acos(alignment);
			const float angleControl = std::clamp(
				1.0f - (std::max)(0.0f, angleOfAttack - m_flightParameters.stallAngleRadians) / (m_flightParameters.criticalAngleRadians - m_flightParameters.stallAngleRadians),
				0.2f,
				1.0f);
			const float speedControl = std::clamp(speed / m_flightParameters.stallSpeed, 0.25f, 1.0f);
			const float controlEffectiveness = angleControl * speedControl;
			const float turnSpeedFactor = std::clamp(
				m_flightParameters.liftReferenceSpeed / (std::max)(speed, m_flightParameters.minimumSpeed),
				m_flightParameters.minimumTurnFactor,
				m_flightParameters.maximumTurnFactor);
			const float turnIntent = std::clamp(
				(std::max)(std::abs(m_turnControl), std::abs(m_yawControl)),
				0.0f,
				1.0f);
			const float highGIntensity = input.highGTurn && turnIntent > 0.05f ? 1.0f : 0.0f;
			const float highGTurnFactor = 1.0f + highGIntensity * (m_flightParameters.highGTurnMultiplier - 1.0f);

			m_pitch = std::remainder(
				m_pitch + m_pitchControl * m_flightParameters.pitchRateRadians * controlEffectiveness * step,
				XM_2PI);

			const float targetRoll = std::clamp(
				-m_turnControl * m_flightParameters.maximumBankRadians * turnSpeedFactor,
				-m_flightParameters.maximumBankRadians,
				m_flightParameters.maximumBankRadians);
			const float rollResponse = std::abs(m_turnControl) > 0.01f
				? m_flightParameters.rollInputResponse
				: m_flightParameters.rollLevelRate;
			const float rollBlend = 1.0f - std::exp(-rollResponse * step);
			m_roll += (targetRoll - m_roll) * rollBlend;
			m_yaw = std::remainder(
				m_yaw + (std::sin(m_roll) * m_flightParameters.turnRateRadians + m_yawControl * m_flightParameters.yawRateRadians) *
				angleControl * turnSpeedFactor * highGTurnFactor * step,
				XM_2PI);

			const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, m_roll);
			const XMVECTOR aircraftForward = XMVector3Normalize(XMVector3TransformNormal(
				XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), rotation));
			const XMVECTOR up = XMVector3Normalize(XMVector3TransformNormal(
				XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation));
			const float angleLift = std::clamp(
				1.0f - (std::max)(0.0f, angleOfAttack - m_flightParameters.stallAngleRadians) / (m_flightParameters.criticalAngleRadians - m_flightParameters.stallAngleRadians),
				0.0f,
				1.0f);
			const float speedRatio = speed / m_flightParameters.liftReferenceSpeed;
			const float verticalLiftAssist = 1.0f / (std::max)(XMVectorGetY(up), 0.35f);
			const float liftAcceleration = (std::min)(
				m_flightParameters.gravity * speedRatio * speedRatio * angleLift * verticalLiftAssist,
				m_flightParameters.gravity * m_flightParameters.maximumLiftFactor);
			const float bankLoad = std::sin(m_roll);
			const XMVECTOR flightPathAcceleration =
				(aircraftForward - travelDirection) *
				(speed * m_flightParameters.flightPathResponse * angleControl * turnSpeedFactor * highGTurnFactor);
			const XMVECTOR acceleration = up * liftAcceleration +
				XMVectorSet(0.0f, -m_flightParameters.gravity, 0.0f, 0.0f) +
				aircraftForward * (m_throttleControl * m_flightParameters.acceleration) +
				flightPathAcceleration;
			velocity += acceleration * step;

			const float drag = m_flightParameters.drag + m_flightParameters.turnDrag * bankLoad * bankLoad + m_flightParameters.highGDrag * highGIntensity * turnIntent;
			velocity *= std::exp(-drag * step);

			float updatedSpeed = XMVectorGetX(XMVector3Length(velocity));
			if (updatedSpeed > m_flightParameters.maximumSpeed) {
				velocity = XMVector3Normalize(velocity) * m_flightParameters.maximumSpeed;
			}
			else if (updatedSpeed < m_flightParameters.minimumSpeed) {
				velocity = (updatedSpeed > 0.001f ? XMVector3Normalize(velocity) : forward) * m_flightParameters.minimumSpeed;
			}

			XMVECTOR position = XMLoadFloat3(&m_position);
			position += velocity * step;
			XMStoreFloat3(&m_position, position);
			XMStoreFloat3(&m_velocity, velocity);
		}
	}

	XMFLOAT3 PlayerAircraft::GetForward() const {
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, m_roll);
		XMFLOAT3 forward;
		XMStoreFloat3(&forward, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), rotation)));
		return forward;
	}

	XMFLOAT4X4 PlayerAircraft::GetWorldTransform() const {
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world,
			XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, m_roll) *
			XMMatrixTranslation(m_position.x, m_position.y, m_position.z));
		return world;
	}
}
