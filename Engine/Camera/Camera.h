#pragma once
#include "../EngineCommon.h"
#include <DirectXMath.h>

namespace Engine {
	// View/projection data only. Input and follow behavior belong to game controllers.
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
		float m_fovY = DirectX::XM_PIDIV4, m_aspect = 16.0f / 9.0f, m_nearZ = 0.1f, m_farZ = 1000.0f;
		DirectX::XMMATRIX m_view = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX m_proj = DirectX::XMMatrixIdentity();
	};
} // namespace Engine
