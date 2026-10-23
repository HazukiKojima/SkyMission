#include "AircraftController.h"

namespace Engine {
	AircraftInput AircraftController::ReadInput() const {
		AircraftInput input;
		if (GetAsyncKeyState('W') & 0x8000) input.pitch -= 1.0f;
		if (GetAsyncKeyState('S') & 0x8000) input.pitch += 1.0f;
		if (GetAsyncKeyState('A') & 0x8000) input.turn += 1.0f;
		if (GetAsyncKeyState('D') & 0x8000) input.turn -= 1.0f;
		if (GetAsyncKeyState('Q') & 0x8000) input.yaw -= 1.0f;
		if (GetAsyncKeyState('E') & 0x8000) input.yaw += 1.0f;
		const bool accelerating = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool decelerating = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
		if (accelerating) input.throttle += 1.0f;
		if (decelerating) input.throttle -= 1.0f;
		input.highGTurn = accelerating && decelerating;
		return input;
	}
}
