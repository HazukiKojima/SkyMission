#pragma once

#include <Windows.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace Engine {
	class JsonParametersLoader {
	public:
		template <typename T>
		T Load(const std::filesystem::path& jsonPath, const char* entryPoint) const
		{
			std::ifstream file(jsonPath, std::ios::binary);
			if (!file) throw std::runtime_error("Failed to open JSON parameters file.");

			std::vector<char> json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			if (json.empty() || json.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
				throw std::runtime_error("JSON parameters file is empty or too large.");
			}

			using GetParameters = int(__cdecl*)(const unsigned char*, int, float*, int);
			HMODULE module = LoadLibraryW(L"REDoxWrapper.dll");
			if (!module) throw std::runtime_error("Failed to load REDoxWrapper.dll.");
			auto getParameters = reinterpret_cast<GetParameters>(GetProcAddress(module, entryPoint));
			if (!getParameters) {
				FreeLibrary(module);
				throw std::runtime_error("Failed to find JSON parameters entry point.");
			}

			std::vector<float> values(sizeof(T) / sizeof(float));
			const int result = getParameters(reinterpret_cast<const unsigned char*>(json.data()),
				static_cast<int>(json.size()), values.data(), static_cast<int>(values.size()));
			FreeLibrary(module);
			if (result != static_cast<int>(values.size())) throw std::runtime_error("Failed to deserialize JSON parameters.");

			T parameters{};
			std::memcpy(&parameters, values.data(), sizeof(parameters));
			return parameters;
		}
	};
}
