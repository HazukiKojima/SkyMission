#pragma once
#include "Engine/Input/RawInputState.h"
#include "Game/Weapons/WeaponController.h"

namespace Game {
	struct CombatInput { WeaponInput weapon; bool nextTarget = false; };
	class CombatController {
	public:
		CombatInput ReadInput(const Engine::InputSnapshot& input) const {
			return {WeaponController{}.ReadInput(input), input.WasKeyPressed(VK_TAB)};
		}
	};
}
