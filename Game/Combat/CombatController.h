#pragma once
#include "Engine/Input/RawInputState.h"
#include "Game/Weapons/WeaponController.h"

namespace Game {
	// 1フレーム分の戦闘操作。連射用の保持状態と、対象切替などの押下を区別する
	struct CombatInput {
		WeaponInput weapon;
		bool nextTarget = false;
		bool retry = false;

		// フレーム間に発生した右クリック回数。短い押下や複数回の入力を失わない
		unsigned missilePressCount = 0;
		bool skipOpening = false;
	};

	// 入力スナップショットを操作意図へ変換する。武器や対象の状態は変更しない
	class CombatController {
	  public:
		CombatInput ReadInput(const Engine::InputSnapshot& input) const {
			using Engine::GamepadButton;
			const auto& pad = input.gamepad;
			const auto missileButton = pad.isDualShock4 ? GamepadButton::East : GamepadButton::North;
			const auto skipButton = pad.isDualShock4 ? GamepadButton::Menu : GamepadButton::South;
			const bool padNextTarget = pad.isDualShock4 ? pad.northTapped : pad.PressCount(GamepadButton::RightStick) != 0;
			return {.weapon = WeaponController{}.ReadInput(input),
				.nextTarget = input.WasKeyPressed(VK_TAB) || (input.active && padNextTarget),
				.missilePressCount = input.active ? input.rightMousePressCount + pad.PressCount(missileButton) : 0u,
				.skipOpening = input.WasKeyPressed(VK_RETURN) || (input.active && pad.PressCount(skipButton))};
		}
	};
} // namespace Game
