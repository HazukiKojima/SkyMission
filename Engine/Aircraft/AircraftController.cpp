#include "AircraftController.h"

namespace Engine {
	AircraftInput AircraftController::MapInput(const KeyboardState& keys) {
		return {.pitch = float(keys.pitchDown) - float(keys.pitchUp),
				.turn = float(keys.turnLeft) - float(keys.turnRight),
				.yaw = float(keys.yawRight) - float(keys.yawLeft),
				.throttle = float(keys.accelerate) - float(keys.decelerate),
				.highGTurn = keys.accelerate && keys.decelerate};
	}

	AircraftInput AircraftController::ReadInput(const InputSnapshot& input) const {
		const auto down = [&](UINT key) { return input.IsKeyDown(key); };
		return MapInput(
			{down('W'), down('S'), down('A'), down('D'), down('Q'), down('E'), down(VK_CONTROL), down(VK_SPACE)});
	}
} // namespace Engine
