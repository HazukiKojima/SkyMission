#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace Engine {
	enum class GamepadButton : unsigned { South, West, North, East, LeftShoulder, RightShoulder, RightStick, Menu, Count };

	struct GamepadSnapshot {
		float leftX = 0, leftY = 0, rightX = 0, rightY = 0;
		float leftTrigger = 0, rightTrigger = 0;
		unsigned buttons = 0;
		std::array<std::uint16_t, static_cast<unsigned>(GamepadButton::Count)> presses{};
		bool isDualShock4 = false;
		bool northTapped = false;
		bool northHeld = false;

		bool IsDown(GamepadButton button) const {
			return (buttons & (1u << static_cast<unsigned>(button))) != 0;
		}

		unsigned PressCount(GamepadButton button) const {
			return presses[static_cast<unsigned>(button)];
		}
	};

	// 同じ更新内で入力スナップショットを共有する
	struct InputSnapshot {
		bool active = false;
		std::array<bool, 256> keys{};
		std::array<bool, 256> pressedKeys{};
		std::array<std::uint16_t, 256> keyPressCounts{};

		unsigned GetKeyPressCount(UINT key) const {
			return active && key < keyPressCounts.size() ? keyPressCounts[key] : 0;
		}

		bool WasKeyPressed(UINT key) const {
			return active && key < pressedKeys.size() && pressedKeys[key];
		}

		bool rightMouseDown = false;
		unsigned rightMousePressCount = 0;
		bool leftMouseDown = false;
		bool middleMouseDown = false;
		std::int64_t mouseDeltaX = 0;
		std::int64_t mouseDeltaY = 0;
		GamepadSnapshot gamepad;

		bool WasConfirmPressed() const {
			return active && (WasKeyPressed(VK_RETURN) || gamepad.PressCount(GamepadButton::South));
		}

		bool IsTargetLookDown() const {
			const bool padLook = gamepad.isDualShock4 ? gamepad.northHeld : gamepad.IsDown(GamepadButton::East);
			return IsKeyDown('Z') || (active && padLook);
		}

		bool IsKeyDown(UINT key) const {
			if (!active || key >= keys.size()) return false;

			// 左右を区別しない修飾キーは両側の状態をまとめる
			if (key == VK_CONTROL) return keys[VK_LCONTROL] || keys[VK_RCONTROL];
			if (key == VK_SHIFT) return keys[VK_LSHIFT] || keys[VK_RSHIFT];
			if (key == VK_MENU) return keys[VK_LMENU] || keys[VK_RMENU];
			return keys[key];
		}
	};

	// OSの状態を照会せず、受信パケットから入力状態を復元する
	class RawInputState {
	  public:
		void SetActive(bool active) {
			if (active != m_active) Clear();
			m_active = active;
		}

		void Clear();

		void ClearMouse();

		void ForgetDevice(HANDLE device);

		void ProcessKeyboard(HANDLE device, const RAWKEYBOARD& keyboard);

		void ProcessMouse(HANDLE device, const RAWMOUSE& mouse, int desktopWidth, int desktopHeight);

		bool IsKeyDown(UINT key) const {
			if (!m_active || key >= 256) return false;

			for (const auto& [device, keys] : m_keyboards)
				if (keys[key]) return true;
			return false;
		}

		bool IsRightMouseDown() const {
			for (const auto& [device, state] : m_mice)
				if (state.rightDown) return true;
			return false;
		}

		bool IsMiddleMouseDown() const {
			for (const auto& [device, state] : m_mice)
				if (state.middleDown) return true;
			return false;
		}

		bool IsLeftMouseDown() const {
			for (const auto& [device, state] : m_mice)
				if (state.leftDown) return true;
			return false;
		}

		InputSnapshot ConsumeFrame();

	  private:
		struct MouseState {
			bool rightDown = false;
			bool leftDown = false;
			bool middleDown = false;
			bool hasAbsolute = false;
			bool virtualDesktop = false;
			int width = 0;
			int height = 0;
			POINT absolute{};
		};

		bool m_active = false;
		std::uint16_t m_rightMousePressCount = 0;
		std::array<bool, 256> m_pressedKeys{};
		std::array<std::uint16_t, 256> m_keyPressCounts{};
		std::unordered_map<HANDLE, std::array<bool, 256>> m_keyboards;
		std::unordered_map<HANDLE, MouseState> m_mice;
		std::int64_t m_deltaX = 0, m_deltaY = 0;
	};
} // namespace Engine
