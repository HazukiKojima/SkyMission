#include "SceneLoader.h"
#include <Windows.h>
#include <iostream>

using MyRedoxParseFn = int(__cdecl*)(const unsigned char*, int);

void TestREDox()
{
	OutputDebugStringA("TestREDox called\n");

	HMODULE module = LoadLibraryW(L"REDoxWrapper.dll");
	if (module == nullptr)
	{
		OutputDebugStringA("Failed to load REDoxWrapper.dll\n");
		return;
	}

	auto parseJson = reinterpret_cast<MyRedoxParseFn>(
		GetProcAddress(module, "MyRedoxParse"));

	if (parseJson == nullptr)
	{
		OutputDebugStringA("Failed to find MyRedoxParse\n");
		FreeLibrary(module);
		return;
	}

	const unsigned char json[] = R"({"name":"SkyMission"})";
	const int result = parseJson(json, sizeof(json) - 1);

	char message[128];
	sprintf_s(message, "REDox parse result: %d\n", result);
	OutputDebugStringA(message);

	FreeLibrary(module);
}