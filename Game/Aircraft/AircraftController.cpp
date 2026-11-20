#include "AircraftController.h"
#include <algorithm>

namespace Game {
	AircraftInput AircraftController::MapInput(const KeyboardState& keys) {
		return {.pitch = float(keys.pitchDown) - float(keys.pitchUp),
			.turn = float(keys.turnLeft) - float(keys.turnRight),
			.yaw = float(keys.yawRight) - float(keys.yawLeft),
			.throttle = float(keys.accelerate) - float(keys.decelerate),
			.highGTurn = keys.accelerate && keys.decelerate};
	}

	AircraftInput AircraftController::ReadInput(const Engine::InputSnapshot& input) const {
		const auto down = [&](UINT key) {
			return input.IsKeyDown(key);
		};
		auto controls = MapInput({down('W'), down('S'), down('A'), down('D'), down('Q'), down('E'), down(VK_CONTROL),
			down(VK_SPACE)});
		if (!input.active) return controls;

		using Engine::GamepadButton;
		const auto& pad = input.gamepad;
		controls.pitch = std::clamp(controls.pitch + pad.leftY, -1.0f, 1.0f);
		controls.turn = std::clamp(controls.turn - pad.leftX, -1.0f, 1.0f);
		const float yaw = float(pad.IsDown(GamepadButton::RightShoulder)) - float(pad.IsDown(GamepadButton::LeftShoulder));
		controls.yaw = std::clamp(controls.yaw + yaw, -1.0f, 1.0f);
		const float accelerate = std::max(float(down(VK_CONTROL)), pad.rightTrigger);
		const float decelerate = std::max(float(down(VK_SPACE)), pad.leftTrigger);
		controls.throttle = accelerate - decelerate;
		controls.highGTurn = accelerate >= 0.5f && decelerate >= 0.5f;
		return controls;
	}
} // namespace Game
