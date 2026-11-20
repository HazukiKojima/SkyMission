#pragma once
#include "Engine/Input/RawInputState.h"

namespace Game {
	struct WeaponInput {
		bool fire = false;
	};

	class WeaponController {
	  public:
		WeaponInput ReadInput(const Engine::InputSnapshot& input) const {
			using Engine::GamepadButton;
			const auto fireButton = input.gamepad.isDualShock4 ? GamepadButton::South : GamepadButton::West;
			return {.fire = input.leftMouseDown ||
				(input.active && input.gamepad.IsDown(fireButton))};
		}
	};
} // namespace Game
