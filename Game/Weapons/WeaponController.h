#pragma once
#include "Engine/Input/RawInputState.h"

namespace Game {
	struct WeaponInput { bool fire = false; };

	class WeaponController {
	public:
		WeaponInput ReadInput(const Engine::InputSnapshot& input) const {
			return {.fire = input.IsKeyDown('F')};
		}
	};
}
