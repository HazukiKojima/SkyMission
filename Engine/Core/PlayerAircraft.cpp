#include "PlayerAircraft.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Engine {
	void PlayerAircraft::Update(const AircraftInput& input, float deltaSeconds) {
		if (deltaSeconds <= 0.0f) {
			return;
		}

		m_speed = std::clamp(m_speed + input.throttle * kAcceleration * deltaSeconds, kMinimumSpeed, kMaximumSpeed);
		m_pitch = std::clamp(m_pitch + input.pitch * kPitchRate * deltaSeconds, -kMaximumPitch, kMaximumPitch);

		const float targetRoll = -input.turn * kMaximumBank;
		const float rollBlend = 1.0f - std::exp(-5.0f * deltaSeconds);
		m_roll += (targetRoll - m_roll) * rollBlend;
		m_yaw += std::sin(m_roll) * kTurnRate * deltaSeconds;

		const XMFLOAT3 forwardValue = GetForward();
		const XMVECTOR forward = XMLoadFloat3(&forwardValue);
		XMVECTOR position = XMLoadFloat3(&m_position);
		position += forward * (m_speed * deltaSeconds);
		XMStoreFloat3(&m_position, position);
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
