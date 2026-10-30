#pragma once
#include <DirectXMath.h>

namespace Engine {
	struct Bounds3 {
		DirectX::XMFLOAT3 minimum{}, maximum{};
		DirectX::XMFLOAT3 Center() const {
			return {(minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f,
				(minimum.z + maximum.z) * 0.5f};
		}
	};
}
