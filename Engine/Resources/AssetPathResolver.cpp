#include "AssetPathResolver.h"
#include <Windows.h>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace Engine {
	namespace {
		using RedoxGetString = int(__cdecl*)(
			const unsigned char*,
			int,
			const unsigned char*,
			int,
			unsigned char*,
			int);
	}

	std::filesystem::path AssetPathResolver::Resolve(
		const std::filesystem::path& jsonPath,
		const std::string& key) const
	{
		std::ifstream file(jsonPath, std::ios::binary);
		if (!file) {
			throw std::runtime_error("Failed to open asset path JSON.");
		}

		std::vector<char> json(
			(std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		if (json.empty()) {
			throw std::runtime_error("Asset path JSON is empty.");
		}

		HMODULE module = LoadLibraryW(L"REDoxWrapper.dll");
		if (!module) {
			throw std::runtime_error("Failed to load REDoxWrapper.dll.");
		}

		auto getString = reinterpret_cast<RedoxGetString>(
			GetProcAddress(module, "MyRedoxGetString"));
		if (!getString) {
			FreeLibrary(module);
			throw std::runtime_error("Failed to find MyRedoxGetString.");
		}

		unsigned char output[4096] = {};
		const int result = getString(
			reinterpret_cast<const unsigned char*>(json.data()),
			static_cast<int>(json.size()),
			reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<int>(key.size()),
			output,
			sizeof(output));
		FreeLibrary(module);

		if (result < 0) {
			throw std::runtime_error("Failed to read asset path from JSON.");
		}

		const std::u8string relativePath(
			reinterpret_cast<const char8_t*>(output),
			static_cast<size_t>(result));
		return jsonPath.parent_path() / std::filesystem::path(relativePath);
	}
}
