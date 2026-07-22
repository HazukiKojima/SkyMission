#pragma once
#include "../EngineCommon.h"
#include <DirectXMath.h>

namespace Engine {
	// UEエディタ風のフライカメラ: WASDで移動、右ボタン押下でマウス視点移動。
	// ベストプラクティス: 入力ポーリングをカメラ内部に持たせ、Applicationをシンプルに保つ。
	class Camera {
	public:
		Camera() = default;
		~Camera() = default;

		// ウィンドウハンドルで初期化（マウスキャプチャや射影パラメータ用）
		void Initialize(HWND hwnd, float fovY = DirectX::XM_PIDIV4, float aspect = 16.0f/9.0f, float nearZ = 0.1f, float farZ = 1000.0f);

		// 毎フレーム、経過秒を渡して呼び出す
		void Update(float deltaSeconds);

		// レンダーターゲットのサイズが変わったときに通知する
		void OnResize(UINT width, UINT height);

		// アクセサ
		DirectX::XMMATRIX GetView() const { return m_view; }
		DirectX::XMMATRIX GetProjection() const { return m_proj; }
		DirectX::XMMATRIX GetViewProjection() const { return m_view * m_proj; }
		DirectX::XMFLOAT3 GetPosition() const { return m_position; }

		void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }

	private:
		void UpdateProjection();
		void UpdateView();

		HWND m_hwnd = nullptr;
		DirectX::XMFLOAT3 m_position = { 0.0f, 10.0f, -10.0f };
		float m_yaw = 0.0f;   // radians
		float m_pitch = -0.4f; // radians
		float m_moveSpeed = 20.0f; // units per second
		float m_mouseSensitivity = 0.0025f; // radians per pixel
		bool m_rmbDown = false;
		POINT m_prevCursorPos = { 0, 0 };

		float m_fovY = DirectX::XM_PIDIV4;
		float m_aspect = 16.0f/9.0f;
		float m_nearZ = 0.1f;
		float m_farZ = 1000.0f;

		DirectX::XMMATRIX m_view;
		DirectX::XMMATRIX m_proj;
	};
}
