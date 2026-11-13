#pragma once
#include "CombatTypes.h"

namespace Game {
	// 自機速度を引き継ぐ等速弾と等速移動する目標の迎撃方向を求める
	// 位置はワールド座標、速度はワールド単位/秒、lifetimeは秒
	// 弾の寿命内に正の迎撃時刻がない場合や、弾速・寿命が不正ならnullopt
	std::optional<AimSolution> SolveGunAim(const DirectX::XMFLOAT3& muzzle,
		const DirectX::XMFLOAT3& shooterVelocity, const CombatTarget& target, float speed, float lifetime);
	// ベクトルを正規化して角度を比較する。ほぼゼロ長の方向は範囲外とする
	bool IsWithinAngle(const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& direction, float degrees);
} // namespace Game
