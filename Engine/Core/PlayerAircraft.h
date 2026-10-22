#pragma once
#include "AircraftController.h"
#include <DirectXMath.h>

namespace Engine {
	class PlayerAircraft {
	public:
		void Update(const AircraftInput& input, float deltaSeconds);
		DirectX::XMFLOAT3 GetPosition() const { return m_position; }
		DirectX::XMFLOAT3 GetForward() const;
		DirectX::XMFLOAT4X4 GetWorldTransform() const;

	private:
		DirectX::XMFLOAT3 m_position = { 0.0f, 0.0f, 0.0f };
		float m_pitch = 0.0f;
		float m_yaw = 0.0f;
		float m_roll = 0.0f;
		float m_speed = 60.0f;
		static constexpr float kMinimumSpeed = 20.0f;
		static constexpr float kMaximumSpeed = 150.0f;
		static constexpr float kAcceleration = 35.0f;
		static constexpr float kMaximumPitch = 0.785398163f;
		static constexpr float kMaximumBank = 0.959931089f;
		static constexpr float kPitchRate = 0.785398163f;
		static constexpr float kTurnRate = 0.959931089f;
	};
}
