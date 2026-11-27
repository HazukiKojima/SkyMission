#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

namespace Game {
	// モデルの前方軸を速度方向へ向け、移動位置を反映する
	DirectX::XMFLOAT4X4 BuildProjectileTransform(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& modelForward);
} // namespace Game
