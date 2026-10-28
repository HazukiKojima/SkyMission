#pragma once
#include <Windows.h>
#include <format>
#include <string>
#include <utility>

namespace Engine {
	template <class... Args> void Log(std::format_string<Args...> format, Args&&... args) {
		const std::string message = std::format(format, std::forward<Args>(args)...);
		OutputDebugStringA(message.c_str());
	}
	template <class... Args> void Log(std::wformat_string<Args...> format, Args&&... args) {
		const std::wstring message = std::format(format, std::forward<Args>(args)...);
		OutputDebugStringW(message.c_str());
	}
} // namespace Engine
