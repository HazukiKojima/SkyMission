#include "SceneLoader.h"
#include <Windows.h>
#include <iostream>

using MyRedoxGetStringFn = int(__cdecl*)(
	const unsigned char*, int,
	const unsigned char*, int,
	unsigned char*, int);

void TestREDox()
{
	OutputDebugStringA("TestREDox called\n");

	HMODULE module = LoadLibraryW(L"REDoxWrapper.dll");
	if (module == nullptr)
	{
		OutputDebugStringA("Failed to load REDoxWrapper.dll\n");
		return;
	}

	auto getString = reinterpret_cast<MyRedoxGetStringFn>(
		GetProcAddress(module, "MyRedoxGetString"));

	if (getString == nullptr)
	{
		OutputDebugStringA("Failed to find MyRedoxGetString\n");
		FreeLibrary(module);
		return;
	}

	const unsigned char json[] = R"({"name":"SkyMission"})";
	const unsigned char key[] = "name";
	unsigned char output[128]{};

	const int result = getString(
		json, sizeof(json) - 1,
		key, sizeof(key) - 1,
		output, sizeof(output));

	char message[256];
	if (result >= 0)
	{
		sprintf_s(message, "REDox result: %.*s\n", result, output);
	}
	else
	{
		sprintf_s(message, "REDox result: %d\n", result);
	}
	OutputDebugStringA(message);

	FreeLibrary(module);
}