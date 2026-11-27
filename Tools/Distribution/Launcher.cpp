#define UNICODE
#define NOMINMAX
#include <Windows.h>

// Win32だけで起動し、開発環境やVCランタイムを必要としない
extern "C" void WINAPI LauncherEntry() {
	constexpr DWORD pathCapacity = 32768;
	static wchar_t path[pathCapacity];
	const DWORD length = GetModuleFileNameW(nullptr, path, pathCapacity);
	if (!length || length >= pathCapacity) ExitProcess(1);
	DWORD end = length;

	while (end && path[end - 1] != L'\\') --end;

	// ランチャーと同じ場所のRuntime内にあるゲームを起動する
	constexpr wchar_t suffix[] = L"Runtime\\SkyMission.exe";
	constexpr DWORD suffixLengthWithNull = sizeof(suffix) / sizeof(suffix[0]);

	if (suffixLengthWithNull > pathCapacity - end) ExitProcess(1);
	for (DWORD i = 0; i < suffixLengthWithNull; ++i) path[end + i] = suffix[i];
	static STARTUPINFOW startup;
	startup.cb = sizeof(startup);
	static PROCESS_INFORMATION process;

	if (!CreateProcessW(path, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
		MessageBoxW(nullptr, L"ゲームを起動できません。ZIPをすべて展開し、Runtimeフォルダーがあることを確認してください。",
			L"SkyMission", MB_OK | MB_ICONERROR);
		ExitProcess(1);
	}

	CloseHandle(process.hThread);

	// ゲームの終了コードをそのままランチャーの終了コードにする
	WaitForSingleObject(process.hProcess, INFINITE);
	DWORD code = 1;
	GetExitCodeProcess(process.hProcess, &code);
	CloseHandle(process.hProcess);
	ExitProcess(code);
}
