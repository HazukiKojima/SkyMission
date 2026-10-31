#pragma once
#include "../EngineCommon.h"
#include <DirectXMath.h>

namespace Engine {
	// ビュー・射影行列を管理し、入力・追従制御はゲーム側で行う
	class Camera {
	  public:
		void Initialize(float fovY = DirectX::XM_PIDIV4, float aspect = 16.0f / 9.0f, float nearZ = 0.1f,
			float farZ = 1000.0f);
		void OnResize(UINT width, UINT height);
		void LookAt(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& target);
		DirectX::XMMATRIX GetView() const {
			return m_view;
		}
		DirectX::XMMATRIX GetProjection() const {
			return m_proj;
		}
		DirectX::XMMATRIX GetProjection(float farZ) const {
			return farZ > m_nearZ ? DirectX::XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, farZ)
								  : m_proj;
		}
		DirectX::XMFLOAT3 GetPosition() const {
			return m_position;
		}

	  private:
		void UpdateProjection();
		DirectX::XMFLOAT3 m_position{0, 10, -10};
		float m_fovY = DirectX::XM_PIDIV4;
		float m_aspect = 16.0f / 9.0f;
		float m_nearZ = 0.1f;
		float m_farZ = 1000.0f;
		DirectX::XMMATRIX m_view = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX m_proj = DirectX::XMMatrixIdentity();
	};
} // namespace Engine
