#pragma once
#include "../EngineCommon.h"

namespace Engine {
	struct AircraftInput {
		float pitch = 0.0f;
		float turn = 0.0f;
		float throttle = 0.0f;
	};

	class AircraftController {
	public:
		AircraftInput ReadInput() const;
	};
}
