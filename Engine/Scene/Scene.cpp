#include "Scene.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Engine {
	void Scene::InterpolateFrom(const Scene& previous, float alpha) {
		using namespace DirectX;
		m_camera.InterpolateFrom(previous.m_camera, alpha);

		// オブジェクトは追加順にIDを割り当て、削除しても生存オブジェクトの順序を保つ。
		auto from = previous.m_objects.begin();
		for (auto& object : m_objects) {
			while (from != previous.m_objects.end() && from->id < object.id) ++from;
			if (from == previous.m_objects.end()) break;
			if (from->id != object.id || from->model != object.model) continue;
			const float turn = std::remainder(object.nodeRotationRadians - from->nodeRotationRadians, XM_2PI);
			object.nodeRotationRadians = from->nodeRotationRadians + turn * alpha;
			if (std::memcmp(&from->worldTransform, &object.worldTransform, sizeof(XMFLOAT4X4)) == 0) continue;
			const auto before = XMLoadFloat4x4(&from->worldTransform);
			const auto after = XMLoadFloat4x4(&object.worldTransform);
			XMVECTOR oldScale, oldRotation, oldPosition, scale, rotation, position;
			if (!XMMatrixDecompose(&oldScale, &oldRotation, &oldPosition, before) ||
				!XMMatrixDecompose(&scale, &rotation, &position, after))
				continue;
			auto world = XMMatrixScalingFromVector(XMVectorLerp(oldScale, scale, alpha)) *
				XMMatrixRotationQuaternion(XMQuaternionSlerp(oldRotation, rotation, alpha));
			world.r[3] = XMVectorLerp(oldPosition, position, alpha);
			XMStoreFloat4x4(&object.worldTransform, world);
		}
	}

	SceneObject* Scene::FindObject(ObjectId id) {
		const auto found = std::find_if(m_objects.begin(), m_objects.end(), [id](const SceneObject& object) {
			return object.id == id;
		});
		return found == m_objects.end() ? nullptr : &*found;
	}

	ObjectId Scene::AddObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& transform) {
		if (!model) return InvalidObjectId;

		m_objects.emplace_back(std::move(model), transform);
		m_objects.back().id = m_nextObjectId++;
		return m_objects.back().id;
	}

	ObjectId Scene::AddObject(std::shared_ptr<Model> model) {
		if (!model) return InvalidObjectId;

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
