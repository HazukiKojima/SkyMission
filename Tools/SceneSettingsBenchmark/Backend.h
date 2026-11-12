#pragma once
#include <Windows.h>
// Benchmark-only indirection: production sources and DLL are not modified.
FARPROC WINAPI SceneBenchmarkGetProcAddress(HMODULE module, LPCSTR name);
HMODULE WINAPI SceneBenchmarkLoadLibraryW(LPCWSTR name);
#define GetProcAddress SceneBenchmarkGetProcAddress
#define LoadLibraryW SceneBenchmarkLoadLibraryW
