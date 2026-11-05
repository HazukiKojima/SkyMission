#pragma once
#include "CombatTypes.h"

namespace Game {
	// 移動する対象のOBBと弾道線分の最初の交差位置を、線分上の割合0～1で返す
	// startFraction・endFractionは対象の前フレーム位置から現位置までの時間割合
	// 交差しない場合や対象のworld行列が逆変換不能な場合はnulloptを返す
	std::optional<float> IntersectMovingTarget(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end,
		const CollisionTarget& target, float startFraction, float endFraction);
}
