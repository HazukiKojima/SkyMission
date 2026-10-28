#pragma once

#include "RedoxBridge.h"
#include <cstring>
#include <type_traits>
#include <vector>

namespace Engine {
	class JsonParametersLoader {
	public:
		JsonParametersLoader() = default;
		explicit JsonParametersLoader(JsonFileCache& files) : m_files(&files) {}

		template <typename T>
		T Load(const std::filesystem::path& jsonPath, const char* entryPoint) const
		{
			static_assert(std::is_trivially_copyable_v<T>);
			static_assert(sizeof(T) % sizeof(float) == 0);

			const std::vector<char> jsonStorage = m_files ? std::vector<char>() : ReadJsonFile(jsonPath);
			const std::vector<char>& json = m_files ? m_files->Read(jsonPath) : jsonStorage;

			using GetParameters = int(__cdecl*)(const unsigned char*, int, float*, int);
			auto getParameters = reinterpret_cast<GetParameters>(RedoxModule::GetFunction(entryPoint));

			std::vector<float> values(sizeof(T) / sizeof(float));
			const int result = getParameters(reinterpret_cast<const unsigned char*>(json.data()),
				static_cast<int>(json.size()), values.data(), static_cast<int>(values.size()));
			if (result != static_cast<int>(values.size())) {
				throw std::runtime_error(
					"Failed to deserialize JSON parameters. REDox error code: " + std::to_string(result));
			}

			T parameters{};
			std::memcpy(&parameters, values.data(), sizeof(parameters));
			return parameters;
		}

	private:
		JsonFileCache* m_files = nullptr;
	};
}
