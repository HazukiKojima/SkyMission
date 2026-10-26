#pragma once

#include "PlayerAircraft.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iterator>

namespace Engine {
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
	};

	class AircraftAIController {
	public:
		AircraftInput ReadInput(
			const AircraftFlightModel& aircraft,
			float targetSpeed,
			float deltaSeconds,
			const AircraftAIParameters& parameters) {
			AircraftInput input;
			if (deltaSeconds <= 0.0f) {
				return input;
			}
			m_elapsedSeconds += (std::min)(deltaSeconds, 0.25f);

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

			static constexpr float directionChanges[] = {
				DirectX::XM_PIDIV2,
				-DirectX::XM_PIDIV2,
				DirectX::XM_PI,
				-DirectX::XM_PIDIV4,
				DirectX::XM_PIDIV2,
				DirectX::XM_PIDIV4,
				-DirectX::XM_PI,
				-DirectX::XM_PIDIV2
			};
			if (m_elapsedSeconds >= m_nextDirectionChange) {
				m_targetYaw = std::remainder(
					currentYaw + directionChanges[m_directionChangeIndex % std::size(directionChanges)],
					DirectX::XM_2PI);
				++m_directionChangeIndex;
				m_nextDirectionChange += parameters.directionChangeSeconds;
			}
			if (m_elapsedSeconds >= m_nextAltitudeChange) {
				m_targetAltitude = m_targetAltitude == parameters.highAltitude
					? parameters.lowAltitude
					: parameters.highAltitude;
				m_nextAltitudeChange += parameters.altitudeChangeSeconds;
			}

			const float yawError = std::remainder(m_targetYaw - currentYaw, DirectX::XM_2PI);
			input.turn = (std::clamp)(yawError * parameters.turnGain, -1.0f, 1.0f);
			const DirectX::XMFLOAT3 forward = aircraft.GetForward();
			const float currentPitch = std::atan2(
				forward.y,
				std::sqrt(forward.x * forward.x + forward.z * forward.z));
			const float altitudeError = m_targetAltitude - position.y;
			const float targetPitch = (std::clamp)(
				std::atan2(altitudeError, parameters.pitchLookAheadDistance),
				-0.45f,
				0.45f);
			input.pitch = (std::clamp)((targetPitch - currentPitch) * parameters.pitchGain, -1.0f, 1.0f);
			input.throttle = (std::clamp)(
				(targetSpeed - aircraft.GetSpeed()) * parameters.speedMatchGain,
				-1.0f,
				1.0f);

			const float maneuverPhase = std::fmod(m_elapsedSeconds, parameters.highGManeuverIntervalSeconds);
			input.highGTurn = maneuverPhase < parameters.highGManeuverDurationSeconds &&
				std::abs(input.turn) > 0.3f;
			if (input.highGTurn) {
				input.turn = std::copysign((std::max)(std::abs(input.turn), 0.85f), input.turn);
			}
			return input;
		}

	private:
		float m_elapsedSeconds = 0.0f;
		float m_nextDirectionChange = 0.0f;
		float m_nextAltitudeChange = 0.0f;
		float m_targetYaw = 0.0f;
		float m_targetAltitude = 0.0f;
		std::size_t m_directionChangeIndex = 0;
		bool m_initialized = false;
	};
}
