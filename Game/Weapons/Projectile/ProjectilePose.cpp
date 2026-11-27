#include "ProjectilePose.h"

namespace Game {
	DirectX::XMFLOAT4X4 BuildProjectileTransform(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& modelForward) {
		using namespace DirectX;
		constexpr float kMinimumVelocityLengthSquared = 0.0001f;
		constexpr float kParallelDirectionDot = 0.9999f;
		constexpr float kMaxVerticalComponentForUpReference = 0.9f;

		// モデルの前方軸を速度方向へ合わせる
		const auto from = XMLoadFloat3(&modelForward);
		const auto velocityVector = XMLoadFloat3(&velocity);
		const auto to = XMVectorGetX(XMVector3LengthSq(velocityVector)) > kMinimumVelocityLengthSquared
			? XMVector3Normalize(velocityVector)
			: from;
		const float dot = std::clamp(XMVectorGetX(XMVector3Dot(from, to)), -1.0f, 1.0f);
		auto rotation = XMMatrixIdentity();

		// 正反対の向きでは前方軸に垂直な軸で半回転する
		if (dot < -kParallelDirectionDot) {
			const auto reference =
				std::abs(modelForward.y) < kMaxVerticalComponentForUpReference
					? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
			rotation = XMMatrixRotationAxis(XMVector3Cross(from, reference), XM_PI);
		} else if (dot < kParallelDirectionDot)
			rotation = XMMatrixRotationAxis(XMVector3Cross(from, to), std::acos(dot));

		// 方向補正の後にワールド位置へ移す
		const auto& p = position;
		XMFLOAT4X4 transform;
		XMStoreFloat4x4(&transform, rotation * XMMatrixTranslation(p.x, p.y, p.z));
		return transform;
	}
} // namespace Game
