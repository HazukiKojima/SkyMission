#pragma once
#include "../EngineCommon.h"

namespace Engine {
	// プレイヤー機体へ渡す正規化済みの操縦入力。
	struct AircraftInput {
		float pitch = 0.0f;
		float turn = 0.0f;
		float yaw = 0.0f;
		float throttle = 0.0f;
		bool highGTurn = false;
	};

	class AircraftController {
	public:
		// キーボード状態を読み取り、機体制御用の入力値へ変換する。
		AircraftInput ReadInput() const;
	};
}
