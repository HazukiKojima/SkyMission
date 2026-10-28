#pragma once
#include "../Camera/Camera.h"
#include "../Aircraft/AircraftAIController.h"
#include "../Aircraft/AircraftFlightModel.h"
#include "../Resources/Mesh/Model.h"
#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace Engine {
	using ObjectId = std::uint64_t;
	inline constexpr ObjectId InvalidObjectId = 0;

	struct SceneObject {
		explicit SceneObject(std::shared_ptr<Model> model)
			: model(std::move(model)), worldTransform(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
													  0.0f, 0.0f, 0.0f, 0.0f, 1.0f) {
		}
		SceneObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& worldTransform)
			: model(std::move(model)), worldTransform(worldTransform) {
		}

		ObjectId id = InvalidObjectId;
		float propellerAngle = 0.0f;
		std::shared_ptr<Model> model;
		DirectX::XMFLOAT4X4 worldTransform;
	};

	struct SceneEnvironment {
		DirectX::XMFLOAT3 sunDirection = {0.32f, 0.88f, -0.28f};
		float sunIntensity = 1.8f;
		DirectX::XMFLOAT3 sunColor = {1.0f, 0.98f, 0.96f};
		float ambientIntensity = 0.32f;
		DirectX::XMFLOAT3 ambientColor = {0.18f, 0.32f, 0.48f};
	};

	class Scene {
	  public:
		void Initialize(HWND hwnd, float aspect, float nearZ, float farZ);
		void Update(const AircraftInput& input, float deltaSeconds);
		void OnResize(UINT width, UINT height);

		const Camera& GetCamera() const {
			return m_camera;
		}
		float GetPlayerAircraftSpeed() const {
			return m_playerAircraft.GetSpeed();
		}
		const SceneEnvironment& GetEnvironment() const {
			return m_environment;
		}
		void SetEnvironment(const SceneEnvironment& environment) {
			m_environment = environment;
		}
		void SetPlayerAircraftVisualScale(float scale) {
			m_playerAircraftVisualScale = scale;
		}
		void SetPlayerAircraftFlightParameters(const AircraftFlightParameters& parameters) {
			m_playerAircraft.SetFlightParameters(parameters);
		}
		const std::vector<SceneObject>& GetObjects() const {
			return m_objects;
		}
		ObjectId AddPlayerObject(std::shared_ptr<Model> model);
		ObjectId AddEnemyAircraft(std::shared_ptr<Model> model, const DirectX::XMFLOAT3& position, float visualScale,
								  const AircraftFlightParameters& flightParameters,
								  const AircraftAIParameters& aiParameters);
		ObjectId AddObject(std::shared_ptr<Model> model);
		ObjectId AddObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& worldTransform);
		void RemoveObject(ObjectId id);

	  private:
		struct EnemyAircraft {
			AircraftFlightModel flightModel;
			AircraftAIController controller;
			AircraftAIParameters aiParameters;
			ObjectId objectId = InvalidObjectId;
			float visualScale;
		};

		Camera m_camera;
		SceneEnvironment m_environment;
		std::vector<SceneObject> m_objects;
		std::vector<EnemyAircraft> m_enemyAircraft;
		AircraftFlightModel m_playerAircraft;
		ObjectId m_playerObjectId = InvalidObjectId;
		ObjectId m_nextObjectId = 1;
		SceneObject* FindObject(ObjectId id);
		float m_playerAircraftVisualScale = 1.0f;
	};
} // namespace Engine
