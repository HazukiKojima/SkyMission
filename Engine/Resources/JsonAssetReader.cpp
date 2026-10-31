#include "JsonAssetReader.h"
#include "../Interop/Redox/RedoxBridge.h"
#include <stdexcept>
#include <vector>

namespace Engine {
	namespace {
		using RedoxGetString = int(__cdecl*)(std::intptr_t, const unsigned char*, int, unsigned char*, int);
		using RedoxGetFloatArray = int(__cdecl*)(std::intptr_t, const unsigned char*, int, float*, int);
		std::string FieldContext(const std::filesystem::path& path, const std::string& key) {
			const auto utf8 = path.u8string();
			return "JSON '" + std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size()) +
				"', field '" + key + "'";
		}
	} // namespace

	std::string JsonAssetReader::ResolveString(
		const std::filesystem::path& jsonPath, const std::string& key) const {
		RedoxDocument transientDocument;
		const RedoxDocument& document = AcquireDocument(jsonPath, transientDocument);

		auto getString =
			reinterpret_cast<RedoxGetString>(RedoxModule::GetFunction("MyRedoxGetStringFromDocument"));

		unsigned char output[4096] = {};
		const int result = getString(document.GetHandle(), reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<int>(key.size()), output, sizeof(output));
		if (result < 0) {
			throw std::runtime_error(FieldContext(jsonPath, key) +
				": failed to read string. REDox error code: " + std::to_string(result));
		}

		return std::string(reinterpret_cast<const char*>(output), static_cast<size_t>(result));
	}

	const RedoxDocument& JsonAssetReader::AcquireDocument(
		const std::filesystem::path& path, RedoxDocument& transient) const {
		if (m_documents)
			return m_documents->Get(path);
		if (m_files)
			transient = RedoxDocument(m_files->Read(path));
		else
			transient = RedoxDocument(ReadJsonFile(path));
		return transient;
	}

	std::filesystem::path JsonAssetReader::Resolve(
		const std::filesystem::path& path, const std::string& key) const {
		const std::string value = ResolveString(path, key);
		return path.parent_path() /
			std::filesystem::path(
				std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
	}

	std::vector<float> JsonAssetReader::ResolveFloatArray(
		const std::filesystem::path& jsonPath, const std::string& key) const {
		RedoxDocument transientDocument;
		const RedoxDocument& document = AcquireDocument(jsonPath, transientDocument);

		auto getFloatArray = reinterpret_cast<RedoxGetFloatArray>(
			RedoxModule::GetFunction("MyRedoxGetFloatArrayFromDocument"));

		std::vector<float> output(3);
		const int result =
			getFloatArray(document.GetHandle(), reinterpret_cast<const unsigned char*>(key.data()),
				static_cast<int>(key.size()), output.data(), static_cast<int>(output.size()));
		if (result < 0) {
			throw std::runtime_error(FieldContext(jsonPath, key) +
				": failed to read number. REDox error code: " + std::to_string(result));
		}

		output.resize(static_cast<size_t>(result));
		return output;
	}

	float JsonAssetReader::ResolveFloat(const std::filesystem::path& jsonPath, const std::string& key) const {
		const std::vector<float> values = ResolveFloatArray(jsonPath, key);
		if (values.size() != 1) {
			throw std::runtime_error(FieldContext(jsonPath, key) + ": value is not a number.");
		}
		return values.front();
	}
} // namespace Engine
