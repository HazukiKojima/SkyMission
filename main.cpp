#include <Windows.h>
#include <exception>

#ifdef REDOX_JSON_BENCHMARK
#include "Engine/Core/JsonBenchmark.h"
#else
#include "Engine/Core/Application.h"
#endif

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine, _In_ int nCmdShow) {
	try {
	#ifdef REDOX_JSON_BENCHMARK
		return Engine::RunJsonBenchmark();
	#else
		Engine::Application app(hInstance);
		app.Initialize();
		return app.Run();
	#endif
	}
	catch (const std::exception& e) {
		MessageBoxA(nullptr, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
		return -1;
	}
}