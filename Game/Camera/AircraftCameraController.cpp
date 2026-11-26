#include "AircraftCameraController.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Game {

	void AircraftCameraController::Initialize(float fovY, float aspect, float nearZ, float farZ) {
		m_camera.Initialize(fovY, aspect, nearZ, farZ);
		m_baseFovY = fovY;
		m_zoomScale = 1;
		m_automaticLook = false;
		m_lookYaw = m_lookPitch = 0;
		m_lookTarget.reset();
		ResetShake();
	}

	void AircraftCameraController::OnResize(UINT width, UINT height) {
		m_camera.OnResize(width, height);
	}

	void AircraftCameraController::Update(float deltaSeconds, const Engine::InputSnapshot& input) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) return;
		if (m_following) {
			UpdateFollow(deltaSeconds, input);
			UpdateView();
			return;
		}

		if (!input.active) return;

		XMFLOAT3 forward;
		XMFLOAT3 right;
		XMFLOAT3 up = {0.0f, 1.0f, 0.0f};

		// ヨー・ピッチから自由移動の基準方向を求める
		XMVECTOR dir =
			XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);
		XMVECTOR f = XMVector3Normalize(dir);
		XMVECTOR r = XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&up), f));
		XMStoreFloat3(&forward, f);
		XMStoreFloat3(&right, r);

		float moveSpeed = m_settings.moveSpeed * deltaSeconds;

		if (input.IsKeyDown('W')) {
			m_position.x += forward.x * moveSpeed;
			m_position.y += forward.y * moveSpeed;
			m_position.z += forward.z * moveSpeed;
		}

		if (input.IsKeyDown('S')) {
			m_position.x -= forward.x * moveSpeed;
			m_position.y -= forward.y * moveSpeed;
			m_position.z -= forward.z * moveSpeed;
		}

		if (input.IsKeyDown('A')) {
			m_position.x -= right.x * moveSpeed;
			m_position.y -= right.y * moveSpeed;
			m_position.z -= right.z * moveSpeed;
		}

		if (input.IsKeyDown('D')) {
			m_position.x += right.x * moveSpeed;
			m_position.y += right.y * moveSpeed;
			m_position.z += right.z * moveSpeed;
		}

		if (input.mouseDeltaX != 0 || input.mouseDeltaY != 0) {
			m_yaw += static_cast<float>(input.mouseDeltaX) * m_settings.mouseSensitivity;
			m_pitch -= static_cast<float>(input.mouseDeltaY) * m_settings.mouseSensitivity;
		}

		const float pitchLimit = XM_PIDIV2 - 0.01f;
		m_pitch = std::clamp(m_pitch, -pitchLimit, pitchLimit);

		UpdateView();
	}

	void AircraftCameraController::SetFollowTarget(const AircraftCameraTarget& target) {
		m_following = true;
		m_followTarget = target.position;
		m_followHeading = target.heading;
		m_followForward = target.forward;
		m_followPitchInputActive = target.pitchInputActive;
		m_followSpeed = target.speed;
		m_followMaximumSpeed = target.maximumSpeed;
		m_targetBank = std::sin(target.rollRadians);
	}

	void AircraftCameraController::UpdateFollow(float deltaSeconds, const Engine::InputSnapshot& input) {
		UpdateFollowOrientation(deltaSeconds);
		UpdateLookOffset(deltaSeconds, input);
		const float zoom = m_automaticLook ? 0.85f : 1.0f;
		m_zoomScale += (zoom - m_zoomScale) * (1 - std::exp(-m_settings.lookReturnRate * deltaSeconds));
		m_camera.SetVerticalFov(m_baseFovY * m_zoomScale);
		UpdateFollowPosition(deltaSeconds);
	}

	void AircraftCameraController::SnapToFollowTarget() {
		m_lookTarget.reset();
		m_automaticLook = false;
		m_zoomScale = 1;
		m_camera.SetVerticalFov(m_baseFovY);

		if (!m_following) return;

		m_lookYaw = 0;
		m_lookPitch = 0;
		UpdateFollowOrientation(0, true);
		UpdateFollowPosition(0, true);
		ResetShake();
	}

	void AircraftCameraController::UpdateLookOffset(float deltaSeconds, const Engine::InputSnapshot& input) {
		const auto& pad = input.gamepad;
		const bool stickLook = pad.rightX != 0 || pad.rightY != 0;
		const bool mouseLook = input.middleMouseDown || input.mouseDeltaX != 0 || input.mouseDeltaY != 0;
		if (input.active && !m_automaticLook && (mouseLook || stickLook)) {
			constexpr float stickLookSpeed = XM_PI;
			const float yawDelta = static_cast<float>(input.mouseDeltaX) * m_settings.mouseSensitivity +
				pad.rightX * stickLookSpeed * deltaSeconds;
			const float pitchDelta = static_cast<float>(input.mouseDeltaY) * m_settings.mouseSensitivity +
				pad.rightY * stickLookSpeed * deltaSeconds;
			m_lookYaw = std::remainder(m_lookYaw - yawDelta, XM_2PI);
			m_lookPitch = std::clamp(m_lookPitch - pitchDelta, -m_settings.lookPitchLimit, m_settings.lookPitchLimit);
		} else {
			float targetYaw = 0, targetPitch = 0;

			if ((m_automaticLook || input.IsTargetLookDown()) && m_lookTarget) {
				const auto& point = *m_lookTarget;

				// doubleで差と距離を求め、極端な座標でも角度の計算を有限に保つ
				const double dx = double(point.x) - m_followTarget.x;
				const double dy = double(point.y) - m_followTarget.y;
				const double dz = double(point.z) - m_followTarget.z;
				const double horizontal = std::hypot(dx, dz);
				const double distance = std::hypot(horizontal, dy);

				if (std::isfinite(distance) && distance > 0.001) {
					// 真上・真下ではヨーを維持し、水平角の不定による反転を避ける
					targetYaw = horizontal > 0.001
						? std::remainder(float(std::atan2(dx, -dz)) - m_followCameraYaw, XM_2PI)
						: m_lookYaw;
					targetPitch = std::clamp(float(std::atan2(dy, horizontal)) - m_followCameraPitch,
						-m_settings.lookPitchLimit, m_settings.lookPitchLimit);
				}
			}

			// 自動注視と標準視点への復帰で同じ時間基準の補間を使う
			const float decay = std::exp(-m_settings.lookReturnRate * deltaSeconds);
			const float blend = 1.0f - decay;
			m_lookYaw = std::remainder(m_lookYaw +
				std::remainder(targetYaw - m_lookYaw, XM_2PI) * blend, XM_2PI);
			m_lookPitch += (targetPitch - m_lookPitch) * blend;
		}
	}

	void AircraftCameraController::UpdateFollowOrientation(float deltaSeconds, bool snap) {
		XMVECTOR followDirection = XMVector3Normalize(XMLoadFloat3(&m_followHeading));

		// ピッチ操作中は機首の水平成分への切替を避ける
		if (!m_followPitchInputActive) {
			const XMVECTOR forward = XMLoadFloat3(&m_followForward);
			const XMVECTOR horizontalForward =
				XMVectorSet(XMVectorGetX(forward), 0.0f, XMVectorGetZ(forward), 0.0f);
			if (XMVectorGetX(XMVector3LengthSq(horizontalForward)) > 0.0001f) {
				followDirection = XMVector3Normalize(horizontalForward);
			}
		}

		// 角度の折り返しをまたいでも短い側へ補間する
		const float targetYaw = std::atan2(XMVectorGetX(followDirection), -XMVectorGetZ(followDirection));
		const float yawBlend =
			snap ? 1.0f : 1.0f - std::exp(-m_settings.followDirectionSmoothing * deltaSeconds);
		m_followCameraYaw = std::remainder(
			m_followCameraYaw + std::remainder(targetYaw - m_followCameraYaw, XM_2PI) * yawBlend, XM_2PI);
		const XMVECTOR forward = XMLoadFloat3(&m_followForward);
		const float horizontalForwardLength = std::sqrt(
			XMVectorGetX(forward) * XMVectorGetX(forward) + XMVectorGetZ(forward) * XMVectorGetZ(forward));
		const float targetPitch = std::clamp(
			std::atan2(XMVectorGetY(forward), horizontalForwardLength) * m_settings.pitchFollowScale,
			-m_settings.pitchFollowLimit, m_settings.pitchFollowLimit);
		const float pitchBlend = yawBlend;
		m_followCameraPitch += (targetPitch - m_followCameraPitch) * pitchBlend;
		m_followBank += (m_targetBank - m_followBank) * yawBlend;
	}

	void AircraftCameraController::UpdateFollowPosition(float deltaSeconds, bool snap) {
		const XMVECTOR target = XMLoadFloat3(&m_followTarget);

		// 追従方向に見回し角を加えて視点の向きを求める
		const float orbitYaw = m_followCameraYaw + m_lookYaw;
		const float orbitPitch = std::clamp(
			m_followCameraPitch + m_lookPitch, -m_settings.orbitPitchLimit, m_settings.orbitPitchLimit);
		const float pitchCosine = std::cos(orbitPitch);
		const XMVECTOR smoothedDirection = XMVectorSet(
			std::sin(orbitYaw) * pitchCosine, std::sin(orbitPitch), -std::cos(orbitYaw) * pitchCosine, 0.0f);
		const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

		// 速度が上がるほど機体からカメラを離す
		const float speedRange = std::max(m_followMaximumSpeed - m_settings.speedDistanceStart, 0.001f);
		const float speedRatio =
			std::clamp((m_followSpeed - m_settings.speedDistanceStart) / speedRange, 0.0f, 1.0f);
		const float followDistance = m_settings.followDistance +
			(m_settings.maxFollowDistance - m_settings.followDistance) * speedRatio;
		// 見回しと目標注視では旋回時の構図補正を徐々に弱める
		const float chaseWeight = 1.0f -
			std::clamp(std::hypot(m_lookYaw, m_lookPitch) / 0.5f, 0.0f, 1.0f);
		const float bank = m_followBank * chaseWeight;
		m_viewRoll = -bank * XMConvertToRadians(m_settings.bankRollDegrees);
		const XMVECTOR right = XMVectorSet(-std::cos(orbitYaw), 0, -std::sin(orbitYaw), 0);
		const XMVECTOR desiredPosition =
			target - smoothedDirection * followDistance + worldUp * m_settings.followHeight -
			right * (bank * m_settings.bankOffsetMeters);
		const XMVECTOR desiredLookAt =
			target + smoothedDirection * m_settings.lookAheadMeters + worldUp * m_settings.followLookHeight;

		// 大きなワールド座標同士の減算で視線が揺れないよう、相対オフセットから求める
		XMStoreFloat3(&m_followViewDirection,
			smoothedDirection * (followDistance + m_settings.lookAheadMeters) +
				worldUp * (m_settings.followLookHeight - m_settings.followHeight));
		const float blend = snap ? 1.0f : 1.0f - std::exp(-m_settings.followSmoothing * deltaSeconds);
		XMStoreFloat3(&m_position, XMVectorLerp(XMLoadFloat3(&m_position), desiredPosition, blend));
		XMStoreFloat3(&m_followLookAt, XMVectorLerp(XMLoadFloat3(&m_followLookAt), desiredLookAt, blend));
	}

	void AircraftCameraController::ResetShake() {
		m_shakePhase = m_flightShakeDegrees = m_impactDegrees = m_impactSeconds = m_impactDuration = 0;
		UpdateView();
	}

	void AircraftCameraController::UpdateShake(float deltaSeconds, const AircraftCameraShakeInput& input) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0) return;

		const auto& settings = m_settings.shake;
		const float acceleration = std::clamp(input.acceleration, 0.0f, 1.0f);
		const float flightDegrees = std::max(settings.accelerationDegrees * acceleration * acceleration,
			input.highG ? settings.highGDegrees : 0.0f);
		m_flightShakeDegrees = std::lerp(m_flightShakeDegrees, flightDegrees,
			1.0f - std::exp(-settings.responsePerSecond * deltaSeconds));
		m_shakePhase = std::fmod(m_shakePhase + XM_2PI * settings.frequencyHz * deltaSeconds, XM_2PI);
		m_impactSeconds = std::max(0.0f, m_impactSeconds - deltaSeconds);

		// 同時に起きた衝撃は最も強いものを採用し、連続被弾でも加算しない
		const auto startImpact = [&](float degrees, float duration) {
			const float current = m_impactSeconds > 0 ? m_impactDegrees * m_impactSeconds / m_impactDuration : 0;
			if (duration > 0 && degrees > 0 && degrees >= current) {
				m_impactDegrees = degrees;
				m_impactSeconds = m_impactDuration = duration;
			}
		};
		if (input.gunFired) startImpact(settings.gunFireDegrees, settings.gunFireSeconds);
		if (input.missileLaunched) startImpact(settings.missileLaunchDegrees, settings.missileLaunchSeconds);
		if (input.gunHit) startImpact(settings.gunHitDegrees, settings.hitSeconds);
		if (input.heavyHit) startImpact(settings.heavyHitDegrees, settings.hitSeconds);
		startImpact(settings.enemyDestroyedDegrees * input.explosionImpact, settings.enemyDestroyedSeconds);
	}

	void AircraftCameraController::UpdateView() {
		XMVECTOR pos = XMLoadFloat3(&m_position);

		if (m_following) {
			const float remaining = m_impactSeconds > 0 ? m_impactSeconds / m_impactDuration : 0;
			// 飛行中の振動に、射撃や被弾などの短い衝撃を重ねる
			const float amplitude = XMConvertToRadians(
				m_flightShakeDegrees + m_impactDegrees * remaining);
			const XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&m_followViewDirection));
			const XMVECTOR right = XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), forward));
			const XMVECTOR up = XMVector3Cross(forward, right);
			// 追従位置や補間状態に揺れを戻さず、描画する視線だけをずらす
			XMFLOAT3 direction;
			XMStoreFloat3(&direction, forward + right * (std::sin(m_shakePhase) * amplitude * 0.65f) +
				up * (std::cos(m_shakePhase) * amplitude));
			XMFLOAT3 cameraUp;
			XMStoreFloat3(&cameraUp, XMVector3Rotate(up, XMQuaternionRotationNormal(forward, m_viewRoll)));
			m_camera.LookTo(m_position, direction, cameraUp);
			return;
		}

		XMVECTOR lookDir =
			XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);
		XMVECTOR target = pos + XMVector3Normalize(lookDir);
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		XMFLOAT3 targetPosition;
		XMStoreFloat3(&targetPosition, target);
		m_camera.LookAt(m_position, targetPosition);
	}

} // namespace Game
