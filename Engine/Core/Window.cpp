#include "Window.h"

namespace Engine {
	// Win32APIを使用したメインウィンドウの生成および登録
	Window::Window(UINT width, UINT height, const wchar_t* title, HINSTANCE hInstance)
		: m_width(width), m_height(height)
	{
		WNDCLASS wc = {};
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = L"DX12WindowClass";
		RegisterClass(&wc);

		// 外枠を含めた全体のウィンドウサイズをクライアント領域から逆算
		RECT rc = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		m_hwnd = CreateWindowEx(
			0, wc.lpszClassName, title,
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top,
			nullptr, nullptr, hInstance, nullptr
		);

		if (!m_hwnd) {
			throw std::runtime_error("ウィンドウの作成に失敗しました。");
		}
	}

	// ウィンドウメッセージを処理するコールバック関数
	LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		if (uMsg == WM_DESTROY) {
			PostQuitMessage(0); // メインループ終了のトリガー
			return 0;
		}
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}