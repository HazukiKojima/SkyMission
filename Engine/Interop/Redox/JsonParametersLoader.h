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
		explicit JsonParametersLoader(JsonDocumentCache& documents) : m_documents(&documents) {}

		template <typename T> T Load(const std::filesystem::path& jsonPath, const char* entryPoint) const {
			static_assert(std::is_trivially_copyable_v<T>);
			static_assert(sizeof(T) % sizeof(float) == 0);

			const std::vector<char> jsonStorage =
				m_files || m_documents ? std::vector<char>() : ReadJsonFile(jsonPath);
			const RedoxDocument transientDocument = m_documents
				? RedoxDocument()
				: RedoxDocument(m_files ? m_files->Read(jsonPath) : jsonStorage);
			const RedoxDocument& document = m_documents ? m_documents->Get(jsonPath) : transientDocument;

			using GetParameters = int(__cdecl*)(std::intptr_t, float*, int);
			const std::string documentEntryPoint = std::string(entryPoint) + "FromDocument";
			auto getParameters =
				reinterpret_cast<GetParameters>(RedoxModule::GetFunction(documentEntryPoint.c_str()));

			std::vector<float> values(sizeof(T) / sizeof(float));
			const int result =
				getParameters(document.GetHandle(), values.data(), static_cast<int>(values.size()));
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
		JsonDocumentCache* m_documents = nullptr;
	};
} // namespace Engine
