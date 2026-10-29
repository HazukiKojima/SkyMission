#pragma once
#include "../Camera/Camera.h"
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
			: model(std::move(model)), worldTransform(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
										   0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f) {
		}
		SceneObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& worldTransform)
			: model(std::move(model)), worldTransform(worldTransform) {
		}

		ObjectId id = InvalidObjectId;
		float nodeRotationRadians = 0.0f;
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
		const Camera& GetCamera() const {
			return m_camera;
		}
		void SetCamera(const Camera& camera) {
			m_camera = camera;
		}
		const SceneEnvironment& GetEnvironment() const {
			return m_environment;
		}
		void SetEnvironment(const SceneEnvironment& environment) {
			m_environment = environment;
		}
		const std::vector<SceneObject>& GetObjects() const {
			return m_objects;
		}
		// ポインタは集合の変更まで有効、継続参照にはIDを使う
		SceneObject* FindObject(ObjectId id);
		ObjectId AddObject(std::shared_ptr<Model> model);
		ObjectId AddObject(std::shared_ptr<Model> model, const DirectX::XMFLOAT4X4& transform);
		void RemoveObject(ObjectId id);

	  private:
		Camera m_camera;
		SceneEnvironment m_environment;
		std::vector<SceneObject> m_objects;
		ObjectId m_nextObjectId = 1;
	};
} // namespace Engine
