#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <optional>

namespace Game {
	struct BombFlight {
		DirectX::XMFLOAT3 position{}, velocity{};
		static constexpr float gravity = 9.81f;
		void Advance(float dt) {
			position.x += velocity.x * dt;
			position.y += velocity.y * dt - 0.5f * gravity * dt * dt;
			position.z += velocity.z * dt;
			velocity.y -= gravity * dt;
		}
		std::optional<float> TimeToHeight(float height) const {
			const float discriminant = velocity.y * velocity.y + 2 * gravity * (position.y - height);
			if (discriminant < 0 || position.y < height)
				return std::nullopt;
			return (velocity.y + std::sqrt(discriminant)) / gravity;
		}
		std::optional<DirectX::XMFLOAT3> Predict(float height) const {
			const auto time = TimeToHeight(height);
			if (!time)
				return std::nullopt;
			return DirectX::XMFLOAT3{
				position.x + velocity.x * *time, height, position.z + velocity.z * *time};
		}
	};
} // namespace Game
