#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace Game {
	// モデルの前方軸を速度方向へ向け、移動位置を反映する
	inline DirectX::XMFLOAT4X4 BuildProjectileTransform(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& modelForward) {
		using namespace DirectX;

		const auto from = XMLoadFloat3(&modelForward);
		const auto velocityVector = XMLoadFloat3(&velocity);
		const auto to = XMVectorGetX(XMVector3LengthSq(velocityVector)) > 0.0001f
			? XMVector3Normalize(velocityVector)
			: from;
		const float dot = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
		auto rotation = XMMatrixIdentity();
		if (dot < -0.9999f) {
			const auto reference =
				std::abs(modelForward.y) < 0.9f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
			rotation = XMMatrixRotationAxis(XMVector3Cross(from, reference), XM_PI);
		} else if (dot < 0.9999f)
			rotation = XMMatrixRotationAxis(XMVector3Cross(from, to), std::acos(dot));
		const auto& p = position;
		XMFLOAT4X4 transform;
		XMStoreFloat4x4(&transform, rotation * XMMatrixTranslation(p.x, p.y, p.z));
		return transform;
	}
} // namespace Game
