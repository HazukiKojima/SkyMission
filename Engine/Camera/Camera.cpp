#include "Camera.h"
namespace Engine {
	void Camera::Initialize(float fovY, float aspect, float nearZ, float farZ) {
		m_fovY = fovY;
		m_aspect = aspect;
		m_nearZ = nearZ;
		m_farZ = farZ;
		UpdateProjection();
	}

	void Camera::OnResize(UINT width, UINT height) {
		if (!height)
			return;
		m_aspect = static_cast<float>(width) / height;
		UpdateProjection();
	}

	void Camera::LookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target) {
		m_position = position;
		// 左手座標系・上方向+Yで、ゲーム側が指定した位置と注視点を使う
		m_view = DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&position), DirectX::XMLoadFloat3(&target),
			DirectX::XMVectorSet(0, 1, 0, 0));
	}

	void Camera::UpdateProjection() {
		m_proj = DirectX::XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, m_farZ);
	}
} // namespace Engine
