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
		DirectX::XMFLOAT3 m_velocity = { 0.0f, 0.0f, -60.0f };
		float m_pitch = 0.0f;
		float m_yaw = 0.0f;
		float m_roll = 0.0f;
		float m_pitchControl = 0.0f;
		float m_turnControl = 0.0f;
		float m_yawControl = 0.0f;
		float m_throttleControl = 0.0f;
		static constexpr float kMinimumSpeed = 8.0f;
		static constexpr float kMaximumSpeed = 150.0f;
		static constexpr float kAcceleration = 60.0f;
		static constexpr float kDrag = 0.035f;
		static constexpr float kMaximumBank = 1.308996939f;
		static constexpr float kPitchRate = 0.872664626f;
		static constexpr float kTurnRate = 0.95f;
		static constexpr float kYawRate = 0.45f;
		static constexpr float kRollLevelRate = 0.5f;
		static constexpr float kControlResponse = 80.0f;
		static constexpr float kMinimumTurnFactor = 0.55f;
		static constexpr float kMaximumTurnFactor = 1.4f;
		static constexpr float kHighGTurnMultiplier = 1.8f;
		static constexpr float kHighGDrag = 0.35f;
		static constexpr float kGravity = 9.81f;
		static constexpr float kStallSpeed = 28.0f;
		static constexpr float kLiftReferenceSpeed = 60.0f;
		static constexpr float kMaximumLiftFactor = 4.0f;
		static constexpr float kStallAngle = 0.610865238f;
		static constexpr float kCriticalAngle = 1.308996939f;
		static constexpr float kTurnDrag = 0.32f;
		static constexpr float kFlightPathResponse = 1.8f;
	};
}
