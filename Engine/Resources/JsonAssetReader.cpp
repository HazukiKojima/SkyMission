#include "JsonAssetReader.h"
#include "../Interop/Redox/RedoxBridge.h"
#include <stdexcept>
#include <vector>

namespace Engine {
	namespace {
		// RE:DOX ABI の InvalidSchema は未指定フィールドでも返る。
		constexpr int kRedoxInvalidSchema = -3;
		constexpr int kStringBufferBytes = 4096;
		constexpr size_t kMaxVectorComponents = 3;
	}

	std::optional<size_t> JsonAssetReader::TryResolveArrayLength(
		const std::filesystem::path& path, const std::string& key) const {
		RedoxDocument transient;
		const auto& document = AcquireDocument(path, transient);
		using GetLength = int(__cdecl*)(std::intptr_t, const unsigned char*, int);

		const auto getLength =
			reinterpret_cast<GetLength>(RedoxModule::GetFunction("MyRedoxGetArrayLengthFromDocument"));
		const int result = getLength(document.GetHandle(), reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<int>(key.size()));
		if (result == kRedoxInvalidSchema) return std::nullopt;
		if (result < 0)
			throw std::runtime_error("JSON '" + path.string() + "', field '" + key +
				"': expected an array (REDox " + std::to_string(result) + ")");
		return static_cast<size_t>(result);
	}

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
		const auto result = TryResolveString(jsonPath, key);
		if (!result) throw std::runtime_error(FieldContext(jsonPath, key) + ": missing field");
		return *result;
	}

	std::optional<std::string> JsonAssetReader::TryResolveString(
		const std::filesystem::path& jsonPath, const std::string& key) const {
		RedoxDocument transientDocument;
		const RedoxDocument& document = AcquireDocument(jsonPath, transientDocument);

		auto getString =
			reinterpret_cast<RedoxGetString>(RedoxModule::GetFunction("MyRedoxGetStringFromDocument"));

		unsigned char output[kStringBufferBytes] = {};
		const int result = getString(document.GetHandle(), reinterpret_cast<const unsigned char*>(key.data()),
			static_cast<int>(key.size()), output, sizeof(output));
		if (result == kRedoxInvalidSchema) return std::nullopt;
		if (result < 0) {
			throw std::runtime_error(FieldContext(jsonPath, key) +
				": failed to read string. REDox error code: " + std::to_string(result));
		}

		return std::string(reinterpret_cast<const char*>(output), static_cast<size_t>(result));
	}

	const RedoxDocument& JsonAssetReader::AcquireDocument(
		const std::filesystem::path& path, RedoxDocument& transient) const {
		// 文書キャッシュがなければ呼び出し中だけ文書を保持する
		if (m_documents) return m_documents->Get(path);
		if (m_files)
			transient = RedoxDocument(m_files->Read(path));
		else
			transient = RedoxDocument(ReadJsonFile(path));
		return transient;
	}

	std::filesystem::path JsonAssetReader::Resolve(
		const std::filesystem::path& path, const std::string& key) const {
		// 相対参照はJSONファイルの場所を基準に解決する
		const std::string value = ResolveString(path, key);
		return path.parent_path() /
			std::filesystem::path(
				std::u8string(reinterpret_cast<const char8_t*>(value.data()), value.size()));
	}

	std::vector<float> JsonAssetReader::ResolveFloatArray(
		const std::filesystem::path& jsonPath, const std::string& key) const {
		const auto result = TryResolveFloatArray(jsonPath, key);
		if (!result) throw std::runtime_error(FieldContext(jsonPath, key) + ": missing field");
		return *result;
	}

	std::optional<std::vector<float>> JsonAssetReader::TryResolveFloatArray(
		const std::filesystem::path& jsonPath, const std::string& key) const {
		RedoxDocument transientDocument;
		const RedoxDocument& document = AcquireDocument(jsonPath, transientDocument);

		auto getFloatArray = reinterpret_cast<RedoxGetFloatArray>(
			RedoxModule::GetFunction("MyRedoxGetFloatArrayFromDocument"));

		// 数値1個または最大3成分のベクトルを受け取る
		std::vector<float> output(kMaxVectorComponents);
		const int result =
			getFloatArray(document.GetHandle(), reinterpret_cast<const unsigned char*>(key.data()),
				static_cast<int>(key.size()), output.data(), static_cast<int>(output.size()));
		if (result == kRedoxInvalidSchema) return std::nullopt;
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
