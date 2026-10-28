#include "../Camera/Camera.h"
#include "../Window/Window.h"
#include <Windowsx.h>
#include <algorithm>
#include <cmath>

using namespace DirectX;

namespace Engine {

	void Camera::Initialize(HWND hwnd, float fovY, float aspect, float nearZ, float farZ) {
		m_hwnd = hwnd;
		m_fovY = fovY;
		m_aspect = aspect;
		m_nearZ = nearZ;
		m_farZ = farZ;
		UpdateProjection();
		UpdateView();

		// カーソル位置を初期化
		POINT p;
		GetCursorPos(&p);
		ScreenToClient(m_hwnd, &p);
		m_prevCursorPos = p;
	}

	void Camera::OnResize(UINT width, UINT height) {
		if (height == 0)
			return;
		m_aspect = static_cast<float>(width) / static_cast<float>(height);
		UpdateProjection();
	}

	void Camera::Update(float deltaSeconds) {
		if (m_following) {
			UpdateFollow(deltaSeconds);
			UpdateView();
			return;
		}

		if (GetForegroundWindow() != m_hwnd) {
			if (m_rmbDown) {
				if (GetCapture() == m_hwnd)
					ReleaseCapture();
				m_rmbDown = false;
			}
			return;
		}
		// 入力ポーリング: キーボードで移動、右ボタン押下でマウス視点
		BYTE keys[256];
		GetKeyboardState(keys);

		// ローカル空間での前方/右方向ベクトルを計算
		XMFLOAT3 forward;
		XMFLOAT3 right;
		XMFLOAT3 up = {0.0f, 1.0f, 0.0f};

		// yaw/pitch から進行方向を算出
		XMVECTOR dir = XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);
		XMVECTOR f = XMVector3Normalize(dir);
		XMVECTOR r = XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&up), f));
		XMStoreFloat3(&forward, f);
		XMStoreFloat3(&right, r);

		float moveSpeed = m_moveSpeed * deltaSeconds;
		if (keys['W'] & 0x80) {
			m_position.x += forward.x * moveSpeed;
			m_position.y += forward.y * moveSpeed;
			m_position.z += forward.z * moveSpeed;
		}
		if (keys['S'] & 0x80) {
			m_position.x -= forward.x * moveSpeed;
			m_position.y -= forward.y * moveSpeed;
			m_position.z -= forward.z * moveSpeed;
		}
		if (keys['A'] & 0x80) {
			m_position.x -= right.x * moveSpeed;
			m_position.y -= right.y * moveSpeed;
			m_position.z -= right.z * moveSpeed;
		}
		if (keys['D'] & 0x80) {
			m_position.x += right.x * moveSpeed;
			m_position.y += right.y * moveSpeed;
			m_position.z += right.z * moveSpeed;
		}

		// マウス: 右ボタン押下時のみキャプチャして視点回転
		bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
		if (rmb) {
			if (!m_rmbDown) {
				// 押下直後: カーソルをキャプチャ
				SetCapture(m_hwnd);
				m_rmbDown = true;
				POINT p;
				GetCursorPos(&p);
				ScreenToClient(m_hwnd, &p);
				m_prevCursorPos = p;
			}

			POINT cur;
			GetCursorPos(&cur);
			ScreenToClient(m_hwnd, &cur);
			int dx = cur.x - m_prevCursorPos.x;
			int dy = cur.y - m_prevCursorPos.y;
			m_prevCursorPos = cur;

			m_yaw += dx * m_mouseSensitivity;	 // X軸回転
			m_pitch += -dy * m_mouseSensitivity; // Y軸回転（マウス上下は反転）
			// ピッチをクランプして反転を防止
			const float pitchLimit = XM_PIDIV2 - 0.01f;
			if (m_pitch > pitchLimit)
				m_pitch = pitchLimit;
			if (m_pitch < -pitchLimit)
				m_pitch = -pitchLimit;
		} else if (m_rmbDown) {
			// 押下解除時: キャプチャ解除
			m_rmbDown = false;
			ReleaseCapture();
		}

		UpdateView();
	}

	void Camera::SetFollowTarget(const XMFLOAT3& position, const XMFLOAT3& heading, const XMFLOAT3& forward,
								 bool pitchInputActive, float speed, float maximumSpeed) {
		m_following = true;
		m_followTarget = position;
		m_followHeading = heading;
		m_followForward = forward;
		m_followPitchInputActive = pitchInputActive;
		m_followSpeed = speed;
		m_followMaximumSpeed = maximumSpeed;
	}

	void Camera::UpdateFollow(float deltaSeconds) {
		const XMVECTOR target = XMLoadFloat3(&m_followTarget);
		XMVECTOR followDirection = XMVector3Normalize(XMLoadFloat3(&m_followHeading));
		if (!m_followPitchInputActive) {
			const XMVECTOR forward = XMLoadFloat3(&m_followForward);
			const XMVECTOR horizontalForward = XMVectorSet(XMVectorGetX(forward), 0.0f, XMVectorGetZ(forward), 0.0f);
			if (XMVectorGetX(XMVector3LengthSq(horizontalForward)) > 0.0001f) {
				followDirection = XMVector3Normalize(horizontalForward);
			}
		}
		const float targetYaw = std::atan2(XMVectorGetX(followDirection), -XMVectorGetZ(followDirection));
		const float yawBlend = 1.0f - std::exp(-m_followDirectionSmoothing * deltaSeconds);
		m_followCameraYaw = std::remainder(
			m_followCameraYaw + std::remainder(targetYaw - m_followCameraYaw, XM_2PI) * yawBlend, XM_2PI);
		const XMVECTOR forward = XMLoadFloat3(&m_followForward);
		const float horizontalForwardLength =
			std::sqrt(XMVectorGetX(forward) * XMVectorGetX(forward) + XMVectorGetZ(forward) * XMVectorGetZ(forward));
		const float targetPitch =
			std::clamp(std::atan2(XMVectorGetY(forward), horizontalForwardLength) * 0.4f, -0.35f, 0.35f);
		const float pitchBlend = 1.0f - std::exp(-m_followDirectionSmoothing * deltaSeconds);
		m_followCameraPitch += (targetPitch - m_followCameraPitch) * pitchBlend;
		const float pitchCosine = std::cos(m_followCameraPitch);
		const XMVECTOR smoothedDirection =
			XMVectorSet(std::sin(m_followCameraYaw) * pitchCosine, std::sin(m_followCameraPitch),
						-std::cos(m_followCameraYaw) * pitchCosine, 0.0f);
		const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		const float speedRange = std::max(m_followMaximumSpeed - 60.0f, 0.001f);
		const float speedRatio = std::clamp((m_followSpeed - 60.0f) / speedRange, 0.0f, 1.0f);
		const float followDistance = m_followDistance + (m_maxFollowDistance - m_followDistance) * speedRatio;
		const XMVECTOR desiredPosition = target - smoothedDirection * followDistance + worldUp * m_followHeight;
		const XMVECTOR desiredLookAt = target + smoothedDirection * 12.0f + worldUp * 2.0f;
		const float blend = 1.0f - std::exp(-m_followSmoothing * deltaSeconds);
		XMStoreFloat3(&m_position, XMVectorLerp(XMLoadFloat3(&m_position), desiredPosition, blend));
		XMStoreFloat3(&m_followLookAt, XMVectorLerp(XMLoadFloat3(&m_followLookAt), desiredLookAt, blend));
	}

	void Camera::UpdateProjection() {
		m_proj = XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
	}

	void Camera::UpdateView() {
		XMVECTOR pos = XMLoadFloat3(&m_position);
		if (m_following) {
			m_view = XMMatrixLookAtLH(pos, XMLoadFloat3(&m_followLookAt), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
			return;
		}
		XMVECTOR lookDir = XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);
		XMVECTOR target = pos + XMVector3Normalize(lookDir);
		XMVECTOR up = XMVectorSet(0, 1, 0, 0);
		m_view = XMMatrixLookAtLH(pos, target, up);
	}

} // namespace Engine
