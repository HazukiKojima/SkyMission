#pragma once
#include "Engine/EngineCommon.h"
#include "AircraftFlightModel.h"
#include "Engine/Input/RawInputState.h"

namespace Game {
	class AircraftController {
	  public:
		// フレームの入力スナップショットを機体制御用の入力値へ変換する
		struct KeyboardState {
			bool pitchUp = false;
			bool pitchDown = false;
			bool turnLeft = false;
			bool turnRight = false;
			bool yawLeft = false;
			bool yawRight = false;
			bool accelerate = false;
			bool decelerate = false;
		};
		static AircraftInput MapInput(const KeyboardState& keys);
		AircraftInput ReadInput(const Engine::InputSnapshot& input) const;
	};
} // namespace Game
