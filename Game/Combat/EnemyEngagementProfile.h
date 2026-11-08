#pragma once
#include "CombatTypes.h"
#include "Game/Aircraft/AircraftAIController.h"

namespace Game {
	// ロック状態による攻撃性の差を集約する。元のJSON設定や操縦状態は書き換えない
	struct EnemyEngagementProfile {
		float cruiseSpeedScale = 1;
		float passDistanceScale = 1;
		float aimHoldScale = 1;
		float attackRangeScale = 1;
		float burstTempoScale = 1;
		float shotIntervalScale = 1;

		AircraftAIParameters Apply(const AircraftAIParameters& base) const {
			auto result = base;
			result.cruiseSpeed *= cruiseSpeedScale;
			result.tailDistance *= passDistanceScale;
			result.aimHoldSeconds *= aimHoldScale;
			result.attackDistance *= attackRangeScale;
			return result;
		}
	};

	inline EnemyEngagementProfile GetEnemyEngagementProfile(
		LockState playerLock, Engine::ObjectId selected, Engine::ObjectId enemy) {
		if (playerLock == LockState::Locked && selected == enemy && enemy != Engine::InvalidObjectId)
			return {};
		// 非ロック機は再攻撃を早めて射撃機会を増やす。回避距離・旋回上限・命中精度は維持する
		return {.cruiseSpeedScale = 1.1f,
			.passDistanceScale = 0.8f,
			.aimHoldScale = 0.65f,
			.attackRangeScale = 1.1f,
			.burstTempoScale = 1.35f,
			.shotIntervalScale = 0.75f};
	}
} // namespace Game
