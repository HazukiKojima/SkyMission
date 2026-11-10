#pragma once
#include "CinematicController.h"
#include "Engine/Scene/Scene.h"
#include <array>

namespace Game {
	// 演出専用の編隊とカメラを所有し、戦闘シーンには触れない
	class CinematicScene {
	  public:
		void Initialize(const CinematicShotSettings& settings, std::shared_ptr<Engine::Model> model,
			float visualScale, const Engine::SceneEnvironment& environment, const DirectX::XMFLOAT3& origin,
			UINT width, UINT height, float farZ);
		void Update(const CinematicPose& pose);
		void OnResize(UINT width, UINT height);
		const Engine::Scene& GetScene() const {
			return m_scene;
		}

	  private:
		Engine::Scene m_scene;
		Engine::Camera m_camera;
		CinematicShotSettings m_settings;
		std::vector<Engine::ObjectId> m_aircraft;
		DirectX::XMFLOAT3 m_origin{}, m_modelCenter{};
		float m_visualScale = 1;
		float m_framingScale = 1;
		float m_farZ = 100000;
	};
} // namespace Game
