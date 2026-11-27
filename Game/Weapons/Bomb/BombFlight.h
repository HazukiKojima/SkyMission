#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <optional>

namespace Game {
	inline constexpr float kMinimumOrdnanceReloadSeconds = 1.0f;
	inline constexpr float kMaximumOrdnanceReloadSeconds = 120.0f;

	struct BombTuning {
		static constexpr float releaseOffsetMeters = 12;
		static constexpr float lifetimeSeconds = 120;
	};

	// 重力を受けず発射速度を保つ。誘導先への旋回はミサイルシステムが適用する
	struct CarrierMissileFlight {
		static constexpr float lifetimeSeconds = 8;
		static constexpr float releaseOffsetMeters = 12;
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 velocity{};
		void Advance(float dt) {
			position.x += velocity.x * dt;
			position.y += velocity.y * dt;
			position.z += velocity.z * dt;
		}

		// 速度の大きさを保ち、目標へ最大radiansだけ旋回する（角速度ではなく今回の角度）
		void GuideTo(const DirectX::XMFLOAT3& target, float radians);

		// 現在速度のまま指定高度へ到達する秒数。平行移動や過去の交差ならnullopt
		std::optional<float> TimeToHeight(float height) const;

		// 誘導による今後の旋回は含めず、8秒以内に到達する位置だけを返す
		std::optional<DirectX::XMFLOAT3> Predict(float height) const;
	};

	// Y上向きのワールド座標で、一定重力による自由落下を計算する
	struct BombFlight {
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 velocity{};
		static constexpr float gravity = 9.81f; // ワールド単位/秒^2
		void Advance(float dt) {
			position.x += velocity.x * dt;
			position.y += velocity.y * dt - 0.5f * gravity * dt * dt;
			position.z += velocity.z * dt;
			velocity.y -= gravity * dt;
		}

		std::optional<float> TimeToHeight(float height) const;

		std::optional<DirectX::XMFLOAT3> Predict(float height) const;
	};
} // namespace Game
