#include "Engine/Core/Application.h"

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	try {
		Engine::Application app(hInstance);
		app.Initialize();
		return app.Run();
	}
	catch (const std::exception& e) {
		MessageBoxA(nullptr, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
		return -1;
	}
}