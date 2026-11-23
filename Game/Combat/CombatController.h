#pragma once
#include "Engine/Input/RawInputState.h"
#include "Game/Weapons/WeaponController.h"

namespace Game {
	// 1フレーム分の戦闘操作。連射用の保持状態と、対象切替などの押下を区別する
	struct CombatInput {
		WeaponInput weapon;
		bool nextTarget = false;
		bool retry = false;

		// 右クリックとパッドの発射ボタンの押下回数を合算し、フレーム間の短い押下も保持する
		unsigned missilePressCount = 0;
		bool skipOpening = false;
	};

	// 入力スナップショットを操作意図へ変換する。武器や対象の状態は変更しない
	class CombatController {
	  public:
		CombatInput ReadInput(const Engine::InputSnapshot& input) const {
			using Engine::GamepadButton;
			const auto& pad = input.gamepad;
			return {.weapon = WeaponController{}.ReadInput(input),
				.nextTarget = input.WasKeyPressed(VK_TAB) || (input.active && pad.northTapped),
				.missilePressCount =
					input.active ? input.rightMousePressCount + pad.PressCount(GamepadButton::East) : 0u,
				.skipOpening = input.WasKeyPressed(VK_RETURN) || (input.active && pad.PressCount(GamepadButton::Menu))};
		}
	};
} // namespace Game
