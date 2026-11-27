#include "RenderVisibility.h"

namespace Engine {
	unsigned SelectModelLod(float diameterPixels, unsigned count) {
		constexpr float kLowDetailThresholdPixels = 70;
		constexpr float kMediumDetailThresholdPixels = 200;
		auto lod = ModelDetailLevel::Full;
		if (diameterPixels < kLowDetailThresholdPixels) {
			lod = ModelDetailLevel::Low;
		} else if (diameterPixels < kMediumDetailThresholdPixels) {
			lod = ModelDetailLevel::Medium;
		}

		// モデルが持つLOD数の範囲へ収める
		return std::min(static_cast<unsigned>(lod), count - 1);
	}

	DirectX::BoundingSphere WorldCullingSphere(
		const Bounds3& bounds, float radius, const DirectX::XMFLOAT4X4& world) {
		using namespace DirectX;
		const auto center = bounds.Center();
		BoundingSphere sphere;
		XMStoreFloat3(&sphere.Center, XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&world)));

		// 非一様スケールやせん断でも球が収まるよう、最大伸長率の上限を求める
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

	std::vector<ModelDrawDecision> BuildModelDrawDecisions(
		const Scene& scene, DirectX::FXMMATRIX view, DirectX::CXMMATRIX projection, UINT height) {
		using namespace DirectX;
		constexpr float kMinimumLodProjectionDepth = 0.001f;

		// 視錐台をワールド空間へ変換する
		BoundingFrustum local, frustum;
		BoundingFrustum::CreateFromMatrix(local, projection);
		local.Transform(frustum, XMMatrixInverse(nullptr, view));

		// シーン順に可視性とLODを判定する
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

				// 投影サイズからLODを選ぶ
				XMFLOAT3 centerView;
				XMStoreFloat3(&centerView, XMVector3TransformCoord(XMLoadFloat3(&sphere.Center), view));
				const float pixels = sphere.Radius * XMVectorGetY(projection.r[1]) * float(height) /
					std::max(centerView.z, kMinimumLodProjectionDepth);
				decision.lod = SelectModelLod(pixels, object.model->GetLodCount());
			}

			decisions.push_back(decision);
		}
		return decisions;
	}
} // namespace Engine
