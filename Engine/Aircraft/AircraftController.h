#pragma once
#include "../EngineCommon.h"
#include "PlayerAircraft.h"

namespace Engine {
	class AircraftController {
	public:
		// キーボード状態を読み取り、機体制御用の入力値へ変換する。
		AircraftInput ReadInput() const;
	};
}
