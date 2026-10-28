#pragma once
#include "Engine/EngineCommon.h"
#include "AircraftFlightModel.h"
#include "Engine/Input/RawInputState.h"

namespace Game {
	class AircraftController {
	  public:
		// フレームの入力スナップショットを機体制御用の入力値へ変換する
		struct KeyboardState {
			bool pitchUp = false, pitchDown = false, turnLeft = false, turnRight = false;
			bool yawLeft = false, yawRight = false, accelerate = false, decelerate = false;
		};
		static AircraftInput MapInput(const KeyboardState& keys);
		AircraftInput ReadInput(const Engine::InputSnapshot& input) const;
	};
} // namespace Game
