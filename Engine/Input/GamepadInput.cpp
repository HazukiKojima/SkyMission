#include "GamepadInput.h"
#include "Engine/Diagnostics/Log.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace Engine {
	namespace {
		constexpr unsigned kNorthButtonMask = 1u << static_cast<unsigned>(GamepadButton::North);
		constexpr std::uint64_t kLongPressMicroseconds = 300'000;
		constexpr std::uint64_t kGamepadRescanMicroseconds = 500'000;
		constexpr ULONGLONG kRumbleUpdateMilliseconds = 16;
		constexpr float kThumbstickDeadZone = 0.18f;
		constexpr float kTriggerDeadZone = 0.12f;

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
					std::abs(state.rightThumbstickX), std::abs(state.rightThumbstickY)}) > kThumbstickDeadZone ||
				std::max(state.leftTrigger, state.rightTrigger) > kTriggerDeadZone;
		}

		GamepadSnapshot MapState(const GameInputGamepadState& state) {
			return {.leftX = ApplyDeadZone(state.leftThumbstickX, kThumbstickDeadZone),
				.leftY = ApplyDeadZone(-state.leftThumbstickY, kThumbstickDeadZone),
				.rightX = ApplyDeadZone(state.rightThumbstickX, kThumbstickDeadZone),
				.rightY = ApplyDeadZone(-state.rightThumbstickY, kThumbstickDeadZone),
				.leftTrigger = ApplyDeadZone(state.leftTrigger, kTriggerDeadZone),
				.rightTrigger = ApplyDeadZone(state.rightTrigger, kTriggerDeadZone),
				.buttons = MapButtons(state.buttons)};
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
		m_northHeldReported = false;
		m_lastRumbleUpdate = 0;
	}

	GamepadSnapshot GamepadInput::ConsumeFrame(const SampleCallback& onSample) {
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
		const auto wallNow = std::chrono::steady_clock::now();
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

		auto frame = MapState(state);
		const auto emit = [&](std::uint64_t time, const GamepadSnapshot& sample) {
			if (onSample) onSample(wallNow - std::chrono::microseconds(now - std::min(now, time)), sample);
		};
		GameInputGamepadState previousState{};
		auto held = m_previous && m_previous->GetGamepadState(&previousState) ? MapState(previousState) : frame;
		const auto emitNorthHold = [&](std::uint64_t until) {
			if (!m_northPressedAt || m_northHeldReported || until < *m_northPressedAt + kLongPressMicroseconds)
				return;

			m_northHeldReported = true;
			auto sample = held;
			sample.northHeld = true;
			sample.northHoldStarted = true;
			frame.northHoldStarted = true;
			emit(*m_northPressedAt + kLongPressMicroseconds, sample);
		};

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
			const auto addReading = [&](GamepadSnapshot sample, std::uint64_t timestamp) {
				emitNorthHold(timestamp);
				held = sample;
				const unsigned pressed = sample.buttons & ~m_buttons;
				for (unsigned i = 0; i < frame.presses.size(); ++i)
					if (pressed & (1u << i)) {
						if (frame.presses[i] < std::numeric_limits<std::uint16_t>::max())
							++frame.presses[i];
						sample.presses[i] = 1;
					}
				if (pressed & kNorthButtonMask) {
					m_northPressedAt = timestamp;
					m_northHeldReported = false;
				} else if (!(sample.buttons & kNorthButtonMask) && m_northPressedAt) {
					sample.northTapCount = timestamp - *m_northPressedAt < kLongPressMicroseconds ? 1 : 0;
					m_northPressedAt.reset();
				}
				if (sample.northTapCount && frame.northTapCount < std::numeric_limits<std::uint16_t>::max())
					++frame.northTapCount;
				sample.northHeld = m_northPressedAt && m_northHeldReported;
				m_buttons = sample.buttons;
				emit(timestamp, sample);
			};

			// フレーム間の短い押下と再押下を保持する。履歴が途切れたときは最新状態へ進む。
			while (true) {
				Microsoft::WRL::ComPtr<IGameInputReading> next;
				if (FAILED(m_input->GetNextReading(
						m_previous.Get(), GameInputKindGamepad, m_device.Get(), next.GetAddressOf())) ||
					!next || next.Get() == m_previous.Get() || next.Get() == latest.Get() ||
					next->GetTimestamp() < m_previous->GetTimestamp() ||
					next->GetTimestamp() > latest->GetTimestamp())
					break;
				GameInputGamepadState intermediate{};
				if (next->GetGamepadState(&intermediate))
					addReading(MapState(intermediate), next->GetTimestamp());
				m_previous = std::move(next);
			}
			addReading(MapState(state), latest->GetTimestamp());
		}
		emitNorthHold(now);
		frame.northHeld = m_northPressedAt && m_northHeldReported;
		m_buttons = frame.buttons;
		m_previous = std::move(latest);
		return frame;
	}
} // namespace Engine
