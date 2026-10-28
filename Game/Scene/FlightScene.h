#pragma once
#include "Engine/Scene/Scene.h"
#include "Game/Aircraft/AircraftAIController.h"
#include "Game/Camera/AircraftCameraController.h"

namespace Game {
	class FlightScene {
	  public:
		void Initialize(float aspect, float nearZ, float farZ);
		void Update(
			const AircraftInput& input, float deltaSeconds, const Engine::InputSnapshot& cameraInput = {});
		void OnResize(UINT width, UINT height);
		const Engine::Scene& GetRenderScene() const {
			return m_scene;
		}
		const std::vector<Engine::SceneObject>& GetObjects() const {
			return m_scene.GetObjects();
		}
		void SetEnvironment(const Engine::SceneEnvironment& environment) {
			m_scene.SetEnvironment(environment);
		}
		void SetPlayerAircraftVisualScale(float scale) {
			m_playerVisualScale = scale;
		}
		void SetPlayerAircraftFlightParameters(const AircraftFlightParameters& parameters) {
			m_player.SetFlightParameters(parameters);
		}
		Engine::ObjectId AddPlayerObject(std::shared_ptr<Engine::Model> model);
		Engine::ObjectId AddEnemyAircraft(std::shared_ptr<Engine::Model> model,
			const DirectX::XMFLOAT3& position, float scale, const AircraftFlightParameters& flight,
			const AircraftAIParameters& ai);
		Engine::ObjectId AddObject(std::shared_ptr<Engine::Model> model) {
			return m_scene.AddObject(std::move(model));
		}
		void RemoveObject(Engine::ObjectId id);

	  private:
		struct EnemyAircraft {
			AircraftFlightModel flightModel;
			AircraftAIController controller;
			AircraftAIParameters aiParameters;
			Engine::ObjectId objectId = Engine::InvalidObjectId;
			float visualScale = 1, propellerAngle = 0;
		};
		void UpdatePlayer(const AircraftInput& input, float dt);
		void UpdateEnemies(float dt);
		void UpdateCamera(const AircraftInput& input, float dt, const Engine::InputSnapshot& cameraInput);
		void ApplyRenderStates();
		Engine::Scene m_scene;
		AircraftCameraController m_cameraController;
		AircraftFlightModel m_player;
		Engine::ObjectId m_playerId = Engine::InvalidObjectId;
		float m_playerVisualScale = 1, m_playerPropellerAngle = 0;
		std::vector<EnemyAircraft> m_enemies;
	};
} // namespace Game
