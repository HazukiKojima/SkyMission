#pragma once
#include "Engine/Input/RawInputState.h"
#include "Game/Weapons/WeaponController.h"

namespace Game {
	struct CombatInput {
		WeaponInput weapon;
		bool nextTarget = false;
		bool retry = false;
		unsigned bombPressCount = 0;
	};
	class CombatController {
	  public:
		CombatInput ReadInput(const Engine::InputSnapshot& input) const {
			return {.weapon = WeaponController{}.ReadInput(input),
				.nextTarget = input.WasKeyPressed(VK_TAB),
				.retry = input.WasKeyPressed('R'),
				.bombPressCount = input.active ? input.rightMousePressCount : 0u};
		}
	};
} // namespace Game
