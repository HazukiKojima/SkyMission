#pragma once
#include "Engine/Resources/Mesh/ModelLoadOptions.h"
#include "Engine/Scene/Scene.h"
#include <DirectXCollision.h>
#include <algorithm>

namespace Engine {
	struct ModelDrawDecision {
		bool visible = true;
		unsigned lod = 0;
	};

	struct RenderStatistics {
		size_t visibleObjects = 0, culledObjects = 0, modelDrawCalls = 0, modelTriangles = 0;
		size_t lodObjects[kModelLodCount]{};
		UINT oceanTriangles = 0;
	};

	unsigned SelectModelLod(float diameterPixels, unsigned count);

	DirectX::BoundingSphere WorldCullingSphere(
		const Bounds3& bounds, float radius, const DirectX::XMFLOAT4X4& world);

	std::vector<ModelDrawDecision> BuildModelDrawDecisions(
		const Scene& scene, DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection, UINT height);
} // namespace Engine
