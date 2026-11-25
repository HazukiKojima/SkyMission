#include "GamepadInput.h"
#include "Engine/Diagnostics/Log.h"
#include <algorithm>
#include <bit>
#include <cmath>

namespace Engine {
	namespace {
		constexpr unsigned kNorthButtonMask = 1u << static_cast<unsigned>(GamepadButton::North);
		constexpr std::uint64_t kLongPressMicroseconds = 300'000;
		constexpr std::uint64_t kGamepadRescanMicroseconds = 500'000;
		constexpr ULONGLONG kRumbleUpdateMilliseconds = 16;

		float ApplyDeadZone(float value, float deadZone) {
			const float magnitude = std::clamp((std::abs(value) - deadZone) / (1.0f - deadZone), 0.0f, 1.0f);
			return std::copysign(magnitude, value);
		}

		unsigned MapButtons(GameInputGamepadButtons buttons) {
			const auto bit = [buttons](GamepadButton button, GameInputGamepadButtons mask) {
				return (buttons & mask) ? 1u << static_cast<unsigned>(button) : 0u;
			};
			return bit(GamepadButton::South, GameInputGamepadA) |
				bit(GamepadButton::North, GameInputGamepadY) |
				bit(GamepadButton::East, GameInputGamepadB) |
				bit(GamepadButton::LeftShoulder, GameInputGamepadLeftShoulder) |
				bit(GamepadButton::RightShoulder, GameInputGamepadRightShoulder) |
				bit(GamepadButton::Menu, GameInputGamepadMenu);
		}

		bool HasGamepadInput(const GameInputGamepadState& state) {
			return state.buttons != GameInputGamepadNone ||
				std::max({std::abs(state.leftThumbstickX), std::abs(state.leftThumbstickY),
					std::abs(state.rightThumbstickX), std::abs(state.rightThumbstickY)}) > 0.18f ||
				std::max(state.leftTrigger, state.rightTrigger) > 0.12f;
		}
	} // namespace

	GamepadInput::GamepadInput() {
		// Windows SDKのGameInput v0を使い、更新済みランタイムがあれば優先する。
		// ランタイムのない環境でもキーボードとマウスで起動できるよう動的に読み込む。
		const wchar_t* runtimes[] = {L"GameInputRedist.dll", L"GameInput.dll"};
		for (const auto* name : runtimes) {
			const HMODULE runtime = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
			if (!runtime) continue;
			const auto create =
				std::bit_cast<decltype(&GameInputCreate)>(GetProcAddress(runtime, "GameInputCreate"));
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

	void GamepadInput::SetRumble(float lowFrequency, float highFrequency) {
		if (!m_device || (m_rumble.lowFrequency == lowFrequency && m_rumble.highFrequency == highFrequency))
			return;
		const bool wasVibrating = m_rumble.lowFrequency > 0 || m_rumble.highFrequency > 0;
		const bool isVibrating = lowFrequency > 0 || highFrequency > 0;
		const ULONGLONG now = GetTickCount64();
		// 振動中の強さの更新だけを間引き、開始と停止は即時に送る
		if (wasVibrating && isVibrating && now - m_lastRumbleUpdate < kRumbleUpdateMilliseconds) return;
		m_rumble = {.lowFrequency = lowFrequency, .highFrequency = highFrequency};
		m_device->SetRumbleState(&m_rumble);
		m_lastRumbleUpdate = now;
	}

	void GamepadInput::Reset() {
		SetRumble(0, 0);
		m_previous.Reset();
		m_device.Reset();
		m_buttons = 0;
		m_northPressedAt.reset();
		m_lastRumbleUpdate = 0;
	}

	GamepadSnapshot GamepadInput::ConsumeFrame() {
		if (!m_input) return {};
		if (m_device && !(m_device->GetDeviceStatus() & GameInputDeviceConnected)) {
			Log("GameInput: active gamepad disconnected\n");
			Reset();
		}

		Microsoft::WRL::ComPtr<IGameInputReading> latest;
		GameInputGamepadState state{};
		const HRESULT read =
			m_input->GetCurrentReading(GameInputKindGamepad, m_device.Get(), latest.GetAddressOf());
		if (FAILED(read) || !latest || !latest->GetGamepadState(&state)) {
			if (!m_device) return {};
			Log("GameInput: active gamepad reading failed (0x{:08x}), scanning again\n",
				static_cast<unsigned>(read));
			Reset();
			if (FAILED(m_input->GetCurrentReading(
					GameInputKindGamepad, nullptr, latest.ReleaseAndGetAddressOf())) ||
				!latest || !latest->GetGamepadState(&state))
				return {};
		}

		const auto now = m_input->GetCurrentTimestamp();
		if (m_device && latest->GetTimestamp() + kGamepadRescanMicroseconds < now) {
			// 操作されていないパッドでは、同じ読み取りが返り続ける
			// 別のパッドで新しい操作を検出したときだけ切り替える
			Microsoft::WRL::ComPtr<IGameInputReading> candidate;
			GameInputGamepadState candidateState{};
			if (SUCCEEDED(
					m_input->GetCurrentReading(GameInputKindGamepad, nullptr, candidate.GetAddressOf())) &&
				candidate && candidate->GetTimestamp() > latest->GetTimestamp() &&
				candidate->GetTimestamp() + kGamepadRescanMicroseconds >= now &&
				candidate->GetGamepadState(&candidateState) && HasGamepadInput(candidateState)) {
				Reset();
				latest = std::move(candidate);
				state = candidateState;
			}
		}

		GamepadSnapshot frame{.leftX = ApplyDeadZone(state.leftThumbstickX, 0.18f),
			.leftY = ApplyDeadZone(-state.leftThumbstickY, 0.18f),
			.rightX = ApplyDeadZone(state.rightThumbstickX, 0.18f),
			.rightY = ApplyDeadZone(-state.rightThumbstickY, 0.18f),
			.leftTrigger = ApplyDeadZone(state.leftTrigger, 0.12f),
			.rightTrigger = ApplyDeadZone(state.rightTrigger, 0.12f),
			.buttons = MapButtons(state.buttons)};

		if (!m_previous) {
			// 接続・フォーカス復帰時に、すでに保持しているボタンを新規押下として扱わない。
			latest->GetDevice(m_device.GetAddressOf());
			if (!m_device || !(m_device->GetDeviceStatus() & GameInputDeviceConnected)) {
				Reset();
				return {};
			}
			if (const auto* info = m_device->GetDeviceInfo())
				Log("GameInput: active gamepad {:04x}:{:04x}\n", info->vendorId, info->productId);
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
				if (FAILED(m_input->GetNextReading(
						m_previous.Get(), GameInputKindGamepad, m_device.Get(), next.GetAddressOf())) ||
					next.Get() == latest.Get() || next->GetTimestamp() > latest->GetTimestamp())
					break;
				GameInputGamepadState intermediate{};
				if (next->GetGamepadState(&intermediate))
					addPresses(MapButtons(intermediate.buttons), next->GetTimestamp());
				m_previous = std::move(next);
			}
			addPresses(frame.buttons, latest->GetTimestamp());
		}
		frame.northHeld =
			m_northPressedAt && m_input->GetCurrentTimestamp() - *m_northPressedAt >= kLongPressMicroseconds;
		m_buttons = frame.buttons;
		m_previous = std::move(latest);
		return frame;
	}
} // namespace Engine
