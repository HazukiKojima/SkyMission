#pragma once
#include "AircraftFlightParameters.h"
#include "ValidateFlightParameters.h"
#include <DirectXMath.h>

namespace Game {
	struct AircraftInput {
		float pitch = 0.0f;
		float turn = 0.0f;
		float yaw = 0.0f;
		float throttle = 0.0f;
		bool highGTurn = false;
	};

	class AircraftFlightModel {
	  public:
		// 入力を受け取り、制御・姿勢・飛行物理・速度・位置の順に機体を更新する
		void Update(const AircraftInput& input, float deltaSeconds);
		// 飛行特性を設定し、機体速度を初期速度に戻す
		void SetFlightParameters(const AircraftFlightParameters& parameters) {
			ValidateFlightParameters(parameters);
			m_flightParameters = parameters;
			m_velocity = {0.0f, 0.0f, -parameters.initialSpeed};
		}
		DirectX::XMFLOAT3 GetPosition() const {
			return m_position;
		}
		void SetPosition(const DirectX::XMFLOAT3& position) {
			m_position = position;
		}
		float GetSpeed() const;
		float GetMaximumSpeed() const {
			return m_flightParameters.maximumSpeed;
		}
		// 機体の前方と、カメラ追従用の水平方向を返す
		DirectX::XMFLOAT3 GetForward() const;
		DirectX::XMFLOAT3 GetHeading() const;
		// 姿勢クォータニオンと位置から描画用ワールド行列を生成する
		DirectX::XMFLOAT4X4 GetWorldTransform() const;

	  private:
		// 固定刻み1回分の物理計算で共有する中間値
		struct FlightStepState {
			// 速度方向と、速度がほぼゼロのときに使う機首方向
			DirectX::XMFLOAT3 travelDirection = {0.0f, 0.0f, -1.0f};
			DirectX::XMFLOAT3 fallbackForward = {0.0f, 0.0f, -1.0f};
			// 揚力計算に使う機体上方向
			DirectX::XMFLOAT3 liftUp = {0.0f, 1.0f, 0.0f};
			float speed = 0.0f;
			float angleOfAttack = 0.0f;
			float angleControl = 1.0f;
			float turnSpeedFactor = 1.0f;
			float highGIntensity = 0.0f;
			float highGTurnFactor = 1.0f;
			float turnIntent = 0.0f;
			float bankLoad = 0.0f;
			bool autoRollCorrection = false;
		};

		void UpdateControls(const AircraftInput& input, float deltaSeconds);
		void UpdateAttitude(const AircraftInput& input, float deltaSeconds, FlightStepState& state);
		void CalculateFlightStepState(const AircraftInput& input, FlightStepState& state) const;
		float UpdatePitchAndRoll(const AircraftInput& input, float deltaSeconds, FlightStepState& state);
		void UpdateYaw(float deltaSeconds, FlightStepState& state);
		void UpdateLiftDirection(float levelRoll, FlightStepState& state) const;
		void UpdateFlightPhysics(
			const FlightStepState& state, DirectX::XMFLOAT3& acceleration, float& drag) const;
		void UpdateVelocity(const DirectX::XMFLOAT3& acceleration, float drag,
			const DirectX::XMFLOAT3& fallbackForward, float deltaSeconds);
		void UpdatePosition(float deltaSeconds);
		DirectX::XMFLOAT3 GetRight() const;
		DirectX::XMFLOAT3 GetUp() const;
		DirectX::XMFLOAT3 GetForwardForAttitude() const;

		// 機体の状態と姿勢
		DirectX::XMFLOAT3 m_position = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 m_velocity = {0.0f, 0.0f, -60.0f};
		DirectX::XMFLOAT4 m_attitude = {0.0f, 0.0f, 0.0f, 1.0f};
		// 連続した操縦角を保持し、描画・方向計算にはm_attitudeを使う
		float m_pitch = 0.0f;
		float m_yaw = 0.0f;
		float m_roll = 0.0f;
		// 入力に滑らかに追従する操縦値
		float m_pitchControl = 0.0f;
		float m_turnControl = 0.0f;
		float m_yawControl = 0.0f;
		float m_throttleControl = 0.0f;
		AircraftFlightParameters m_flightParameters;
	};
} // namespace Game