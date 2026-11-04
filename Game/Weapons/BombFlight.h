#pragma once
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <optional>

namespace Game {
	struct BombTuning {
		static constexpr float releaseOffsetMeters = 12;
		static constexpr float lifetimeSeconds = 120;
	};
	// Mission-two missiles retain launch speed without gravity; the scene applies locked-target steering.
	struct CarrierMissileFlight {
		DirectX::XMFLOAT3 position{}, velocity{};
		void Advance(float dt) {
			position.x += velocity.x * dt;
			position.y += velocity.y * dt;
			position.z += velocity.z * dt;
		}
		void GuideTo(const DirectX::XMFLOAT3& target, float radians) {
			using namespace DirectX;
			const auto value = XMLoadFloat3(&velocity);
			const auto desired = XMLoadFloat3(&target) - XMLoadFloat3(&position);
			if (XMVectorGetX(XMVector3LengthSq(desired)) < 0.0001f) return;
			const auto from = XMVector3Normalize(value), to = XMVector3Normalize(desired);
			const float angle = std::acos(std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f));
			if (angle < 0.00001f) return;
			auto axis = XMVector3Cross(from, to);
			if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f) {
				axis = XMVector3Cross(from, XMVectorSet(0, 1, 0, 0));
				if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f) axis = XMVectorSet(1, 0, 0, 0);
			}
			XMStoreFloat3(&velocity, XMVector3TransformNormal(value, XMMatrixRotationAxis(axis, std::min(angle, radians))));
		}
		std::optional<float> TimeToHeight(float height) const {
			if (std::abs(velocity.y) < 0.0001f) return std::nullopt;
			const float seconds = (height - position.y) / velocity.y;
			return seconds >= 0 ? std::optional<float>(seconds) : std::nullopt;
		}
		std::optional<DirectX::XMFLOAT3> Predict(float height) const {
			const auto seconds = TimeToHeight(height);
			if (!seconds || *seconds > 8) return std::nullopt;
			return DirectX::XMFLOAT3{position.x + velocity.x * *seconds, height,
				position.z + velocity.z * *seconds};
		}
	};
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
