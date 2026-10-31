#include "AircraftFlightModel.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Game {
	namespace {
		constexpr float kMaximumFrameSeconds = 0.25f;
		constexpr float kPhysicsStepSeconds = 1.0f / 60.0f;
		float OrientationSign(float angle) {
			return std::cos(angle) < 0.0f ? -1.0f : 1.0f;
		}

		float AngleResponse(float angle, const AircraftFlightParameters& p, float minimum) {
			return std::clamp(1.0f -
					std::max(0.0f, angle - p.stallAngleRadians) /
						(p.criticalAngleRadians - p.stallAngleRadians),
				minimum, 1.0f);
		}
	} // namespace

	void AircraftFlightModel::Update(const AircraftInput& input, float deltaSeconds) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f) {
			return;
		}

		// 経過時間を制限し、最大1/60秒刻みで挙動を安定させる
		float remainingSeconds = std::min(deltaSeconds, kMaximumFrameSeconds);
		while (remainingSeconds > 0.0f) {
			const float step = std::min(remainingSeconds, kPhysicsStepSeconds);
			remainingSeconds -= step;

			// 各刻みで操縦入力、姿勢、力、速度、位置の順に更新する
			UpdateControls(input, step);
			FlightStepState state;
			UpdateAttitude(input, step, state);

			XMFLOAT3 acceleration;
			float drag = 0.0f;
			UpdateFlightPhysics(state, acceleration, drag);
			UpdateVelocity(acceleration, drag, state.fallbackForward, step);
			UpdatePosition(step);
		}
	}

	void AircraftFlightModel::UpdateControls(const AircraftInput& input, float deltaSeconds) {
		// 外部入力の範囲を制限し、指数補間で機体操作を滑らかにする
		const float targetPitchControl = std::clamp(input.pitch, -1.0f, 1.0f);
		const float targetTurnControl = std::clamp(input.turn, -1.0f, 1.0f);
		const float targetYawControl = std::clamp(input.yaw, -1.0f, 1.0f);
		const float targetThrottleControl = std::clamp(input.throttle, -1.0f, 1.0f);
		const float controlBlend = 1.0f - std::exp(-m_flightParameters.controlResponse * deltaSeconds);

		m_pitchControl += (targetPitchControl - m_pitchControl) * controlBlend;
		const float turnInputDirection = OrientationSign(m_pitch);
		m_turnControl += (targetTurnControl * turnInputDirection - m_turnControl) * controlBlend;
		m_yawControl += (targetYawControl - m_yawControl) * controlBlend;
		m_throttleControl += (targetThrottleControl - m_throttleControl) * controlBlend;
	}

	void AircraftFlightModel::UpdateAttitude(
		const AircraftInput& input, float deltaSeconds, FlightStepState& state) {
		CalculateFlightStepState(input, state);
		const float levelRoll = UpdatePitchAndRoll(input, deltaSeconds, state);
		UpdateYaw(deltaSeconds, state);
		UpdateLiftDirection(levelRoll, state);
	}

	void AircraftFlightModel::CalculateFlightStepState(
		const AircraftInput& input, FlightStepState& state) const {
		// 姿勢変更前の速度と機首方向から、迎角や速度依存の操縦性能を求める
		const XMFLOAT3 currentForward = GetForwardForAttitude();
		state.fallbackForward = currentForward;
		const XMVECTOR forward = XMLoadFloat3(&currentForward);
		const XMVECTOR velocity = XMLoadFloat3(&m_velocity);
		state.speed = XMVectorGetX(XMVector3Length(velocity));
		const XMVECTOR travelDirection = state.speed > 0.001f ? XMVector3Normalize(velocity) : forward;
		XMStoreFloat3(&state.travelDirection, travelDirection);

		const float alignment = std::clamp(XMVectorGetX(XMVector3Dot(forward, travelDirection)), -1.0f, 1.0f);
		state.angleOfAttack = std::acos(alignment);
		state.angleControl = AngleResponse(state.angleOfAttack, m_flightParameters, 0.2f);
		state.turnSpeedFactor = std::clamp(
			m_flightParameters.liftReferenceSpeed / std::max(state.speed, m_flightParameters.minimumSpeed),
			m_flightParameters.minimumTurnFactor, m_flightParameters.maximumTurnFactor);
		state.turnIntent = std::clamp(std::max(std::abs(m_turnControl), std::abs(m_yawControl)), 0.0f, 1.0f);
		state.highGIntensity = input.highGTurn && state.turnIntent > 0.05f ? 1.0f : 0.0f;
		state.highGTurnFactor = 1.0f + state.highGIntensity * (m_flightParameters.highGTurnMultiplier - 1.0f);
	}

	float AircraftFlightModel::UpdatePitchAndRoll(
		const AircraftInput& input, float deltaSeconds, FlightStepState& state) {
		// 半回転時の角度の曖昧さを避けるため、制御角は連続して更新する
		if (std::abs(input.pitch) > 0.01f) {
			const float pitchDirection = OrientationSign(m_roll);
			m_pitch = std::remainder(m_pitch +
					m_pitchControl * pitchDirection * m_flightParameters.pitchRateRadians * deltaSeconds,
				XM_2PI);
		}

		const bool pitchInputActive = std::abs(input.pitch) > 0.01f && !input.autoLevelRoll;
		const bool turnInputActive = std::abs(input.turn) > 0.01f;
		state.autoRollCorrection = !pitchInputActive && !turnInputActive;
		const float levelRoll = std::cos(m_pitch) < 0.0f ? XM_PI : 0.0f;
		const float currentRoll = m_roll;
		float turnRollBase = levelRoll;
		if (pitchInputActive)
			turnRollBase = std::cos(currentRoll) < 0.0f ? XM_PI : 0.0f;
		const float turnDirection = OrientationSign(turnRollBase);
		const float turnBank = std::clamp(
			-m_turnControl * turnDirection * m_flightParameters.maximumBankRadians * state.turnSpeedFactor,
			-m_flightParameters.maximumBankRadians, m_flightParameters.maximumBankRadians);
		float targetRoll = levelRoll;
		if (turnInputActive)
			targetRoll = turnRollBase + turnBank;
		else if (pitchInputActive)
			targetRoll = currentRoll;
		const float rollResponse =
			turnInputActive ? m_flightParameters.rollInputResponse : m_flightParameters.rollLevelRate;
		// 旋回中は目標バンクへ追従し、入力がないときは水平姿勢へ戻す
		const float rollDifference = std::remainder(targetRoll - m_roll, XM_2PI);
		const float nearLevelingFactor = state.autoRollCorrection
			? 1.0f - std::clamp(std::abs(rollDifference) / XM_PIDIV4, 0.0f, 1.0f)
			: 0.0f;
		const float adjustedRollResponse = rollResponse * (1.0f + 0.5f * nearLevelingFactor);
		const float rollBlend = 1.0f - std::exp(-adjustedRollResponse * deltaSeconds);
		m_roll = std::remainder(m_roll + rollDifference * rollBlend, XM_2PI);
		return levelRoll;
	}

	void AircraftFlightModel::UpdateYaw(float deltaSeconds, FlightStepState& state) {
		state.bankLoad = std::sin(m_roll);
		const float yawTurnDirection = OrientationSign(m_pitch);
		const float bankYawRate = state.autoRollCorrection
			? 0.0f
			: state.bankLoad * yawTurnDirection * m_flightParameters.turnRateRadians;
		m_yaw = std::remainder(m_yaw +
				(bankYawRate + m_yawControl * m_flightParameters.yawRateRadians) * state.angleControl *
					state.turnSpeedFactor * state.highGTurnFactor * deltaSeconds,
			XM_2PI);
		// 制御角から姿勢用の正規化クォータニオンを生成する
		XMStoreFloat4(
			&m_attitude, XMQuaternionNormalize(XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, m_roll)));
	}

	void AircraftFlightModel::UpdateLiftDirection(float levelRoll, FlightStepState& state) const {
		if (state.autoRollCorrection) {
			// 自動水平化中は水平な姿勢の上方向を揚力に使う
			const XMMATRIX liftRotation = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, levelRoll);
			XMStoreFloat3(&state.liftUp,
				XMVector3Normalize(
					XMVector3TransformNormal(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), liftRotation)));
		} else {
			state.liftUp = GetUp();
		}
	}

	void AircraftFlightModel::UpdateFlightPhysics(
		const FlightStepState& state, XMFLOAT3& acceleration, float& drag) const {
		const XMFLOAT3 forwardValue = GetForwardForAttitude();
		const XMVECTOR aircraftForward = XMLoadFloat3(&forwardValue);
		const XMVECTOR up = XMLoadFloat3(&state.liftUp);
		// 迎角と速度に応じた揚力を求め、推力・重力・飛行経路補正と合成する
		const float angleLift = AngleResponse(state.angleOfAttack, m_flightParameters, 0.0f);
		const float speedRatio = state.speed / m_flightParameters.liftReferenceSpeed;
		const float verticalLiftAssist = 1.0f / std::max(XMVectorGetY(up), 0.35f);
		const float liftAcceleration =
			std::min(m_flightParameters.gravity * speedRatio * speedRatio * angleLift * verticalLiftAssist,
				m_flightParameters.gravity * m_flightParameters.maximumLiftFactor);
		const XMVECTOR flightPathAcceleration = (aircraftForward - XMLoadFloat3(&state.travelDirection)) *
			(state.speed * m_flightParameters.flightPathResponse * state.angleControl *
				state.turnSpeedFactor * state.highGTurnFactor);
		const XMVECTOR totalAcceleration = up * liftAcceleration +
			XMVectorSet(0.0f, -m_flightParameters.gravity, 0.0f, 0.0f) +
			aircraftForward * (m_throttleControl * m_flightParameters.acceleration) + flightPathAcceleration;
		XMStoreFloat3(&acceleration, totalAcceleration);
		// バンク旋回とハイG入力は抗力を増加させる
		drag = m_flightParameters.drag + m_flightParameters.turnDrag * state.bankLoad * state.bankLoad +
			m_flightParameters.highGDrag * state.highGIntensity * state.turnIntent;
	}

	void AircraftFlightModel::UpdateVelocity(
		const XMFLOAT3& acceleration, float drag, const XMFLOAT3& fallbackForward, float deltaSeconds) {
		XMVECTOR velocity = XMLoadFloat3(&m_velocity) + XMLoadFloat3(&acceleration) * deltaSeconds;
		velocity *= std::exp(-drag * deltaSeconds);
		const float updatedSpeed = XMVectorGetX(XMVector3Length(velocity));
		// 速度上限を守り、下限を割った場合は直前の機首方向へ戻す
		if (updatedSpeed > m_flightParameters.maximumSpeed) {
			velocity = XMVector3Normalize(velocity) * m_flightParameters.maximumSpeed;
		} else if (updatedSpeed < m_flightParameters.minimumSpeed) {
			velocity = XMVector3Normalize(XMLoadFloat3(&fallbackForward)) * m_flightParameters.minimumSpeed;
		}

		XMStoreFloat3(&m_velocity, velocity);
	}

	void AircraftFlightModel::UpdatePosition(float deltaSeconds) {
		// 更新後の速度を積分してワールド位置を進める
		XMVECTOR position = XMLoadFloat3(&m_position) + XMLoadFloat3(&m_velocity) * deltaSeconds;
		XMStoreFloat3(&m_position, position);
	}

	XMFLOAT3 AircraftFlightModel::GetForwardForAttitude() const {
		// 機体ローカルの前方(-Z)を姿勢クォータニオンでワールド空間へ変換する
		XMFLOAT3 forward;
		XMStoreFloat3(&forward,
			XMVector3Normalize(
				XMVector3Rotate(XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), XMLoadFloat4(&m_attitude))));
		return forward;
	}

	XMFLOAT3 AircraftFlightModel::GetForward() const {
		return GetForwardForAttitude();
	}

	XMFLOAT3 AircraftFlightModel::GetRight() const {
		XMFLOAT3 right;
		XMStoreFloat3(&right,
			XMVector3Normalize(
				XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), XMLoadFloat4(&m_attitude))));
		return right;
	}

	XMFLOAT3 AircraftFlightModel::GetUp() const {
		XMFLOAT3 up;
		XMStoreFloat3(&up,
			XMVector3Normalize(
				XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMLoadFloat4(&m_attitude))));
		return up;
	}

	float AircraftFlightModel::GetSpeed() const {
		return XMVectorGetX(XMVector3Length(XMLoadFloat3(&m_velocity)));
	}

	XMFLOAT3 AircraftFlightModel::GetHeading() const {
		// カメラ用に機首方向を水平面へ射影する
		XMFLOAT3 forward = GetForward();
		forward.y = 0.0f;
		const XMVECTOR horizontalForward = XMLoadFloat3(&forward);
		if (XMVectorGetX(XMVector3LengthSq(horizontalForward)) > 0.0001f) {
			XMFLOAT3 heading;
			XMStoreFloat3(&heading, XMVector3Normalize(horizontalForward));
			return heading;
		}

		// 機首がほぼ垂直のときは右方向から水平な向きを補う
		const XMFLOAT3 right = GetRight();
		XMFLOAT3 heading = {right.z, 0.0f, -right.x};
		XMStoreFloat3(&heading, XMVector3Normalize(XMLoadFloat3(&heading)));
		return heading;
	}

	XMFLOAT4X4 AircraftFlightModel::GetWorldTransform() const {
		XMFLOAT4X4 world;
		XMStoreFloat4x4(&world,
			XMMatrixRotationQuaternion(XMLoadFloat4(&m_attitude)) *
				XMMatrixTranslation(m_position.x, m_position.y, m_position.z));
		return world;
	}

} // namespace Game
