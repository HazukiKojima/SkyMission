#include "ProjectileCollision.h"
#include <algorithm>
#include <cmath>

namespace Game {
	std::optional<float> IntersectMovingTarget(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
		const CollisionTarget& target, float startFraction, float endFraction) {
		using namespace DirectX;
		const auto world = XMLoadFloat4x4(&target.world);
		XMVECTOR determinant;
		const auto inverse = XMMatrixInverse(&determinant, world);
		if (std::abs(XMVectorGetX(determinant)) < 1e-12f)
			return std::nullopt;
		const auto displacement = world.r[3] - XMLoadFloat3(&target.previousPosition);
		// 敵の移動量を差し戻し、相対的な弾道を現在のOBBのローカル空間で判定する
		XMFLOAT3 localStart;
		XMFLOAT3 localEnd;
		XMStoreFloat3(&localStart,
			XMVector3TransformCoord(XMLoadFloat3(&start) + displacement * (1 - startFraction), inverse));
		XMStoreFloat3(&localEnd,
			XMVector3TransformCoord(XMLoadFloat3(&end) + displacement * (1 - endFraction), inverse));
		const float origins[] = {localStart.x, localStart.y, localStart.z};
		const float ends[] = {localEnd.x, localEnd.y, localEnd.z};
		const float minimum[] = {target.bounds.minimum.x, target.bounds.minimum.y, target.bounds.minimum.z};
		const float maximum[] = {target.bounds.maximum.x, target.bounds.maximum.y, target.bounds.maximum.z};
		float enterFraction = 0;
		float exitFraction = 1;
		for (int axis = 0; axis < 3; ++axis) {
			const float delta = ends[axis] - origins[axis];
			if (std::abs(delta) < 1e-8f) {
				if (origins[axis] < minimum[axis] || origins[axis] > maximum[axis])
					return std::nullopt;
			} else {
				float nearFraction = (minimum[axis] - origins[axis]) / delta;
				float farFraction = (maximum[axis] - origins[axis]) / delta;
				if (nearFraction > farFraction)
					std::swap(nearFraction, farFraction);
				enterFraction = std::max(enterFraction, nearFraction);
				exitFraction = std::min(exitFraction, farFraction);
				if (enterFraction > exitFraction)
					return std::nullopt;
			}
		}
		return enterFraction;
	}
} // namespace Game
