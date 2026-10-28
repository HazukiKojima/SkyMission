#include "Scene.h"
#include <algorithm>
#include <cmath>

namespace Engine {
	namespace {
		// Visual angular speed per world-unit/second; each aircraft owns its phase.
		constexpr float kPropellerAngularSpeedPerSpeed = 0.06f;
		void UpdatePropeller(SceneObject& object, float speed, float dt) {
			object.propellerAngle =
				std::fmod(object.propellerAngle + speed * kPropellerAngularSpeedPerSpeed * dt, DirectX::XM_2PI);
		}
	} // namespace

	SceneObject* Scene::FindObject(ObjectId id) {
		const auto found = std::find_if(m_objects.begin(), m_objects.end(),
										[id](const SceneObject& object) { return object.id == id; });
		return found == m_objects.end() ? nullptr : &*found;
	}

	void Scene::Initialize(HWND hwnd, float aspect, float nearZ, float farZ) {
		m_camera.Initialize(hwnd, DirectX::XM_PIDIV4, aspect, nearZ, farZ);
	}

	void Scene::Update(const AircraftInput& input, float deltaSeconds) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
			return;
		const float animationSeconds = std::min(deltaSeconds, 0.25f); // Match the flight integrator's catch-up limit.
		if (SceneObject* player = FindObject(m_playerObjectId)) {
			m_playerAircraft.Update(input, deltaSeconds);
			UpdatePropeller(*player, m_playerAircraft.GetSpeed(), animationSeconds);
			const DirectX::XMFLOAT4X4 aircraftTransform = m_playerAircraft.GetWorldTransform();
			DirectX::XMStoreFloat4x4(&player->worldTransform,
									 DirectX::XMMatrixScaling(m_playerAircraftVisualScale, m_playerAircraftVisualScale,
															  m_playerAircraftVisualScale) *
										 DirectX::XMLoadFloat4x4(&aircraftTransform));
			m_camera.SetFollowTarget(m_playerAircraft.GetPosition(), m_playerAircraft.GetHeading(),
									 m_playerAircraft.GetForward(), input.pitch > 0.01f || input.pitch < -0.01f,
									 m_playerAircraft.GetSpeed(), m_playerAircraft.GetMaximumSpeed());
		}
		for (EnemyAircraft& enemy : m_enemyAircraft) {
			SceneObject* object = FindObject(enemy.objectId);
			if (!object) {
				continue;
			}
			const AircraftInput enemyInput = enemy.controller.ReadInput(
				enemy.flightModel, m_playerAircraft.GetMaximumSpeed(), deltaSeconds, enemy.aiParameters);
			enemy.flightModel.Update(enemyInput, deltaSeconds);
			UpdatePropeller(*object, enemy.flightModel.GetSpeed(), animationSeconds);
			const DirectX::XMFLOAT4X4 enemyTransform = enemy.flightModel.GetWorldTransform();
			DirectX::XMStoreFloat4x4(&object->worldTransform,
									 DirectX::XMMatrixScaling(enemy.visualScale, enemy.visualScale, enemy.visualScale) *
										 DirectX::XMLoadFloat4x4(&enemyTransform));
		}
		m_camera.Update(deltaSeconds);
	}

	ObjectId Scene::AddEnemyAircraft(std::shared_ptr<Model> model, const DirectX::XMFLOAT3& position, float visualScale,
									 const AircraftFlightParameters& flightParameters,
									 const AircraftAIParameters& aiParameters) {
		if (model) {
			EnemyAircraft enemy;
			enemy.flightModel.SetFlightParameters(flightParameters);
			enemy.flightModel.SetPosition(position);
			enemy.aiParameters = aiParameters;
			enemy.objectId = m_nextObjectId++;
			enemy.visualScale = visualScale;
			m_objects.emplace_back(std::move(model));
			m_objects.back().id = enemy.objectId;
			const ObjectId id = enemy.objectId;
			m_enemyAircraft.push_back(std::move(enemy));
			return id;
		}
		return InvalidObjectId;
	}

	void Scene::OnResize(UINT width, UINT height) {
		m_camera.OnResize(width, height);
	}

	ObjectId Scene::AddPlayerObject(std::shared_ptr<Model> model) {
		m_playerObjectId = AddObject(std::move(model));
		return m_playerObjectId;
	}

	ObjectId Scene::AddObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& transform) {
		if (!model)
			return InvalidObjectId;
		m_objects.emplace_back(std::move(model), transform);
		m_objects.back().id = m_nextObjectId++;
		return m_objects.back().id;
	}

	ObjectId Scene::AddObject(std::shared_ptr<Model> model) {
		if (!model)
			return InvalidObjectId;
		m_objects.emplace_back(std::move(model));
		m_objects.back().id = m_nextObjectId++;
		return m_objects.back().id;
	}

	void Scene::RemoveObject(ObjectId id) {
		std::erase_if(m_objects, [id](const SceneObject& object) { return object.id == id; });
		std::erase_if(m_enemyAircraft, [id](const EnemyAircraft& enemy) { return enemy.objectId == id; });
		if (m_playerObjectId == id)
			m_playerObjectId = InvalidObjectId;
	}
} // namespace Engine
