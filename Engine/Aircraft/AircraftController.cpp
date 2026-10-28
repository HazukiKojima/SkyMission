#include "AircraftController.h"

namespace Engine {
	AircraftInput AircraftController::MapInput(const KeyboardState& keys) {
		return {.pitch = float(keys.pitchDown) - float(keys.pitchUp),
				.turn = float(keys.turnLeft) - float(keys.turnRight),
				.yaw = float(keys.yawRight) - float(keys.yawLeft),
				.throttle = float(keys.accelerate) - float(keys.decelerate),
				.highGTurn = keys.accelerate && keys.decelerate};
	}

	AircraftInput AircraftController::ReadInput(HWND window) const {
		if (!window || GetForegroundWindow() != window)
			return {};
		const auto down = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
		return MapInput(
			{down('W'), down('S'), down('A'), down('D'), down('Q'), down('E'), down(VK_CONTROL), down(VK_SPACE)});
	}
} // namespace Engine
