#pragma once
#include "RawInputState.h"
#include <GameInput.h>
#include <optional>
#include <wrl/client.h>

namespace Engine {
	class GamepadInput {
	  public:
		GamepadInput();
		~GamepadInput();
		GamepadInput(const GamepadInput&) = delete;
		GamepadInput& operator=(const GamepadInput&) = delete;

		GamepadSnapshot ConsumeFrame();
		void Reset();

	  private:
		HMODULE m_runtime = nullptr;
		Microsoft::WRL::ComPtr<IGameInput> m_input;
		Microsoft::WRL::ComPtr<IGameInputDevice> m_device;
		Microsoft::WRL::ComPtr<IGameInputReading> m_previous;
		unsigned m_buttons = 0;
		std::optional<std::uint64_t> m_northPressedAt;
	};
} // namespace Engine
