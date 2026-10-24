#pragma once
#include "AircraftController.h"
#include "AircraftFlightParameters.h"
#include <DirectXMath.h>

namespace Engine {
	class PlayerAircraft {
	public:
		void Update(const AircraftInput& input, float deltaSeconds);
		void SetFlightParameters(const AircraftFlightParameters& parameters) {
			m_flightParameters = parameters;
			m_velocity = { 0.0f, 0.0f, -parameters.initialSpeed };
		}
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
		AircraftFlightParameters m_flightParameters;
	};
}