#include "Scene.h"

namespace Engine {
	void Scene::Initialize(HWND hwnd, float aspect, float nearZ, float farZ) {
		m_camera.Initialize(hwnd, DirectX::XM_PIDIV4, aspect, nearZ, farZ);
	}

	void Scene::Update(float deltaSeconds) {
		if (m_playerObjectIndex < m_objects.size()) {
			m_playerAircraft.Update(m_aircraftController.ReadInput(), deltaSeconds);
			const DirectX::XMFLOAT4X4 aircraftTransform = m_playerAircraft.GetWorldTransform();
			DirectX::XMStoreFloat4x4(
				&m_objects[m_playerObjectIndex].worldTransform,
				DirectX::XMMatrixScaling(m_playerAircraftVisualScale, m_playerAircraftVisualScale, m_playerAircraftVisualScale) *
				DirectX::XMLoadFloat4x4(&aircraftTransform));
			m_camera.SetFollowTarget(m_playerAircraft.GetPosition(), m_playerAircraft.GetForward());
		}
		m_camera.Update(deltaSeconds);
	}

	void Scene::OnResize(UINT width, UINT height) {
		m_camera.OnResize(width, height);
	}

	void Scene::AddObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& worldTransform) {
		if (model) {
			m_objects.emplace_back(std::move(model), worldTransform);
			if (m_playerObjectIndex == static_cast<std::size_t>(-1)) {
				m_playerObjectIndex = m_objects.size() - 1;
			}
		}
	}

	void Scene::AddObject(std::shared_ptr<Model> model) {
		if (model) {
			m_objects.emplace_back(std::move(model));
			if (m_playerObjectIndex == static_cast<std::size_t>(-1)) {
				m_playerObjectIndex = m_objects.size() - 1;
			}
		}
	}

	void Scene::RemoveObject(std::size_t index) {
		if (index < m_objects.size()) {
			m_objects.erase(m_objects.begin() + index);
			if (m_playerObjectIndex == index) {
				m_playerObjectIndex = static_cast<std::size_t>(-1);
			}
			else if (m_playerObjectIndex > index && m_playerObjectIndex < m_objects.size() + 1) {
				--m_playerObjectIndex;
			}
		}
	}
}
