#pragma once
#include "../EngineCommon.h"
#include "AircraftFlightModel.h"
#include "../Input/RawInputState.h"

namespace Engine {
	class AircraftController {
	  public:
		// フレームの入力スナップショットを機体制御用の入力値へ変換する。
		struct KeyboardState {
			bool pitchUp = false, pitchDown = false, turnLeft = false, turnRight = false;
			bool yawLeft = false, yawRight = false, accelerate = false, decelerate = false;
		};
		static AircraftInput MapInput(const KeyboardState& keys);
		AircraftInput ReadInput(const InputSnapshot& input) const;
	};
} // namespace Engine
