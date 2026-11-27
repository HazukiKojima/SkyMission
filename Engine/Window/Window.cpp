#include "Window.h"
#include "Engine/Diagnostics/Log.h"
#include <hidusage.h>
#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace Engine {
	namespace {
		constexpr int kLoadingCompletePercent = 100;
		constexpr UINT kRawInputError = static_cast<UINT>(-1);
	}

	Window::Window(UINT width, UINT height, const wchar_t* title, HINSTANCE hInstance)
		: m_width(width), m_height(height) {
		// ウィンドウクラスを登録する
		WNDCLASSW wc{};
		wc.lpfnWndProc = WindowProc;
		wc.hInstance = hInstance;
		wc.lpszClassName = L"DX12WindowClass";

		if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

		// 指定したクライアント領域を持つウィンドウを作る
		RECT rc{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
		m_hwnd = CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
			CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, this);
		if (!m_hwnd) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));

		// 前面時のみ受信し、ウィンドウ操作やAlt+F4用の従来メッセージは残す
		const RAWINPUTDEVICE devices[] = {
			{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_DEVNOTIFY, m_hwnd},
			{HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, RIDEV_DEVNOTIFY, m_hwnd}};
		if (!RegisterRawInputDevices(devices, _countof(devices), sizeof(RAWINPUTDEVICE))) {
			const HRESULT error = HRESULT_FROM_WIN32(GetLastError());
			DestroyWindow(m_hwnd);
			ThrowIfFailed(error);
		}

		m_input.SetActive(GetForegroundWindow() == m_hwnd);
	}

	Window::~Window() {
		m_onResize = {};
		if (m_hwnd && IsWindow(m_hwnd)) DestroyWindow(m_hwnd);
	}

	InputSnapshot Window::ConsumeInput() {
		if (GetForegroundWindow() != m_hwnd) DeactivateInput();
		auto snapshot = m_input.ConsumeFrame();
		if (m_timedInputEnabled) {
			m_inputSamples.push_back({std::chrono::steady_clock::now(), snapshot});
			if (snapshot.active) {
				snapshot.gamepad = m_gamepad.ConsumeFrame([this](auto time, const auto& sample) {
					m_inputSamples.push_back({time, sample});
				});
				auto held = snapshot.gamepad;
				held.presses = {};
				held.northTapCount = 0;
				held.northHoldStarted = false;
				m_inputSamples.push_back({std::chrono::steady_clock::now(), held});
			}
		} else if (snapshot.active) {
			snapshot.gamepad = m_gamepad.ConsumeFrame();
		}
		return snapshot;
	}

	void Window::SetTimedInputEnabled(bool enabled) {
		m_timedInputEnabled = enabled;
		m_inputSamples.clear();
		m_lastRawInputTime = std::chrono::steady_clock::now();
	}

	std::vector<TimedInputSample> Window::TakeInputSamples() {
		return std::exchange(m_inputSamples, {});
	}

	void Window::RecordRawInput() {
		if (!m_timedInputEnabled) return;
		// GetMessageTime はキューに入った時刻を返すため、描画中に届いた入力も元の時刻で扱える。
		const DWORD age = GetTickCount() - static_cast<DWORD>(GetMessageTime());
		const auto time = std::chrono::steady_clock::now() - std::chrono::milliseconds(age);
		m_lastRawInputTime = std::max(m_lastRawInputTime, time);
		m_inputSamples.push_back({m_lastRawInputTime, m_input.ConsumeFrame()});
	}

	void Window::SetGamepadRumble(float lowFrequency, float highFrequency) {
		m_gamepad.SetRumble(lowFrequency, highFrequency);
	}

	void Window::SetLoading(bool loading) {
		m_loading = loading;
		InvalidateRect(m_hwnd, nullptr, FALSE);

		// 読み込み中に受けた終了要求はワーカーの停止後に反映する
		if (!loading && m_closeRequested) DestroyWindow(m_hwnd);
	}

	void Window::SetLoadingProgress(int percent, std::wstring_view item) {
		m_loadingPercent = std::clamp(percent, 0, kLoadingCompletePercent);
		m_loadingItem = item;
		InvalidateRect(m_hwnd, nullptr, FALSE);
		UpdateWindow(m_hwnd);
	}

	void Window::DeactivateInput() {
		m_input.SetActive(false);
		m_gamepad.Reset();
		m_fullscreenEnterHeld = false;
		EndCameraLook();
	}

	void Window::ToggleFullscreen() {
		EndCameraLook();
		const UINT positioningFlags = SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED |
			(m_loading ? SWP_NOCOPYBITS : 0);

		// 元の配置を保存してモニター全体へ広げる
		if (!m_fullscreen) {
			MONITORINFO monitor{sizeof(MONITORINFO)};
			if (!GetMonitorInfoW(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &monitor) ||
				!GetWindowPlacement(m_hwnd, &m_windowedPlacement)) return;
			m_windowedStyle = GetWindowLongPtrW(m_hwnd, GWL_STYLE);
			m_fullscreen = true;
			SetWindowLongPtrW(m_hwnd, GWL_STYLE, m_windowedStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(m_hwnd, nullptr, monitor.rcMonitor.left, monitor.rcMonitor.top,
				monitor.rcMonitor.right - monitor.rcMonitor.left,
				monitor.rcMonitor.bottom - monitor.rcMonitor.top,
				positioningFlags);
		} else {
			m_fullscreen = false;
			SetWindowLongPtrW(m_hwnd, GWL_STYLE, m_windowedStyle);
			SetWindowPlacement(m_hwnd, &m_windowedPlacement);
			SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | positioningFlags);
		}

		// スタイルと配置の変更がすべて終わってから、新しいクライアント領域を描き直す
		if (m_loading && !IsIconic(m_hwnd))
			RedrawWindow(m_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
	}

	void Window::BeginCameraLook() {
		// 標準のウィンドウ移動・サイズ変更が所有するキャプチャは奪わない
		if (m_cameraMouseCaptured || GetCapture() != nullptr) return;

		SetCapture(m_hwnd);
		m_cameraMouseCaptured = GetCapture() == m_hwnd;
	}

	void Window::EndCameraLook() {
		if (!m_cameraMouseCaptured) return;

		m_cameraMouseCaptured = false;
		if (GetCapture() == m_hwnd) ReleaseCapture();
	}

	void Window::UnregisterRawInput() {
		// プロセス共通の入力登録を、別のWindowから解除しない
		UINT count = 0;
		if (GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) == kRawInputError) return;

		std::vector<RAWINPUTDEVICE> registered(count), remove;
		if (count &&
			GetRegisteredRawInputDevices(registered.data(), &count, sizeof(RAWINPUTDEVICE)) == kRawInputError)
			return;
		for (const auto& device : registered) {
			if (device.hwndTarget == m_hwnd && device.usUsagePage == HID_USAGE_PAGE_GENERIC &&
				(device.usUsage == HID_USAGE_GENERIC_KEYBOARD || device.usUsage == HID_USAGE_GENERIC_MOUSE))
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

		// 受信前にRaw Inputパケットのサイズを確認する
		UINT size = 0;
		if (GetRawInputData(handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == kRawInputError) return;
		if (size < sizeof(RAWINPUTHEADER)) return;

		// GetRawInputDataの受信領域をDWORD境界にそろえる
		std::vector<DWORD> storage((size + sizeof(DWORD) - 1) / sizeof(DWORD));
		const UINT bytes = GetRawInputData(handle, RID_INPUT, storage.data(), &size, sizeof(RAWINPUTHEADER));
		if (bytes == kRawInputError || bytes < sizeof(RAWINPUTHEADER)) {
			Log("Window: failed to read raw input packet ({})\n", GetLastError());
			return;
		}

		// パケットの種類とサイズを確認して入力を振り分ける
		const auto& raw = *reinterpret_cast<const RAWINPUT*>(storage.data());
		if (raw.header.dwType == RIM_TYPEKEYBOARD &&
			bytes >= offsetof(RAWINPUT, data) + sizeof(RAWKEYBOARD)) {
			// 全画面切り替えのEnterをゲーム側のスキップ操作に渡さない
			if (raw.data.keyboard.VKey == VK_RETURN) {
				const bool released = (raw.data.keyboard.Flags & RI_KEY_BREAK) != 0;

				if (m_fullscreenEnterHeld) {
					if (released) m_fullscreenEnterHeld = false;
					return;
				}

				if (!released && !m_input.IsKeyDown(VK_RETURN) &&
					(m_input.IsKeyDown(VK_LSHIFT) || m_input.IsKeyDown(VK_RSHIFT))) {
					m_fullscreenEnterHeld = true;
					ToggleFullscreen();
					return;
				}
			}

			m_input.ProcessKeyboard(raw.header.hDevice, raw.data.keyboard);
		} else if (raw.header.dwType == RIM_TYPEMOUSE &&
			bytes >= offsetof(RAWINPUT, data) + sizeof(RAWMOUSE)) {
			const bool virtualDesktop = (raw.data.mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
			m_input.ProcessMouse(raw.header.hDevice, raw.data.mouse,
				GetSystemMetrics(virtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN),
				GetSystemMetrics(virtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN));
			if (m_input.IsMiddleMouseDown())
				BeginCameraLook();
			else
				EndCameraLook();
		}
	}

	LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
		// 作成時にインスタンスを関連付け、以後のメッセージを振り分ける
		auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (message == WM_NCCREATE) {
			self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
			self->m_hwnd = hwnd;
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}

		if (self) {
			switch (message) {
			case WM_NCCALCSIZE:
				if (self->m_loading && wParam) {
					DefWindowProcW(hwnd, message, wParam, lParam);
					// 最大化や復元でも、旧クライアント領域に描いた文字をコピーさせない
					return WVR_REDRAW;
				}
				break;
			case WM_CLOSE:
				// 読み込み中の終了要求はワーカーの完了後に処理する
				if (self->m_loading) {
					self->m_closeRequested = true;
					return 0;
				}
				break;
			case WM_PAINT:
				if (self->m_loading) {
					constexpr LONG kTrackWidthDivisor = 3;
					constexpr LONG kMaxTrackHalfWidth = 220;
					constexpr LONG kTrackHalfHeight = 5;
					constexpr LONG kHeadingTopOffset = -46;
					constexpr LONG kHeadingBottomOffset = -18;
					constexpr LONG kDetailSidePadding = 8;
					constexpr LONG kDetailTopOffset = 14;
					constexpr LONG kDetailBottomOffset = 40;
					PAINTSTRUCT paint{};
					const HDC dc = BeginPaint(hwnd, &paint);
					RECT area{};
					GetClientRect(hwnd, &area);
					FillRect(dc, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
					SetBkMode(dc, TRANSPARENT);
					SetTextColor(dc, RGB(230, 230, 230));
					const LONG centerX = (area.left + area.right) / 2;
					const LONG centerY = (area.top + area.bottom) / 2;
					const LONG halfWidth = std::clamp(
						(area.right - area.left) / kTrackWidthDivisor, 0L, kMaxTrackHalfWidth);
					RECT heading{area.left, centerY + kHeadingTopOffset, area.right, centerY + kHeadingBottomOffset};
					const std::wstring title = L"Loading SkyMission...  " +
						std::to_wstring(self->m_loadingPercent) + L"%";
					DrawTextW(dc, title.c_str(), -1, &heading, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

					RECT track{centerX - halfWidth, centerY - kTrackHalfHeight,
						centerX + halfWidth, centerY + kTrackHalfHeight};
					const auto brush = static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
					SetDCBrushColor(dc, RGB(50, 55, 61));
					FillRect(dc, &track, brush);
					track.right = track.left +
						(track.right - track.left) * self->m_loadingPercent / kLoadingCompletePercent;
					SetDCBrushColor(dc, RGB(255, 255, 255));
					FillRect(dc, &track, brush);

					RECT detail{area.left + kDetailSidePadding, centerY + kDetailTopOffset,
						area.right - kDetailSidePadding, centerY + kDetailBottomOffset};
					DrawTextW(dc, self->m_loadingItem.c_str(), -1, &detail,
						DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
					EndPaint(hwnd, &paint);
					return 0;
				}
				break;
			case WM_MBUTTONDOWN:
				self->BeginCameraLook();
				return 0;
			case WM_MBUTTONUP:
				if (!self->m_input.IsMiddleMouseDown()) self->EndCameraLook();
				return 0;
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_CONTEXTMENU:
				// 発射はRaw Inputの押下エッジで処理し、カメラのキャプチャには使わない
				return 0;
			case WM_INPUT:
				if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT) {
					self->ProcessRawInput(reinterpret_cast<HRAWINPUT>(lParam));
					self->RecordRawInput();
				}
				// 前面のWM_INPUTは後始末のためDefWindowProcへ渡す
				return DefWindowProcW(hwnd, message, wParam, lParam);
			case WM_SETFOCUS:
				self->m_input.SetActive(true);
				self->RecordRawInput();
				break;
			case WM_KILLFOCUS:
				self->DeactivateInput();
				self->RecordRawInput();
				break;
			case WM_ACTIVATEAPP:
				if (!wParam) {
					self->DeactivateInput();
					self->RecordRawInput();
				}
				break;
			case WM_INPUT_DEVICE_CHANGE:
				if (wParam == GIDC_REMOVAL) {
					self->m_input.ForgetDevice(reinterpret_cast<HANDLE>(lParam));

					if (!self->m_input.IsMiddleMouseDown()) self->EndCameraLook();
					self->RecordRawInput();
				}
				return 0;
			case WM_CAPTURECHANGED:
				if (reinterpret_cast<HWND>(lParam) != hwnd && self->m_cameraMouseCaptured) {
					self->m_cameraMouseCaptured = false;
					self->m_input.ClearMouse();
					self->RecordRawInput();
				}
				break;
			case WM_CANCELMODE:
				self->m_input.Clear();
				self->EndCameraLook();
				self->RecordRawInput();
				break;
			case WM_SIZE:
				self->m_width = LOWORD(lParam);
				self->m_height = HIWORD(lParam);

				// Loadingの旧描画位置も消せるよう、拡張された部分だけでなく全体を再描画する
				if (self->m_loading && wParam != SIZE_MINIMIZED) InvalidateRect(hwnd, nullptr, FALSE);

				if (self->m_onResize) self->m_onResize(self->m_width, self->m_height);
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
