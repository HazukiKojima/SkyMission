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
		float age = 0;				// 発射からフレーム末までの経過秒数。演出の開始時刻補正に使う
		std::uint64_t sequence = 0; // 発射ごとの識別値。演出のばらつきを安定させる
	};

	// 描画用の曳光線。head・tailはいずれもワールド座標で、弾の寿命とは独立する
	struct GunTracerSegment {
		DirectX::XMFLOAT3 head{};
		DirectX::XMFLOAT3 tail{};
	};
} // namespace Game
