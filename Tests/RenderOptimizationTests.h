#pragma once
#include "Engine/Renderer/Device/RenderVisibility.h"

void TestRenderOptimizationMath() {
	using namespace DirectX;
	Require(OceanGeometry::GridResolution == 257, "Ocean LOD grid budget changed");
	const float size = 10000000;
	float previous = -size;
	for (int i = 0; i < OceanGeometry::GridResolution; ++i) {
		const float value = OceanGeometry::GridCoordinate(i, size);
		Require(std::isfinite(value) && value > previous, "Ocean distance grid is not strictly increasing");
		previous = value;
	}
	Require(std::abs(OceanGeometry::GridCoordinate(0, size) + size / 2) < 1 &&
			std::abs(OceanGeometry::GridCoordinate(256, size) - size / 2) < 1 &&
			OceanGeometry::GridCoordinate(128, size) == 0,
		"Ocean grid lost its center or horizon extent");
	Require(OceanGeometry::GridCoordinate(129, size) <
			(OceanGeometry::GridCoordinate(256, size) - OceanGeometry::GridCoordinate(255, size)) / 20,
		"Ocean must retain much greater density around the camera");
	const Bounds3 bounds{{-1, -1, -1}, {1, 1, 1}};
	XMFLOAT4X4 world;
	XMStoreFloat4x4(
		&world, XMMatrixScaling(2, 3, 4) * XMMatrixRotationY(0.6f) * XMMatrixTranslation(10, 2, 30));
	const auto sphere = WorldCullingSphere(bounds, std::sqrt(3.0f), world);
	for (float x : {-1.0f, 1.0f})
		for (float y : {-1.0f, 1.0f})
			for (float z : {-1.0f, 1.0f}) {
				const auto point = XMVector3TransformCoord(XMVectorSet(x, y, z, 1), XMLoadFloat4x4(&world));
				const float distance = XMVectorGetX(XMVector3Length(point - XMLoadFloat3(&sphere.Center)));
				Require(
					distance <= sphere.Radius + 0.001f, "World culling sphere clips scaled/rotated bounds");
			}
	Require(SelectModelLod(300, 3) == 0 && SelectModelLod(120, 3) == 1 && SelectModelLod(50, 3) == 2 &&
			SelectModelLod(10, 1) == 0,
		"Projected-size LOD selection is broken");
	std::cout << "PASS: continuous ocean distance LOD, conservative transformed bounds and screen-size LOD\n";
}

void TestRenderVisibility(const std::shared_ptr<Model>& model) {
	using namespace DirectX;
	Scene scene;
	for (const auto position : {XMFLOAT3{0, 0, 50}, XMFLOAT3{0, 0, -50}, XMFLOAT3{300, 0, 50}}) {
		XMFLOAT4X4 matrix;
		XMStoreFloat4x4(&matrix, XMMatrixTranslation(position.x, position.y, position.z));
		scene.AddObject(model, matrix);
	}
	const auto projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, 16.0f / 9, 1, 1000);
	const auto decisions = BuildModelDrawDecisions(scene, XMMatrixIdentity(), projection, 720);
	Require(decisions.size() == 3 && decisions[0].visible && !decisions[1].visible && !decisions[2].visible,
		"Visible/behind-camera/outside-frustum objects were not classified correctly");
	std::cout << "PASS: visible, behind-camera and offscreen models\n";
}
