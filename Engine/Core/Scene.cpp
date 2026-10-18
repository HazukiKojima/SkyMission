#include "Scene.h"

namespace Engine {
	void Scene::Initialize(HWND hwnd, float aspect, float nearZ, float farZ) {
		m_camera.Initialize(hwnd, DirectX::XM_PIDIV4, aspect, nearZ, farZ);
	}

	void Scene::Update(float deltaSeconds) {
		m_camera.Update(deltaSeconds);
	}

	void Scene::OnResize(UINT width, UINT height) {
		m_camera.OnResize(width, height);
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
		}
	}
}
