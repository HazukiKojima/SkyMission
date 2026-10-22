#include "AircraftController.h"

namespace Engine {
	AircraftInput AircraftController::ReadInput() const {
		AircraftInput input;
		if (GetAsyncKeyState('W') & 0x8000) input.pitch -= 1.0f;
		if (GetAsyncKeyState('S') & 0x8000) input.pitch += 1.0f;
		if (GetAsyncKeyState('A') & 0x8000) input.turn += 1.0f;
		if (GetAsyncKeyState('D') & 0x8000) input.turn -= 1.0f;
		if (GetAsyncKeyState(VK_CONTROL) & 0x8000) input.throttle += 1.0f;
		if (GetAsyncKeyState(VK_SPACE) & 0x8000) input.throttle -= 1.0f;
		return input;
	}
}
