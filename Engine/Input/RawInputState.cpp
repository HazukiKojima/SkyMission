#include "RawInputState.h"

namespace Engine {
	void RawInputState::Clear() {
		m_keyboards.clear();
		m_pressedKeys.fill(false);
		m_keyPressCounts.fill(0);
		ClearMouse();
	}

	void RawInputState::ClearMouse() {
		m_mice.clear();
		m_rightMousePressCount = 0;
		m_deltaX = 0;
		m_deltaY = 0;
	}

	void RawInputState::ForgetDevice(HANDLE device) {
		m_keyboards.erase(device);
		if (m_mice.erase(device)) {
			m_deltaX = 0;
			m_deltaY = 0;
		}
	}

	void RawInputState::ProcessKeyboard(HANDLE device, const RAWKEYBOARD& keyboard) {
		// 疑似キー・入力超過通知の0xffは押下状態に含めない
		constexpr USHORT kKeyboardOverrunMakeCode = 0xff;
		if (!m_active || keyboard.VKey >= 0xff || keyboard.MakeCode == kKeyboardOverrunMakeCode) return;

		// Pauseに含まれる、解放通知のないE1付き疑似Ctrl押下を除外する
		if (keyboard.VKey == VK_CONTROL && (keyboard.Flags & RI_KEY_E1)) return;

		// 修飾キーの左右を識別する
		UINT key = keyboard.VKey;
		if (key == VK_CONTROL)
			key = keyboard.Flags & RI_KEY_E0 ? VK_RCONTROL : VK_LCONTROL;
		else if (key == VK_MENU)
			key = keyboard.Flags & RI_KEY_E0 ? VK_RMENU : VK_LMENU;
		else if (key == VK_SHIFT)
			key = keyboard.MakeCode == 0x36 ? VK_RSHIFT : VK_LSHIFT;
		const bool down = (keyboard.Flags & RI_KEY_BREAK) == 0;
		bool wasDown = false;

		// 全キーボードを通じた新規押下を判定する
		for (const auto& [id, keys] : m_keyboards)
			wasDown = wasDown || keys[key];
		if (down && !wasDown) {
			m_pressedKeys[key] = true;

			// 同じフレーム内の再押下も保持し、OSのキーリピートは数えない
			if (m_keyPressCounts[key] < std::numeric_limits<std::uint16_t>::max()) ++m_keyPressCounts[key];
		}

		m_keyboards[device][key] = down;
	}

	void RawInputState::ProcessMouse(
		HANDLE device, const RAWMOUSE& mouse, int desktopWidth, int desktopHeight) {
		if (!m_active) return;

		// 全マウスを通じた右ボタンの押下を数える
		const bool wasRightDown = IsRightMouseDown();
		if ((mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) && !wasRightDown &&
			m_rightMousePressCount < std::numeric_limits<std::uint16_t>::max())
			++m_rightMousePressCount;
		auto& state = m_mice[device];

		// 受信元マウスのボタン状態を更新する
		if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) state.leftDown = true;
		if (mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) state.leftDown = false;
		if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) state.rightDown = true;
		if (mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) state.rightDown = false;
		if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) state.middleDown = true;
		if (mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) state.middleDown = false;

		// マウス入力を移動量へそろえる
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

		// 中ボタン操作中の移動量を蓄積する
		if (IsMiddleMouseDown()) {
			m_deltaX += dx;
			m_deltaY += dy;
		}
	}

	InputSnapshot RawInputState::ConsumeFrame() {
		// フレームで共有する入力状態をまとめる
		InputSnapshot snapshot{.active = m_active};
		if (m_active) {
			for (const auto& [device, keys] : m_keyboards)
				for (size_t i = 0; i < keys.size(); ++i)
					snapshot.keys[i] = snapshot.keys[i] || keys[i];
			snapshot.rightMouseDown = IsRightMouseDown();
			snapshot.rightMousePressCount = m_rightMousePressCount;
			snapshot.pressedKeys = m_pressedKeys;
			snapshot.keyPressCounts = m_keyPressCounts;
			snapshot.leftMouseDown = IsLeftMouseDown();
			snapshot.middleMouseDown = IsMiddleMouseDown();
			snapshot.mouseDeltaX = m_deltaX;
			snapshot.mouseDeltaY = m_deltaY;
		}

		// 消費済みの押下回数と移動量をリセットする
		m_deltaX = 0;
		m_deltaY = 0;
		m_pressedKeys.fill(false);
		m_keyPressCounts.fill(0);
		m_rightMousePressCount = 0;
		return snapshot;
	}
} // namespace Engine
