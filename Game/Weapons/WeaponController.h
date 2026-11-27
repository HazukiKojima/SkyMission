#pragma once
#include "Engine/Input/RawInputState.h"

namespace Game {
	struct WeaponInput {
		bool fire = false;
	};

	class WeaponController {
	  public:
		WeaponInput ReadInput(const Engine::InputSnapshot& input) const {
			constexpr auto fireButton = Engine::GamepadButton::South;
			return {.fire = input.active && (input.leftMouseDown || input.leftMousePressed ||
				input.gamepad.IsDown(fireButton) || input.gamepad.PressCount(fireButton) != 0)};
		}
	};
} // namespace Game
