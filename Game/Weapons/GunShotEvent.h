#pragma once
#include <DirectXMath.h>
#include <cstddef>
#include <cstdint>

namespace Game {
	// フレーム内で発生した発射を、弾の生存期間とは独立して通知する
	struct GunShotEvent {
		std::size_t muzzleIndex = 0;
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 aircraftVelocity{};
		DirectX::XMFLOAT3 direction{0, 0, -1};
		float age = 0;
		std::uint64_t sequence = 0;
	};

	struct GunTracerSegment {
		DirectX::XMFLOAT3 head{};
		DirectX::XMFLOAT3 tail{};
	};
} // namespace Game
