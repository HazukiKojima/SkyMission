#define UNICODE
#define NOMINMAX
#include <Windows.h>

// Win32だけで起動し、開発環境やVCランタイムを必要としない。
extern "C" void WINAPI LauncherEntry() {
	static wchar_t path[32768];
	const DWORD length = GetModuleFileNameW(nullptr, path, 32768);
	DWORD end = length;
	while (end && path[end - 1] != L'\\') --end;
	constexpr wchar_t suffix[] = L"Runtime\\SkyMission.exe";
	if (!length || length >= 32768 || end + sizeof(suffix) / sizeof(wchar_t) >= 32768)
		ExitProcess(1);
	for (DWORD i = 0; i < sizeof(suffix) / sizeof(wchar_t); ++i) path[end + i] = suffix[i];
	static STARTUPINFOW startup;
	startup.cb = sizeof(startup);
	static PROCESS_INFORMATION process;
	if (!CreateProcessW(path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
		MessageBoxW(nullptr, L"ゲームを起動できません。ZIPをすべて展開し、Runtimeフォルダーがあることを確認してください。",
			L"SkyMission", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}
	CloseHandle(process.hThread);
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD code = 1;
	GetExitCodeProcess(process.hProcess, &code);
	CloseHandle(process.hProcess);
	ExitProcess(code);
}
