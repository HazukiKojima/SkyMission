#include "Camera.h"
#include "Window.h"
#include <Windowsx.h>

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
	POINT p; GetCursorPos(&p); ScreenToClient(m_hwnd, &p);
	m_prevCursorPos = p;
}

void Camera::OnResize(UINT width, UINT height) {
	if (height == 0) return;
	m_aspect = static_cast<float>(width) / static_cast<float>(height);
	UpdateProjection();
}

void Camera::Update(float deltaSeconds) {
	// 入力ポーリング: キーボードで移動、右ボタン押下でマウス視点
	BYTE keys[256];
	GetKeyboardState(keys);

	// ローカル空間での前方/右方向ベクトルを計算
	XMFLOAT3 forward; XMFLOAT3 right;
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
			POINT p; GetCursorPos(&p); ScreenToClient(m_hwnd, &p); m_prevCursorPos = p;
		}

		POINT cur; GetCursorPos(&cur); ScreenToClient(m_hwnd, &cur);
		int dx = cur.x - m_prevCursorPos.x;
		int dy = cur.y - m_prevCursorPos.y;
		m_prevCursorPos = cur;

		m_yaw += dx * m_mouseSensitivity; // X軸回転
		m_pitch += -dy * m_mouseSensitivity; // Y軸回転（マウス上下は反転）
		// ピッチをクランプして反転を防止
		const float pitchLimit = XM_PIDIV2 - 0.01f;
		if (m_pitch > pitchLimit) m_pitch = pitchLimit;
		if (m_pitch < -pitchLimit) m_pitch = -pitchLimit;
	}
	else if (m_rmbDown) {
		// 押下解除時: キャプチャ解除
		m_rmbDown = false;
		ReleaseCapture();
	}

	UpdateView();
}

void Camera::UpdateProjection() {
	m_proj = XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
}

void Camera::UpdateView() {
	XMVECTOR pos = XMLoadFloat3(&m_position);
	XMVECTOR lookDir = XMVectorSet(cosf(m_pitch) * sinf(m_yaw), sinf(m_pitch), cosf(m_pitch) * cosf(m_yaw), 0.0f);
	XMVECTOR target = pos + XMVector3Normalize(lookDir);
	XMVECTOR up = XMVectorSet(0,1,0,0);
	m_view = XMMatrixLookAtLH(pos, target, up);
}

}
