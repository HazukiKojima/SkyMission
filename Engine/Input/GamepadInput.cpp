#include "GamepadInput.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace Engine {
	namespace {
		constexpr unsigned kNorthButtonMask = 1u << static_cast<unsigned>(GamepadButton::North);
		constexpr std::uint64_t kLongPressMicroseconds = 300'000;

		float ApplyDeadZone(float value, float deadZone) {
			const float magnitude = std::clamp((std::abs(value) - deadZone) / (1.0f - deadZone), 0.0f, 1.0f);
			return std::copysign(magnitude, value);
		}

		unsigned MapButtons(GameInputGamepadButtons buttons) {
			const auto bit = [buttons](GamepadButton button, GameInputGamepadButtons mask) {
				return (buttons & mask) ? 1u << static_cast<unsigned>(button) : 0u;
			};
			return bit(GamepadButton::South, GameInputGamepadA) |
				bit(GamepadButton::West, GameInputGamepadX) |
				bit(GamepadButton::North, GameInputGamepadY) |
				bit(GamepadButton::East, GameInputGamepadB) |
				bit(GamepadButton::LeftShoulder, GameInputGamepadLeftShoulder) |
				bit(GamepadButton::RightShoulder, GameInputGamepadRightShoulder) |
				bit(GamepadButton::RightStick, GameInputGamepadRightThumbstick) |
				bit(GamepadButton::Menu, GameInputGamepadMenu);
		}
	}

	GamepadInput::GamepadInput() {
		// Windows SDKのGameInput v0を使い、更新済みランタイムがあれば優先する。
		// ランタイムのない環境でもキーボードとマウスで起動できるよう動的に読み込む。
		const wchar_t* runtimes[] = {L"GameInputRedist.dll", L"GameInput.dll"};
		for (const auto* name : runtimes) {
			const HMODULE runtime = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (!runtime) continue;
			const auto create = std::bit_cast<decltype(&GameInputCreate)>(GetProcAddress(runtime, "GameInputCreate"));
			if (create && SUCCEEDED(create(m_input.GetAddressOf())) && m_input) {
				m_runtime = runtime;
				return;
			}
			m_input.Reset();
			FreeLibrary(runtime);
		}
	}

	GamepadInput::~GamepadInput() {
		Reset();
		m_input.Reset();
		if (m_runtime) FreeLibrary(m_runtime);
	}

	void GamepadInput::Reset() {
		m_previous.Reset();
		m_device.Reset();
		m_buttons = 0;
		m_northPressedAt.reset();
	}

	GamepadSnapshot GamepadInput::ConsumeFrame() {
		if (!m_input) return {};
		if (m_device && !(m_device->GetDeviceStatus() & GameInputDeviceConnected)) Reset();

		Microsoft::WRL::ComPtr<IGameInputReading> latest;
		GameInputGamepadState state{};
		if (FAILED(m_input->GetCurrentReading(GameInputKindGamepad, m_device.Get(), latest.GetAddressOf())) ||
			!latest->GetGamepadState(&state)) {
			Reset();
			return {};
		}

		GamepadSnapshot frame{
			.leftX = ApplyDeadZone(state.leftThumbstickX, 0.18f),
			.leftY = ApplyDeadZone(-state.leftThumbstickY, 0.18f),
			.rightX = ApplyDeadZone(state.rightThumbstickX, 0.18f),
			.rightY = ApplyDeadZone(-state.rightThumbstickY, 0.18f),
			.leftTrigger = ApplyDeadZone(state.leftTrigger, 0.12f),
			.rightTrigger = ApplyDeadZone(state.rightTrigger, 0.12f),
			.buttons = MapButtons(state.buttons)};

		if (!m_previous) {
			// 接続・フォーカス復帰時に、すでに保持しているボタンを新規押下として扱わない。
			latest->GetDevice(m_device.GetAddressOf());
		} else if (m_previous.Get() != latest.Get()) {
			const auto addPresses = [&](unsigned buttons, std::uint64_t timestamp) {
				const unsigned pressed = buttons & ~m_buttons;
				for (unsigned i = 0; i < frame.presses.size(); ++i)
					if (pressed & (1u << i)) ++frame.presses[i];
				if (pressed & kNorthButtonMask) {
					m_northPressedAt = timestamp;
				} else if (!(buttons & kNorthButtonMask) && m_northPressedAt) {
					frame.northTapped |= timestamp - *m_northPressedAt < kLongPressMicroseconds;
					m_northPressedAt.reset();
				}
				m_buttons = buttons;
			};

			// フレーム間の短い押下と再押下を保持する。履歴が途切れたときは最新状態へ進む。
			for (unsigned i = 0; i < 256; ++i) {
				Microsoft::WRL::ComPtr<IGameInputReading> next;
				if (FAILED(m_input->GetNextReading(m_previous.Get(), GameInputKindGamepad,
						m_device.Get(), next.GetAddressOf())) ||
					next.Get() == latest.Get() || next->GetTimestamp() > latest->GetTimestamp())
					break;
				GameInputGamepadState intermediate{};
				if (next->GetGamepadState(&intermediate))
					addPresses(MapButtons(intermediate.buttons), next->GetTimestamp());
				m_previous = std::move(next);
			}
			addPresses(frame.buttons, latest->GetTimestamp());
		}
		frame.northHeld = m_northPressedAt &&
			m_input->GetCurrentTimestamp() - *m_northPressedAt >= kLongPressMicroseconds;
		const auto* info = m_device ? m_device->GetDeviceInfo() : nullptr;
		frame.isDualShock4 = info && info->vendorId == 0x054c &&
			(info->productId == 0x05c4 || info->productId == 0x09cc);
		m_buttons = frame.buttons;
		m_previous = std::move(latest);
		return frame;
	}
} // namespace Engine
