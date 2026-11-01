#include "Window.h"
#include "../Log.h"
#include <cstddef>
#include <vector>

namespace Engine {
	Window::Window(UINT width, UINT height, const wchar_t* title, HINSTANCE hInstance)
		: m_width(width), m_height(height) {
		WNDCLASSW wc{};
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = L"DX12WindowClass";
		if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		RECT rc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
		m_hwnd = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
			CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, this);
		if (!m_hwnd)
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

		// 前面時のみ受信し、ウィンドウ操作やAlt+F4用の従来メッセージは残す
		const RAWINPUTDEVICE devices[] = {
			{0x01, 0x06, RIDEV_DEVNOTIFY, m_hwnd}, {0x01, 0x02, RIDEV_DEVNOTIFY, m_hwnd}};
		if (!RegisterRawInputDevices(devices, _countof(devices), sizeof(RAWINPUTDEVICE))) {
			const HRESULT error = HRESULT_FROM_WIN32(GetLastError());
			DestroyWindow(m_hwnd);
			ThrowIfFailed(error);
		}
		m_input.SetActive(GetForegroundWindow() == m_hwnd);
	}

	Window::~Window() {
		m_onResize = {};
		if (m_hwnd && IsWindow(m_hwnd))
			DestroyWindow(m_hwnd);
	}

	InputSnapshot Window::ConsumeInput() {
		if (GetForegroundWindow() != m_hwnd)
			DeactivateInput();
		return m_input.ConsumeFrame();
	}

	void Window::DeactivateInput() {
		m_input.SetActive(false);
		EndCameraLook();
	}

	void Window::BeginCameraLook() {
		// 標準のウィンドウ移動・サイズ変更が所有するキャプチャは奪わない。
		if (m_cameraMouseCaptured || GetCapture() != nullptr)
			return;
		SetCapture(m_hwnd);
		m_cameraMouseCaptured = GetCapture() == m_hwnd;
	}

	void Window::EndCameraLook() {
		if (!m_cameraMouseCaptured)
			return;
		m_cameraMouseCaptured = false;
		if (GetCapture() == m_hwnd)
			ReleaseCapture();
	}

	void Window::UnregisterRawInput() {
		// プロセス共通の入力登録を、別のWindowから解除しない
		UINT count = 0;
		if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) == UINT(-1))
			return;
		std::vector<RAWINPUTDEVICE> registered(count), remove;
		if (count &&
			GetRegisteredRawInputDevices(registered.data(), &count, sizeof(RAWINPUTDEVICE)) == UINT(-1))
			return;
		for (const auto& device : registered) {
			if (device.hwndTarget == m_hwnd && device.usUsagePage == 0x01 &&
				(device.usUsage == 0x06 || device.usUsage == 0x02))
				remove.push_back({device.usUsagePage, device.usUsage, RIDEV_REMOVE, nullptr});
		}
		if (!remove.empty() &&
			!RegisterRawInputDevices(remove.data(), static_cast<UINT>(remove.size()), sizeof(RAWINPUTDEVICE)))
			Log("Window: raw input unregistration failed ({})\n", GetLastError());
	}

	void Window::ProcessRawInput(HRAWINPUT handle) {
		if (GetForegroundWindow() != m_hwnd) {
			DeactivateInput();
			return;
		}
		UINT size = 0;
		if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == UINT(-1))
			return;
		if (size < sizeof(RAWINPUTHEADER))
			return;
		// GetRawInputDataの受信領域をDWORD境界にそろえる
		std::vector<DWORD> storage((size + sizeof(DWORD) - 1) / sizeof(DWORD));
		const UINT bytes = GetRawInputData(handle, RID_INPUT, storage.data(), &size, sizeof(RAWINPUTHEADER));
		if (bytes == UINT(-1) || bytes < sizeof(RAWINPUTHEADER)) {
			Log("Window: failed to read raw input packet ({})\n", GetLastError());
			return;
		}
		const auto& raw = *reinterpret_cast<const RAWINPUT*>(storage.data());
		if (raw.header.dwType == RIM_TYPEKEYBOARD &&
			bytes >= offsetof(RAWINPUT, data) + sizeof(RAWKEYBOARD)) {
			m_input.ProcessKeyboard(raw.header.hDevice, raw.data.keyboard);
			if (m_input.IsKeyDown('Z'))
				BeginCameraLook();
			else
				EndCameraLook();
		} else if (raw.header.dwType == RIM_TYPEMOUSE &&
			bytes >= offsetof(RAWINPUT, data) + sizeof(RAWMOUSE)) {
			const bool virtualDesktop = (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
			m_input.ProcessMouse(raw.header.hDevice, raw.data.mouse,
				GetSystemMetrics(virtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN),
				GetSystemMetrics(virtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN));
		}
	}

	LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
		auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (message == WM_NCCREATE) {
			self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
			self->m_hwnd = hwnd;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		if (self) {
			switch (message) {
			case WM_KEYDOWN:
				if (wParam == 'Z') {
					self->BeginCameraLook();
					return 0;
				}
				break;
			case WM_KEYUP:
				if (wParam == 'Z') {
					if (!self->m_input.IsKeyDown('Z'))
						self->EndCameraLook();
					return 0;
				}
				break;
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_CONTEXTMENU:
				// 発射はRaw Inputの押下エッジで処理し、カメラのキャプチャには使わない。
				return 0;
			case WM_INPUT:
				if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT)
					self->ProcessRawInput(reinterpret_cast<HRAWINPUT>(lParam));
				// 前面のWM_INPUTは後始末のためDefWindowProcへ渡す
				return DefWindowProcW(hwnd, message, wParam, lParam);
			case WM_SETFOCUS:
				self->m_input.SetActive(true);
				break;
			case WM_KILLFOCUS:
				self->DeactivateInput();
				break;
			case WM_ACTIVATEAPP:
				if (!wParam)
					self->DeactivateInput();
				break;
			case WM_INPUT_DEVICE_CHANGE:
				if (wParam == GIDC_REMOVAL) {
					self->m_input.ForgetDevice(reinterpret_cast<HANDLE>(lParam));
					if (!self->m_input.IsKeyDown('Z'))
						self->EndCameraLook();
				}
				return 0;
			case WM_CAPTURECHANGED:
				if (reinterpret_cast<HWND>(lParam) != hwnd && self->m_cameraMouseCaptured) {
					self->m_cameraMouseCaptured = false;
					self->m_input.Clear();
				}
				break;
			case WM_CANCELMODE:
				self->m_input.Clear();
				self->EndCameraLook();
				break;
			case WM_SIZE:
				self->m_width = LOWORD(lParam);
				self->m_height = HIWORD(lParam);
				if (self->m_onResize)
					self->m_onResize(self->m_width, self->m_height);
				return 0;
			case WM_DESTROY:
				self->DeactivateInput();
				self->UnregisterRawInput();
				PostQuitMessage(0);
				return 0;
			case WM_NCDESTROY:
				self->m_hwnd = nullptr;
				SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
				break;
			}
		}
		return DefWindowProcW(hwnd, message, wParam, lParam);
	}
} // namespace Engine
