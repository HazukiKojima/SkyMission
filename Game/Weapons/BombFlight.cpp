#include "BombFlight.h"

namespace Game {
	void CarrierMissileFlight::GuideTo(const DirectX::XMFLOAT3& target, float radians) {
		using namespace DirectX;

		const auto value = XMLoadFloat3(&velocity);
		const auto desired = XMLoadFloat3(&target) - XMLoadFloat3(&position);

		if (XMVectorGetX(XMVector3LengthSq(desired)) < 0.0001f) return;

		// 目標へ向くための旋回角を求める
		const auto from = XMVector3Normalize(value), to = XMVector3Normalize(desired);
		const float angle = std::acos(std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f));

		if (angle < 0.00001f) return;

		// 回転軸が決まらない場合の代替軸を選ぶ
		auto axis = XMVector3Cross(from, to);

		if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f) {
			axis = XMVector3Cross(from, XMVectorSet(0, 1, 0, 0));

			if (XMVectorGetX(XMVector3LengthSq(axis)) < 0.0001f) axis = XMVectorSet(1, 0, 0, 0);
		}

		// 速度を保って旋回角の範囲内で向きを変える
		XMStoreFloat3(
			&velocity, XMVector3TransformNormal(value, XMMatrixRotationAxis(axis, std::min(angle, radians))));
	}

	std::optional<float> CarrierMissileFlight::TimeToHeight(float height) const {
		if (std::abs(velocity.y) < 0.0001f) return std::nullopt;

		// 指定高度への到達時刻を現在の鉛直速度から求める
		const float seconds = (height - position.y) / velocity.y;
		return seconds >= 0 ? std::optional<float>(seconds) : std::nullopt;
	}

	std::optional<DirectX::XMFLOAT3> CarrierMissileFlight::Predict(float height) const {
		const auto seconds = TimeToHeight(height);

		if (!seconds || *seconds > lifetimeSeconds) return std::nullopt;
		return DirectX::XMFLOAT3{
			position.x + velocity.x * *seconds, height, position.z + velocity.z * *seconds};
	}

	std::optional<float> BombFlight::TimeToHeight(float height) const {
		// 自由落下の高さの式を到達時刻について解く
		const float discriminant = velocity.y * velocity.y + 2 * gravity * (position.y - height);

		if (discriminant < 0 || position.y < height) return std::nullopt;
		return (velocity.y + std::sqrt(discriminant)) / gravity;
	}

	std::optional<DirectX::XMFLOAT3> BombFlight::Predict(float height) const {
		const auto time = TimeToHeight(height);

		if (!time) return std::nullopt;
		return DirectX::XMFLOAT3{position.x + velocity.x * *time, height, position.z + velocity.z * *time};
	}
} // namespace Game
