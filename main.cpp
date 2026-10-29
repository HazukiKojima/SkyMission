#include "Game/Application/Application.h"
#include <string>

namespace {
	std::wstring ToWide(const char* text) {
		const int length = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
		if (length <= 0)
			return L"Unknown error.";

		std::wstring result(static_cast<size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), length);
		result.resize(static_cast<size_t>(length - 1));
		return result;
	}
} // namespace

int WINAPI WinMain(
	_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	try {
		Game::Application app(hInstance);
		app.Initialize();
		return app.Run();
	} catch (const std::exception& e) {
		MessageBoxW(nullptr, ToWide(e.what()).c_str(), L"Fatal Error", MB_OK | MB_ICONERROR);
		return -1;
	}
}