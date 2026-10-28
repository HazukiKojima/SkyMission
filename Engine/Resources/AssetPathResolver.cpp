#include "AssetPathResolver.h"
#include "../Interop/Redox/RedoxBridge.h"
#include <stdexcept>
#include <vector>

namespace Engine {
	namespace {
		using RedoxGetString = int(__cdecl*)(
			std::intptr_t,
			const unsigned char*,
			int,
			unsigned char*,
			int);
		using RedoxGetFloatArray = int(__cdecl*)(
			std::intptr_t,
			const unsigned char*,
			int,
			float*,
			int);
	}

	std::filesystem::path AssetPathResolver::Resolve(
		const std::filesystem::path& jsonPath,
		const std::string& key) const
	{
		const std::vector<char> jsonStorage = m_files || m_documents ? std::vector<char>() : ReadJsonFile(jsonPath);
		const RedoxDocument transientDocument = m_documents
			? RedoxDocument()
			: RedoxDocument(m_files ? m_files->Read(jsonPath) : jsonStorage);
		const RedoxDocument& document = m_documents ? m_documents->Get(jsonPath) : transientDocument;

		auto getString = reinterpret_cast<RedoxGetString>(
			RedoxModule::GetFunction("MyRedoxGetStringFromDocument"));

		unsigned char output[4096] = {};
		const int result = getString(document.GetHandle(),
			reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<int>(key.size()),
			output,
			sizeof(output));
		if (result < 0) {
			throw std::runtime_error(
				"Failed to read asset path from JSON. REDox error code: " + std::to_string(result));
		}

		const std::u8string relativePath(
			reinterpret_cast<const char8_t*>(output),
			static_cast<size_t>(result));
		return jsonPath.parent_path() / std::filesystem::path(relativePath);
	}

	std::vector<float> AssetPathResolver::ResolveFloatArray(
		const std::filesystem::path& jsonPath,
		const std::string& key) const
	{
		const std::vector<char> jsonStorage = m_files || m_documents ? std::vector<char>() : ReadJsonFile(jsonPath);
		const RedoxDocument transientDocument = m_documents
			? RedoxDocument()
			: RedoxDocument(m_files ? m_files->Read(jsonPath) : jsonStorage);
		const RedoxDocument& document = m_documents ? m_documents->Get(jsonPath) : transientDocument;

		auto getFloatArray = reinterpret_cast<RedoxGetFloatArray>(
			RedoxModule::GetFunction("MyRedoxGetFloatArrayFromDocument"));

		std::vector<float> output(3);
		const int result = getFloatArray(document.GetHandle(),
			reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<int>(key.size()),
			output.data(),
			static_cast<int>(output.size()));
		if (result < 0) {
			throw std::runtime_error(
				"Failed to read numeric value from JSON. REDox error code: " + std::to_string(result));
		}

		output.resize(static_cast<size_t>(result));
		return output;
	}

	float AssetPathResolver::ResolveFloat(
		const std::filesystem::path& jsonPath,
		const std::string& key) const
	{
		const std::vector<float> values = ResolveFloatArray(jsonPath, key);
		if (values.size() != 1) {
			throw std::runtime_error("JSON value is not a number.");
		}
		return values.front();
	}
}
