#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <unordered_map>

namespace Engine {
	// 同じ更新内で入力スナップショットを共有する
	struct InputSnapshot {
		bool active = false;
		std::array<bool, 256> keys{};
		std::array<bool, 256> pressedKeys{};
		bool WasKeyPressed(UINT key) const { return active && key < pressedKeys.size() && pressedKeys[key]; }
		bool rightMouseDown = false;
		bool leftMouseDown = false;
		std::int64_t mouseDeltaX = 0;
		std::int64_t mouseDeltaY = 0;

		bool IsKeyDown(UINT key) const {
			if (!active || key >= keys.size())
				return false;
			if (key == VK_CONTROL)
				return keys[VK_LCONTROL] || keys[VK_RCONTROL];
			if (key == VK_SHIFT)
				return keys[VK_LSHIFT] || keys[VK_RSHIFT];
			if (key == VK_MENU)
				return keys[VK_LMENU] || keys[VK_RMENU];
			return keys[key];
		}
	};

	// OSの状態を照会せず、受信パケットから入力状態を復元する
	class RawInputState {
	  public:
		void SetActive(bool active) {
			if (active != m_active)
				Clear();
			m_active = active;
		}
		void Clear() {
			m_keyboards.clear();
			m_pressedKeys.fill(false);
			ClearMouse();
		}
		void ClearMouse() {
			m_mice.clear();
			m_deltaX = m_deltaY = 0;
		}
		void ForgetDevice(HANDLE device) {
			m_keyboards.erase(device);
			if (m_mice.erase(device))
				m_deltaX = m_deltaY = 0;
		}
		void ProcessKeyboard(HANDLE device, const RAWKEYBOARD& keyboard) {
			// 疑似キー・入力超過通知の0xffは押下状態に含めない
			constexpr USHORT kKeyboardOverrunMakeCode = 0xff;
			if (!m_active || keyboard.VKey >= 0xff || keyboard.MakeCode == kKeyboardOverrunMakeCode)
				return;
			// Pauseに含まれる、解放通知のないE1付き疑似Ctrl押下を除外する
			if (keyboard.VKey == VK_CONTROL && (keyboard.Flags & RI_KEY_E1))
				return;
			UINT key = keyboard.VKey;
			if (key == VK_CONTROL)
				key = keyboard.Flags & RI_KEY_E0 ? VK_RCONTROL : VK_LCONTROL;
			else if (key == VK_MENU)
				key = keyboard.Flags & RI_KEY_E0 ? VK_RMENU : VK_LMENU;
			else if (key == VK_SHIFT)
				key = keyboard.MakeCode == 0x36 ? VK_RSHIFT : VK_LSHIFT;
			const bool down = (keyboard.Flags & RI_KEY_BREAK) == 0;
			bool wasDown = false;
			for (const auto& [id, keys] : m_keyboards)
				wasDown = wasDown || keys[key];
			if (down && !wasDown)
				m_pressedKeys[key] = true;
			m_keyboards[device][key] = down;
		}
		void ProcessMouse(HANDLE device, const RAWMOUSE& mouse, int desktopWidth, int desktopHeight) {
			if (!m_active)
				return;
			const bool wasDragging = IsRightMouseDown();
			auto& state = m_mice[device];
			if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
				state.leftDown = true;
			if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP)
				state.leftDown = false;
			if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN)
				state.rightDown = true;
			if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP)
				state.rightDown = false;
			std::int64_t dx = mouse.lLastX, dy = mouse.lLastY;
			if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE) {
				// 絶対位置入力は0〜65535の正規化座標から変換する
				const LONG x = static_cast<LONG>(std::int64_t(mouse.lLastX) * (desktopWidth - 1) / 65535);
				const LONG y = static_cast<LONG>(std::int64_t(mouse.lLastY) * (desktopHeight - 1) / 65535);
				const bool virtualDesktop = (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
				if (state.hasAbsolute && state.virtualDesktop == virtualDesktop && state.width == desktopWidth &&
					state.height == desktopHeight) {
					dx = std::int64_t(x) - state.absolute.x;
					dy = std::int64_t(y) - state.absolute.y;
				} else
					dx = dy = 0; // 初回は基準位置だけを設定し、視点の飛びを防ぐ
				state.absolute = {x, y};
				state.hasAbsolute = true;
				state.virtualDesktop = virtualDesktop;
				state.width = desktopWidth;
				state.height = desktopHeight;
			} else
				state.hasAbsolute = false;
			if (wasDragging || IsRightMouseDown()) {
				m_deltaX += dx;
				m_deltaY += dy;
			}
		}
		bool IsRightMouseDown() const {
			for (const auto& [device, state] : m_mice)
				if (state.rightDown)
					return true;
			return false;
		}
		bool IsLeftMouseDown() const {
			for (const auto& [device, state] : m_mice)
				if (state.leftDown)
					return true;
			return false;
		}
		InputSnapshot ConsumeFrame() {
			InputSnapshot snapshot{.active = m_active};
			if (m_active) {
				for (const auto& [device, keys] : m_keyboards)
					for (size_t i = 0; i < keys.size(); ++i)
						snapshot.keys[i] = snapshot.keys[i] || keys[i];
				snapshot.rightMouseDown = IsRightMouseDown();
				snapshot.pressedKeys = m_pressedKeys;
				snapshot.leftMouseDown = IsLeftMouseDown();
				snapshot.mouseDeltaX = m_deltaX;
				snapshot.mouseDeltaY = m_deltaY;
			}
			m_deltaX = m_deltaY = 0;
			m_pressedKeys.fill(false);
			return snapshot;
		}

	  private:
		struct MouseState {
			bool rightDown = false;
			bool leftDown = false;
			bool hasAbsolute = false;
			bool virtualDesktop = false;
			int width = 0, height = 0;
			POINT absolute{};
		};
		bool m_active = false;
		std::array<bool, 256> m_pressedKeys{};
		std::unordered_map<HANDLE, std::array<bool, 256>> m_keyboards;
		std::unordered_map<HANDLE, MouseState> m_mice;
		std::int64_t m_deltaX = 0, m_deltaY = 0;
	};
} // namespace Engine
