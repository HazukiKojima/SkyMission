#pragma once

#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace Engine {
	inline constexpr std::uintmax_t MaxJsonFileSize = 16u * 1024u * 1024u;

	inline std::vector<char> ReadJsonFile(const std::filesystem::path& jsonPath)
	{
		std::error_code error;
		const std::uintmax_t fileSize = std::filesystem::file_size(jsonPath, error);
		if (error) {
			throw std::runtime_error("Failed to determine JSON file size.");
		}
		if (fileSize == 0 || fileSize > MaxJsonFileSize ||
			fileSize > static_cast<std::uintmax_t>((std::numeric_limits<int>::max)())) {
			throw std::runtime_error("JSON file is empty or exceeds the size limit.");
		}

		std::ifstream file(jsonPath, std::ios::binary);
		if (!file) {
			throw std::runtime_error("Failed to open JSON file.");
		}

		std::vector<char> json(static_cast<size_t>(fileSize));
		file.read(json.data(), static_cast<std::streamsize>(json.size()));
		if (!file || file.gcount() != static_cast<std::streamsize>(json.size())) {
			throw std::runtime_error("Failed to read JSON file.");
		}
		return json;
	}

	class RedoxModule {
	public:
		static FARPROC GetFunction(const char* entryPoint)
		{
			HMODULE module = GetModule();
			FARPROC function = ::GetProcAddress(module, entryPoint);
			if (!function) {
				throw std::runtime_error(
					std::string("Failed to find REDox entry point: ") + entryPoint);
			}
			return function;
		}

	private:
		static HMODULE GetModule()
		{
			static HMODULE module = [] {
				HMODULE loaded = ::LoadLibraryW(L"REDoxWrapper.dll");
				if (!loaded) {
					throw std::runtime_error("Failed to load REDoxWrapper.dll.");
				}
				return loaded;
			}();
			return module;
		}
	};
}
