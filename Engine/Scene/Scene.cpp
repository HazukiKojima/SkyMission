#include "Scene.h"
#include <algorithm>

namespace Engine {
	void Scene::Initialize(HWND hwnd, float aspect, float nearZ, float farZ) {
		m_camera.Initialize(hwnd, DirectX::XM_PIDIV4, aspect, nearZ, farZ);
	}

	void Scene::Update(float deltaSeconds) {
		if (m_playerObjectIndex < m_objects.size()) {
			const AircraftInput input = m_aircraftController.ReadInput();
			m_playerAircraft.Update(input, deltaSeconds);
			const DirectX::XMFLOAT4X4 aircraftTransform = m_playerAircraft.GetWorldTransform();
			DirectX::XMStoreFloat4x4(
				&m_objects[m_playerObjectIndex].worldTransform,
				DirectX::XMMatrixScaling(m_playerAircraftVisualScale, m_playerAircraftVisualScale, m_playerAircraftVisualScale) *
				DirectX::XMLoadFloat4x4(&aircraftTransform));
			m_camera.SetFollowTarget(
				m_playerAircraft.GetPosition(),
				m_playerAircraft.GetHeading(),
				m_playerAircraft.GetForward(),
				input.pitch > 0.01f || input.pitch < -0.01f);
		}
		for (EnemyAircraft& enemy : m_enemyAircraft) {
			if (enemy.objectIndex >= m_objects.size()) {
				continue;
			}
			const AircraftInput input = enemy.controller.ReadInput(
				enemy.flightModel, m_playerAircraft.GetMaximumSpeed(), deltaSeconds, enemy.aiParameters);
			enemy.flightModel.Update(input, deltaSeconds);
			const DirectX::XMFLOAT4X4 enemyTransform = enemy.flightModel.GetWorldTransform();
			DirectX::XMStoreFloat4x4(
				&m_objects[enemy.objectIndex].worldTransform,
				DirectX::XMMatrixScaling(enemy.visualScale, enemy.visualScale, enemy.visualScale) *
				DirectX::XMLoadFloat4x4(&enemyTransform));
		}
		m_camera.Update(deltaSeconds);
	}

	void Scene::AddEnemyAircraft(
		std::shared_ptr<Model> model,
		const DirectX::XMFLOAT3& position,
		float visualScale,
		const AircraftFlightParameters& flightParameters,
		const AircraftAIParameters& aiParameters) {
		if (model) {
			EnemyAircraft enemy;
			enemy.flightModel.SetFlightParameters(flightParameters);
			enemy.flightModel.SetPosition(position);
			enemy.aiParameters = aiParameters;
			enemy.objectIndex = m_objects.size();
			enemy.visualScale = visualScale;
			m_objects.emplace_back(std::move(model));
			m_enemyAircraft.push_back(std::move(enemy));
		}
	}

	void Scene::OnResize(UINT width, UINT height) {
		m_camera.OnResize(width, height);
	}

	void Scene::AddPlayerObject(std::shared_ptr<Model> model) {
		if (model) {
			m_objects.emplace_back(std::move(model));
			m_playerObjectIndex = m_objects.size() - 1;
		}
	}

	void Scene::AddObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& worldTransform) {
		if (model) {
			m_objects.emplace_back(std::move(model), worldTransform);
		}
	}

	void Scene::AddObject(std::shared_ptr<Model> model) {
		if (model) {
			m_objects.emplace_back(std::move(model));
		}
	}

	void Scene::RemoveObject(std::size_t index) {
		if (index < m_objects.size()) {
			m_objects.erase(m_objects.begin() + index);
			m_enemyAircraft.erase(
				std::remove_if(m_enemyAircraft.begin(), m_enemyAircraft.end(), [index](const EnemyAircraft& enemy) {
					return enemy.objectIndex == index;
				}),
				m_enemyAircraft.end());
			for (EnemyAircraft& enemy : m_enemyAircraft) {
				if (enemy.objectIndex > index) {
					--enemy.objectIndex;
				}
			}
			if (m_playerObjectIndex == index) {
				m_playerObjectIndex = static_cast<std::size_t>(-1);
			}
			else if (m_playerObjectIndex > index && m_playerObjectIndex < m_objects.size() + 1) {
				--m_playerObjectIndex;
			}
		}
	}
}
