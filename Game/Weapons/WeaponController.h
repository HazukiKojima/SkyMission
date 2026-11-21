#pragma once
#include "Engine/Input/RawInputState.h"

namespace Game {
	struct WeaponInput {
		bool fire = false;
	};

	class WeaponController {
	  public:
		WeaponInput ReadInput(const Engine::InputSnapshot& input) const {
			return {.fire = input.leftMouseDown ||
				(input.active && input.gamepad.IsDown(Engine::GamepadButton::South))};
		}
	};
} // namespace Game
