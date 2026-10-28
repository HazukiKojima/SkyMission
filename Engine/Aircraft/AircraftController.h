#pragma once
#include "../EngineCommon.h"
#include "AircraftFlightModel.h"

namespace Engine {
	class AircraftController {
	  public:
		// キーボード状態を読み取り、機体制御用の入力値へ変換する。
		struct KeyboardState {
			bool pitchUp = false, pitchDown = false, turnLeft = false, turnRight = false;
			bool yawLeft = false, yawRight = false, accelerate = false, decelerate = false;
		};
		static AircraftInput MapInput(const KeyboardState& keys);
		AircraftInput ReadInput(HWND window) const;
	};
} // namespace Engine
