#pragma once
#include "Engine/Camera/Camera.h"
#include <DirectXMath.h>
#include "Engine/Input/RawInputState.h"

namespace Game {
	// 追従先の位置・向き・速度（ワールド単位/秒）
	struct AircraftCameraTarget {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT3 heading;
		DirectX::XMFLOAT3 forward;
		bool pitchInputActive;
		float speed;
		float maximumSpeed;
	};

	class AircraftCameraController {
	  public:
		AircraftCameraController() = default;
		~AircraftCameraController() = default;

		// 射影と初期視点を設定する（fovYはラジアン、nearZ・farZはワールド単位）
		void Initialize(float fovY = DirectX::XM_PIDIV4, float aspect = 16.0f / 9.0f, float nearZ = 0.1f,
			float farZ = 1000.0f);

		// 機体への追従かRaw Inputによる自由移動を更新する（deltaSecondsは秒）
		void Update(float deltaSeconds, const Engine::InputSnapshot& input = {});
		void SetFollowTarget(const AircraftCameraTarget& target);

		// レンダーターゲットのサイズが変わったときに通知する
		void OnResize(UINT width, UINT height);

		const Engine::Camera& GetCamera() const {
			return m_camera;
		}
		DirectX::XMMATRIX GetView() const {
			return m_camera.GetView();
		}
		DirectX::XMFLOAT3 GetPosition() const {
			return m_position;
		}

	  private:
		void UpdateView();
		void UpdateFollow(float deltaSeconds, const Engine::InputSnapshot& input);

		DirectX::XMFLOAT3 m_position = {0.0f, 10.0f, -10.0f};
		float m_yaw = 0.0f;			// ラジアン
		float m_pitch = -0.4f;		// ラジアン
		float m_moveSpeed = 100.0f; // ワールド単位/秒
		float m_mouseSensitivity =
			0.0025f; // マウス入力1カウント当たりのラジアン（絶対位置入力ではピクセル差分）
		bool m_following = false;
		DirectX::XMFLOAT3 m_followTarget = {0.0f, 0.0f, 0.0f};
		DirectX::XMFLOAT3 m_followHeading = {0.0f, 0.0f, -1.0f};
		DirectX::XMFLOAT3 m_followForward = {0.0f, 0.0f, -1.0f};
		bool m_followPitchInputActive = false;
		float m_followSpeed = 60.0f;
		float m_followMaximumSpeed = 800.0f;
		float m_followCameraYaw = 0.0f;
		float m_followCameraPitch = 0.0f;
		float m_lookYaw = 0, m_lookPitch = 0;
		float m_lookReturnRate = 6.0f;
		DirectX::XMFLOAT3 m_followLookAt = {0.0f, 0.0f, 0.0f};
		float m_followDistance = 250.0f;
		float m_maxFollowDistance = 450.0f;
		float m_followHeight = 120.0f;
		float m_followSmoothing = 1000.0f;
		float m_followDirectionSmoothing = 2.0f;

		Engine::Camera m_camera;
	};
} // namespace Game
