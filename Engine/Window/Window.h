#pragma once
#include <functional>
#include "../EngineCommon.h"
#include "../Input/RawInputState.h"

namespace Engine {
	class Window {
	  public:
		Window(UINT width, UINT height, const wchar_t* title, HINSTANCE hInstance);
		~Window();
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		InputSnapshot ConsumeInput();

		HWND GetHandle() const {
			return m_hwnd;
		}
		UINT GetWidth() const {
			return m_width;
		}
		UINT GetHeight() const {
			return m_height;
		}

		// リサイズイベントのコールバック登録
		void SetOnResize(std::function<void(UINT, UINT)> cb) {
			m_onResize = cb;
		}

	  private:
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		void ProcessRawInput(HRAWINPUT handle);
		void DeactivateInput();
		void UnregisterRawInput();
		void BeginCameraLook();
		void EndCameraLook();

		RawInputState m_input;
		HWND m_hwnd = nullptr;
		bool m_cameraMouseCaptured = false;
		UINT m_width = 0;
		UINT m_height = 0;
		std::function<void(UINT, UINT)> m_onResize;
	};
} // namespace Engine
