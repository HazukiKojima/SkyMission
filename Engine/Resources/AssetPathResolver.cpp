#include "AssetPathResolver.h"
#include "../Interop/Redox/RedoxBridge.h"
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
		using RedoxGetFloatArray = int(__cdecl*)(
			const unsigned char*,
			int,
			const unsigned char*,
			int,
			float*,
			int);
	}

	std::filesystem::path AssetPathResolver::Resolve(
		const std::filesystem::path& jsonPath,
		const std::string& key) const
	{
		const std::vector<char> jsonStorage = m_files ? std::vector<char>() : ReadJsonFile(jsonPath);
		const std::vector<char>& json = m_files ? m_files->Read(jsonPath) : jsonStorage;

		auto getString = reinterpret_cast<RedoxGetString>(
			RedoxModule::GetFunction("MyRedoxGetString"));

		unsigned char output[4096] = {};
		const int result = getString(
			reinterpret_cast<const unsigned char*>(json.data()),
			static_cast<int>(json.size()),
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
		const std::vector<char> jsonStorage = m_files ? std::vector<char>() : ReadJsonFile(jsonPath);
		const std::vector<char>& json = m_files ? m_files->Read(jsonPath) : jsonStorage;

		auto getFloatArray = reinterpret_cast<RedoxGetFloatArray>(
			RedoxModule::GetFunction("MyRedoxGetFloatArray"));

		std::vector<float> output(3);
		const int result = getFloatArray(
			reinterpret_cast<const unsigned char*>(json.data()),
			static_cast<int>(json.size()),
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
