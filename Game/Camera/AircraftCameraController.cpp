#include "AircraftCameraController.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Game {

	void AircraftCameraController::Initialize(float fovY, float aspect, float nearZ, float farZ) {
		m_camera.Initialize(fovY, aspect, nearZ, farZ);
		m_lookYaw = m_lookPitch = 0;
		UpdateView();
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

		if (input.active && input.middleMouseDown) {
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
	}

	void AircraftCameraController::UpdateFollow(float deltaSeconds, const Engine::InputSnapshot& input) {
		UpdateLookOffset(deltaSeconds, input);
		UpdateFollowOrientation(deltaSeconds);
		UpdateFollowPosition(deltaSeconds);
	}
	void AircraftCameraController::SnapToFollowTarget() {
		if (!m_following) return;
		m_lookYaw = 0;
		m_lookPitch = 0;
		UpdateFollowOrientation(0, true);
		UpdateFollowPosition(0, true);
		UpdateView();
	}
	void AircraftCameraController::UpdateLookOffset(float deltaSeconds, const Engine::InputSnapshot& input) {
		if (input.active && input.middleMouseDown) {
			m_lookYaw = std::remainder(
				m_lookYaw - static_cast<float>(input.mouseDeltaX) * m_settings.mouseSensitivity, XM_2PI);
			m_lookPitch =
				std::clamp(m_lookPitch - static_cast<float>(input.mouseDeltaY) * m_settings.mouseSensitivity,
					-m_settings.lookPitchLimit, m_settings.lookPitchLimit);
		} else {
			// 機体の追従とは別に視点のオフセットを減衰させ、解放時に瞬間移動しない。
			const float decay = std::exp(-m_settings.lookReturnRate * deltaSeconds);
			m_lookYaw *= decay;
			m_lookPitch *= decay;
		}
	}
	void AircraftCameraController::UpdateFollowOrientation(float deltaSeconds, bool snap) {
		XMVECTOR followDirection = XMVector3Normalize(XMLoadFloat3(&m_followHeading));
		if (!m_followPitchInputActive) {
			const XMVECTOR forward = XMLoadFloat3(&m_followForward);
			const XMVECTOR horizontalForward =
				XMVectorSet(XMVectorGetX(forward), 0.0f, XMVectorGetZ(forward), 0.0f);
			if (XMVectorGetX(XMVector3LengthSq(horizontalForward)) > 0.0001f) {
				followDirection = XMVector3Normalize(horizontalForward);
			}
		}

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
	}
	void AircraftCameraController::UpdateFollowPosition(float deltaSeconds, bool snap) {
		const XMVECTOR target = XMLoadFloat3(&m_followTarget);
		const float orbitYaw = m_followCameraYaw + m_lookYaw;
		const float orbitPitch = std::clamp(
			m_followCameraPitch + m_lookPitch, -m_settings.orbitPitchLimit, m_settings.orbitPitchLimit);
		const float pitchCosine = std::cos(orbitPitch);
		const XMVECTOR smoothedDirection = XMVectorSet(
			std::sin(orbitYaw) * pitchCosine, std::sin(orbitPitch), -std::cos(orbitYaw) * pitchCosine, 0.0f);
		const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const float speedRange = std::max(m_followMaximumSpeed - m_settings.speedDistanceStart, 0.001f);
		const float speedRatio =
			std::clamp((m_followSpeed - m_settings.speedDistanceStart) / speedRange, 0.0f, 1.0f);
		const float followDistance = m_settings.followDistance +
			(m_settings.maxFollowDistance - m_settings.followDistance) * speedRatio;
		const XMVECTOR desiredPosition =
			target - smoothedDirection * followDistance + worldUp * m_settings.followHeight;
		const XMVECTOR desiredLookAt =
			target + smoothedDirection * m_settings.lookAheadMeters + worldUp * m_settings.followLookHeight;
		// 大きなワールド座標同士の減算で視線が揺れないよう、相対オフセットから求める。
		XMStoreFloat3(&m_followViewDirection,
			smoothedDirection * (followDistance + m_settings.lookAheadMeters) +
				worldUp * (m_settings.followLookHeight - m_settings.followHeight));
		const float blend = snap ? 1.0f : 1.0f - std::exp(-m_settings.followSmoothing * deltaSeconds);
		XMStoreFloat3(&m_position, XMVectorLerp(XMLoadFloat3(&m_position), desiredPosition, blend));
		XMStoreFloat3(&m_followLookAt, XMVectorLerp(XMLoadFloat3(&m_followLookAt), desiredLookAt, blend));
	}

	void AircraftCameraController::UpdateView() {
		XMVECTOR pos = XMLoadFloat3(&m_position);
		if (m_following) {
			m_camera.LookTo(m_position, m_followViewDirection);
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
