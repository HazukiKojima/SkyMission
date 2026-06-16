#pragma once
#include "../EngineCommon.h"

namespace Engine {
	class Window {
	public:
		Window(UINT width, UINT height, const wchar_t* title, HINSTANCE hInstance);
		~Window() = default;

		HWND GetHandle() const { return m_hwnd; }
		UINT GetWidth() const { return m_width; }
		UINT GetHeight() const { return m_height; }

	private:
		static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		HWND m_hwnd = nullptr;
		UINT m_width = 0;
		UINT m_height = 0;
	};
}