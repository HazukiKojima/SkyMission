#include "AircraftCameraController.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Game {

	void AircraftCameraController::Initialize(float fovY, float aspect, float nearZ, float farZ) {
		m_camera.Initialize(fovY, aspect, nearZ, farZ);
		UpdateView();
	}

	void AircraftCameraController::OnResize(UINT width, UINT height) {
		m_camera.OnResize(width, height);
	}

	void AircraftCameraController::Update(float deltaSeconds, const Engine::InputSnapshot& input) {
		if (m_following) {
			UpdateFollow(deltaSeconds);
			UpdateView();
			return;
		}

		if (!input.active)
			return;

		// ローカル空間での前方/右方向ベクトルを計算
		XMFLOAT3 forward;
		XMFLOAT3 right;
		XMFLOAT3 up = {0.0f, 1.0f, 0.0f};

		// yaw/pitch から進行方向を算出
		XMVECTOR dir =
			XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);
		XMVECTOR f = XMVector3Normalize(dir);
		XMVECTOR r = XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&up), f));
		XMStoreFloat3(&forward, f);
		XMStoreFloat3(&right, r);

		float moveSpeed = m_moveSpeed * deltaSeconds;
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

		// Deltas include a drag released earlier in this frame, so short drags are not lost.
		m_yaw += static_cast<float>(input.mouseDeltaX) * m_mouseSensitivity;
		m_pitch -= static_cast<float>(input.mouseDeltaY) * m_mouseSensitivity;
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

	void AircraftCameraController::UpdateFollow(float deltaSeconds) {
		const XMVECTOR target = XMLoadFloat3(&m_followTarget);
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
		const float yawBlend = 1.0f - std::exp(-m_followDirectionSmoothing * deltaSeconds);
		m_followCameraYaw = std::remainder(
			m_followCameraYaw + std::remainder(targetYaw - m_followCameraYaw, XM_2PI) * yawBlend, XM_2PI);
		const XMVECTOR forward = XMLoadFloat3(&m_followForward);
		const float horizontalForwardLength = std::sqrt(
			XMVectorGetX(forward) * XMVectorGetX(forward) + XMVectorGetZ(forward) * XMVectorGetZ(forward));
		const float targetPitch =
			std::clamp(std::atan2(XMVectorGetY(forward), horizontalForwardLength) * 0.4f, -0.35f, 0.35f);
		const float pitchBlend = 1.0f - std::exp(-m_followDirectionSmoothing * deltaSeconds);
		m_followCameraPitch += (targetPitch - m_followCameraPitch) * pitchBlend;
		const float pitchCosine = std::cos(m_followCameraPitch);
		const XMVECTOR smoothedDirection = XMVectorSet(std::sin(m_followCameraYaw) * pitchCosine,
			std::sin(m_followCameraPitch), -std::cos(m_followCameraYaw) * pitchCosine, 0.0f);
		const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const float speedRange = std::max(m_followMaximumSpeed - 60.0f, 0.001f);
		const float speedRatio = std::clamp((m_followSpeed - 60.0f) / speedRange, 0.0f, 1.0f);
		const float followDistance = m_followDistance + (m_maxFollowDistance - m_followDistance) * speedRatio;
		const XMVECTOR desiredPosition =
			target - smoothedDirection * followDistance + worldUp * m_followHeight;
		const XMVECTOR desiredLookAt = target + smoothedDirection * 12.0f + worldUp * 2.0f;
		const float blend = 1.0f - std::exp(-m_followSmoothing * deltaSeconds);
		XMStoreFloat3(&m_position, XMVectorLerp(XMLoadFloat3(&m_position), desiredPosition, blend));
		XMStoreFloat3(&m_followLookAt, XMVectorLerp(XMLoadFloat3(&m_followLookAt), desiredLookAt, blend));
	}

	void AircraftCameraController::UpdateView() {
		XMVECTOR pos = XMLoadFloat3(&m_position);
		if (m_following) {
			m_camera.LookAt(m_position, m_followLookAt);
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
