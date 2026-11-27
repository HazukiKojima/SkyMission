#pragma once
#include "RawInputState.h"
#include <GameInput.h>
#include <chrono>
#include <functional>
#include <optional>
#include <wrl/client.h>

namespace Engine {
	class GamepadInput {
	  public:
		GamepadInput();
		~GamepadInput();
		GamepadInput(const GamepadInput&) = delete;
		GamepadInput& operator=(const GamepadInput&) = delete;

		using SampleCallback = std::function<void(std::chrono::steady_clock::time_point, const GamepadSnapshot&)>;
		GamepadSnapshot ConsumeFrame(const SampleCallback& onSample = {});
		void SetRumble(float lowFrequency, float highFrequency);
		void Reset();

	  private:
		HMODULE m_runtime = nullptr;
		Microsoft::WRL::ComPtr<IGameInput> m_input;
		Microsoft::WRL::ComPtr<IGameInputDevice> m_device;
		Microsoft::WRL::ComPtr<IGameInputReading> m_previous;
		unsigned m_buttons = 0;
		std::optional<std::uint64_t> m_northPressedAt;
		bool m_northHeldReported = false;
		GameInputRumbleParams m_rumble{};
		ULONGLONG m_lastRumbleUpdate = 0;
	};
} // namespace Engine
