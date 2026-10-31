#include "Scene.h"
#include <algorithm>
namespace Engine {

	SceneObject* Scene::FindObject(ObjectId id) {
		const auto found = std::find_if(m_objects.begin(), m_objects.end(), [id](const SceneObject& object) {
			return object.id == id;
		});
		return found == m_objects.end() ? nullptr : &*found;
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
		std::erase_if(m_objects, [id](const SceneObject& object) {
			return object.id == id;
		});
	}
} // namespace Engine
