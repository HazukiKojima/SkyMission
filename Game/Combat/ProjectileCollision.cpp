#include "ProjectileCollision.h"
#include <algorithm>
#include <cmath>

namespace Game {
	std::optional<float> IntersectMovingTarget(const DirectX::XMFLOAT3& start,
		const DirectX::XMFLOAT3& end, const CollisionTarget& target, float startFraction, float endFraction) {
		using namespace DirectX;
		const auto world = XMLoadFloat4x4(&target.world);
		XMVECTOR determinant;
		const auto inverse = XMMatrixInverse(&determinant, world);
		if (std::abs(XMVectorGetX(determinant)) < 1e-12f) return std::nullopt;
		const auto displacement = world.r[3] - XMLoadFloat3(&target.previousPosition);
		XMFLOAT3 a, b;
		XMStoreFloat3(&a, XMVector3TransformCoord(XMLoadFloat3(&start) + displacement*(1-startFraction), inverse));
		XMStoreFloat3(&b, XMVector3TransformCoord(XMLoadFloat3(&end) + displacement*(1-endFraction), inverse));
		const float origins[] = {a.x, a.y, a.z}, ends[] = {b.x, b.y, b.z};
		const float lo[] = {target.bounds.minimum.x, target.bounds.minimum.y, target.bounds.minimum.z};
		const float hi[] = {target.bounds.maximum.x, target.bounds.maximum.y, target.bounds.maximum.z};
		float enter = 0, exit = 1;
		for (int axis = 0; axis < 3; ++axis) {
			const float delta = ends[axis] - origins[axis];
			if (std::abs(delta) < 1e-8f) {
				if (origins[axis] < lo[axis] || origins[axis] > hi[axis]) return std::nullopt;
			} else {
				float nearTime = (lo[axis]-origins[axis])/delta, farTime = (hi[axis]-origins[axis])/delta;
				if (nearTime > farTime) std::swap(nearTime, farTime);
				enter = std::max(enter, nearTime); exit = std::min(exit, farTime);
				if (enter > exit) return std::nullopt;
			}
		}
		return enter;
	}
}
