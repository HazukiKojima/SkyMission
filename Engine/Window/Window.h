#pragma once
#include <chrono>
#include <functional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "../EngineCommon.h"
#include "../Input/GamepadInput.h"

namespace Engine {
	struct TimedInputSample {
		std::chrono::steady_clock::time_point time;
		std::variant<InputSnapshot, GamepadSnapshot> state;
	};

	class Window {
	  public:
		Window(UINT width, UINT height, const wchar_t* title, HINSTANCE hInstance);
		~Window();
		Window(const Window&) = delete;

		Window& operator=(const Window&) = delete;

		InputSnapshot ConsumeInput();
		void SetTimedInputEnabled(bool enabled);
		std::vector<TimedInputSample> TakeInputSamples();
		void SetGamepadRumble(float lowFrequency, float highFrequency);
		void SetLoading(bool loading);
		void SetLoadingProgress(int percent, std::wstring_view item);

		bool IsCloseRequested() const {
			return m_closeRequested;
		}

		HWND GetHandle() const {
			return m_hwnd;
		}

		UINT GetWidth() const {
			return m_width;
		}

		UINT GetHeight() const {
			return m_height;
		}

		void SetOnResize(std::function<void(UINT, UINT)> cb) {
			m_onResize = cb;
		}

	  private:
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		void ProcessRawInput(HRAWINPUT handle);
		void RecordRawInput();
		void DeactivateInput();
		void UnregisterRawInput();
		void BeginCameraLook();
		void EndCameraLook();
		void ToggleFullscreen();

		RawInputState m_input;
		GamepadInput m_gamepad;
		bool m_timedInputEnabled = false;
		std::vector<TimedInputSample> m_inputSamples;
		std::chrono::steady_clock::time_point m_lastRawInputTime;
		HWND m_hwnd = nullptr;
		bool m_cameraMouseCaptured = false;
		bool m_loading = false;
		int m_loadingPercent = 0;
		std::wstring m_loadingItem;
		bool m_closeRequested = false;
		bool m_fullscreen = false;
		bool m_fullscreenEnterHeld = false;
		WINDOWPLACEMENT m_windowedPlacement{sizeof(WINDOWPLACEMENT)};
		LONG_PTR m_windowedStyle = 0;
		UINT m_width = 0;
		UINT m_height = 0;
		std::function<void(UINT, UINT)> m_onResize;
	};
} // namespace Engine
