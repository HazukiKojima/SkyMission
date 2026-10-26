#pragma once
#include "../Camera/Camera.h"
#include "../Aircraft/AircraftAIController.h"
#include "../Aircraft/AircraftController.h"
#include "../Aircraft/PlayerAircraft.h"
#include "../Resources/Mesh/Model.h"
#include <DirectXMath.h>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace Engine {
	struct SceneObject {
		explicit SceneObject(std::shared_ptr<Model> model)
			: model(std::move(model)), worldTransform(
				1.0f, 0.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f, 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f) {}
		SceneObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& worldTransform)
			: model(std::move(model)), worldTransform(worldTransform) {}

		std::shared_ptr<Model> model;
		DirectX::XMFLOAT4X4 worldTransform;
	};

	struct SceneEnvironment {
		DirectX::XMFLOAT3 sunDirection = { 0.32f, 0.88f, -0.28f };
		float sunIntensity = 1.8f;
		DirectX::XMFLOAT3 sunColor = { 1.0f, 0.98f, 0.96f };
		float ambientIntensity = 0.32f;
		DirectX::XMFLOAT3 ambientColor = { 0.18f, 0.32f, 0.48f };
	};

	class Scene {
	public:
		void Initialize(HWND hwnd, float aspect, float nearZ, float farZ);
		void Update(float deltaSeconds);
		void OnResize(UINT width, UINT height);

		Camera& GetCamera() { return m_camera; }
		const Camera& GetCamera() const { return m_camera; }
		float GetPlayerAircraftSpeed() const { return m_playerAircraft.GetSpeed(); }
		SceneEnvironment& GetEnvironment() { return m_environment; }
		const SceneEnvironment& GetEnvironment() const { return m_environment; }
		void SetEnvironment(const SceneEnvironment& environment) { m_environment = environment; }
		void SetPlayerAircraftVisualScale(float scale) { m_playerAircraftVisualScale = scale; }
		void SetPlayerAircraftFlightParameters(const AircraftFlightParameters& parameters) {
			m_playerAircraft.SetFlightParameters(parameters);
		}
		std::vector<SceneObject>& GetObjects() { return m_objects; }
		const std::vector<SceneObject>& GetObjects() const { return m_objects; }
		void AddPlayerObject(std::shared_ptr<Model> model);
		void AddEnemyAircraft(
			std::shared_ptr<Model> model,
			const DirectX::XMFLOAT3& position,
			float visualScale,
			const AircraftFlightParameters& flightParameters,
			const AircraftAIParameters& aiParameters);
		void AddObject(std::shared_ptr<Model> model);
		void AddObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& worldTransform);
		void RemoveObject(std::size_t index);

	private:
		struct EnemyAircraft {
			AircraftFlightModel flightModel;
			AircraftAIController controller;
			AircraftAIParameters aiParameters;
			std::size_t objectIndex;
			float visualScale;
		};

		Camera m_camera;
		SceneEnvironment m_environment;
		std::vector<SceneObject> m_objects;
		std::vector<EnemyAircraft> m_enemyAircraft;
		AircraftFlightModel m_playerAircraft;
		AircraftController m_aircraftController;
		std::size_t m_playerObjectIndex = static_cast<std::size_t>(-1);
		float m_playerAircraftVisualScale = 1.0f;
	};
}
