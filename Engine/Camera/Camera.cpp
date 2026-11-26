#include "Camera.h"
#include <cmath>

namespace Engine {
	void Camera::InterpolateFrom(const Camera& previous, float alpha) {
		using namespace DirectX;
		const auto from = XMQuaternionRotationMatrix(XMMatrixTranspose(previous.m_view));
		const auto to = XMQuaternionRotationMatrix(XMMatrixTranspose(m_view));
		const auto rotation = XMMatrixRotationQuaternion(XMQuaternionSlerp(from, to, alpha));
		XMFLOAT3 position, forward, up;
		XMStoreFloat3(&position, XMVectorLerp(XMLoadFloat3(&previous.m_position), XMLoadFloat3(&m_position), alpha));
		XMStoreFloat3(&forward, rotation.r[2]);
		XMStoreFloat3(&up, rotation.r[1]);
		LookTo(position, forward, up);
		SetVerticalFov(std::lerp(previous.m_fovY, m_fovY, alpha));
	}
	void Camera::Initialize(float fovY, float aspect, float nearZ, float farZ) {
		m_fovY = fovY;
		m_aspect = aspect;
		m_nearZ = nearZ;
		m_farZ = farZ;
		UpdateProjection();
	}

	void Camera::OnResize(UINT width, UINT height) {
		if (!height) return;

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

	void Camera::LookTo(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& direction,
		const DirectX::XMFLOAT3& up) {
		m_position = position;
		m_view = DirectX::XMMatrixLookToLH(DirectX::XMLoadFloat3(&position),
			DirectX::XMLoadFloat3(&direction), DirectX::XMLoadFloat3(&up));
	}
} // namespace Engine
