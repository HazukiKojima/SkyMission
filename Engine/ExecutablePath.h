#pragma once
#include "EngineCommon.h"
#include <filesystem>
#include <vector>

namespace Engine {
	inline std::filesystem::path ExecutableDirectory() {
		std::vector<wchar_t> buffer(512);
		for (;;) {
			const DWORD count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
			if (count == 0)
				ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			if (count < buffer.size())
				return std::filesystem::path(std::wstring(buffer.data(), count)).parent_path();
			buffer.resize(buffer.size() * 2);
		}
	}
} // namespace Engine
