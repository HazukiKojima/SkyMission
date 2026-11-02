#pragma once
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
		size_t lodObjects[3]{};
		UINT oceanTriangles = 0;
	};

	inline unsigned SelectModelLod(float diameterPixels, unsigned count) {
		return std::min(diameterPixels < 70 ? 2u : diameterPixels < 200 ? 1u : 0u, count - 1);
	}
	inline DirectX::BoundingSphere WorldCullingSphere(
		const Bounds3& bounds, float radius, const DirectX::XMFLOAT4X4& world) {
		using namespace DirectX;
		const auto center = bounds.Center();
		BoundingSphere sphere;
		XMStoreFloat3(&sphere.Center, XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&world)));
		// sqrt(||A||1 * ||A||inf) bounds the spectral norm, including nonuniform scale and shear.
		float rowNorm = 0, columnNorm = 0;
		for (int i = 0; i < 3; ++i) {
			float row = 0, column = 0;
			for (int j = 0; j < 3; ++j) {
				row += std::abs(world.m[i][j]);
				column += std::abs(world.m[j][i]);
			}
			rowNorm = std::max(rowNorm, row);
			columnNorm = std::max(columnNorm, column);
		}
		sphere.Radius = radius * std::sqrt(rowNorm * columnNorm);
		return sphere;
	}
	inline std::vector<ModelDrawDecision> BuildModelDrawDecisions(
		const Scene& scene, DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection, UINT height) {
		using namespace DirectX;
		BoundingFrustum local, frustum;
		BoundingFrustum::CreateFromMatrix(local, projection);
		local.Transform(frustum, XMMatrixInverse(nullptr, view));
		std::vector<ModelDrawDecision> decisions;
		decisions.reserve(scene.GetObjects().size());
		for (const auto& object : scene.GetObjects()) {
			ModelDrawDecision decision;
			if (!object.model || !object.model->IsLoaded())
				decision.visible = false;
			else if (const auto& bounds = object.model->GetLocalBounds()) {
				const auto sphere =
					WorldCullingSphere(*bounds, object.model->GetCullingRadius(), object.worldTransform);
				decision.visible = frustum.Intersects(sphere);
				XMFLOAT3 centerView;
				XMStoreFloat3(&centerView, XMVector3TransformCoord(XMLoadFloat3(&sphere.Center), view));
				const float pixels = sphere.Radius * XMVectorGetY(projection.r[1]) * float(height) /
					std::max(centerView.z, 0.001f);
				decision.lod = SelectModelLod(pixels, object.model->GetLodCount());
			}
			decisions.push_back(decision);
		}
		return decisions;
	}
} // namespace Engine
