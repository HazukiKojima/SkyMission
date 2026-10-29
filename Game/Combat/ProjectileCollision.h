#pragma once
#include "CombatTypes.h"

namespace Game {
	std::optional<float> IntersectMovingTarget(const DirectX::XMFLOAT3& start,
		const DirectX::XMFLOAT3& end, const CollisionTarget& target, float startFraction, float endFraction);
}
