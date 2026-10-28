#pragma once
#include "../EngineCommon.h"
#include <DirectXMath.h>
#include "../Input/RawInputState.h"

namespace Engine {
	// UEエディタ風のフライカメラ: WASDで移動、右ボタン押下でマウス視点移動。
	// フライカメラは Raw Input のフレームスナップショットを使用する。
	class Camera {
	  public:
		Camera() = default;
		~Camera() = default;

		// 射影・ビュー行列を初期化（hwnd は既存呼び出し元との互換性のため保持）。
		void Initialize(HWND hwnd, float fovY = DirectX::XM_PIDIV4, float aspect = 16.0f / 9.0f, float nearZ = 0.1f,
						float farZ = 1000.0f);

		// 毎フレーム、経過秒を渡して呼び出す
		void Update(float deltaSeconds, const InputSnapshot& input = {});
		void SetFollowTarget(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& heading,
							 const DirectX::XMFLOAT3& forward, bool pitchInputActive, float speed, float maximumSpeed);

		// レンダーターゲットのサイズが変わったときに通知する
		void OnResize(UINT width, UINT height);

		// アクセサ
		DirectX::XMMATRIX GetView() const {
			return m_view;
		}
		DirectX::XMMATRIX GetProjection() const {
			return m_proj;
		}
		DirectX::XMMATRIX GetProjection(float farZ) const {
			return farZ > m_nearZ ? DirectX::XMMatrixPerspectiveFovLH(m_fovY, m_aspect, m_nearZ, farZ) : m_proj;
		}
		DirectX::XMMATRIX GetViewProjection() const {
			return m_view * m_proj;
		}
		DirectX::XMFLOAT3 GetPosition() const {
			return m_position;
		}

		void SetPosition(const DirectX::XMFLOAT3& pos) {
			m_position = pos;
		}
		void SetFarPlane(float farZ) {
			if (farZ > m_nearZ && farZ != m_farZ) {
				m_farZ = farZ;
				UpdateProjection();
			}
		}

	  private:
		void UpdateProjection();
		void UpdateView();
		void UpdateFollow(float deltaSeconds);

		DirectX::XMFLOAT3 m_position = {0.0f, 10.0f, -10.0f};
		float m_yaw = 0.0f;					// radians
		float m_pitch = -0.4f;				// radians
		float m_moveSpeed = 100.0f;			// units per second
		float m_mouseSensitivity = 0.0025f; // radians per pixel
		bool m_following = false;
		DirectX::XMFLOAT3 m_followTarget = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 m_followHeading = {0.0f, 0.0f, -1.0f};
		DirectX::XMFLOAT3 m_followForward = {0.0f, 0.0f, -1.0f};
		bool m_followPitchInputActive = false;
		float m_followSpeed = 60.0f;
		float m_followMaximumSpeed = 800.0f;
		float m_followCameraYaw = 0.0f;
		float m_followCameraPitch = 0.0f;
		DirectX::XMFLOAT3 m_followLookAt = {0.0f, 0.0f, 0.0f};
		float m_followDistance = 250.0f;
		float m_maxFollowDistance = 450.0f;
		float m_followHeight = 120.0f;
		float m_followSmoothing = 1000.0f;
		float m_followDirectionSmoothing = 2.0f;

		float m_fovY = DirectX::XM_PIDIV4;
		float m_aspect = 16.0f / 9.0f;
		float m_nearZ = 0.1f;
		float m_farZ = 1000.0f;

		DirectX::XMMATRIX m_view;
		DirectX::XMMATRIX m_proj;
	};
} // namespace Engine
