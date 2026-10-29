#pragma once

#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace Engine {
	inline constexpr std::uintmax_t MaxJsonFileSize = 16u * 1024u * 1024u;

	inline std::vector<char> ReadJsonFile(const std::filesystem::path& jsonPath) {
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

	class JsonFileCache {
	  public:
		const std::vector<char>& Read(const std::filesystem::path& jsonPath) {
			auto found = m_files.find(jsonPath);
			if (found == m_files.end()) {
				found = m_files.emplace(jsonPath, ReadJsonFile(jsonPath)).first;
			}
			return found->second;
		}

	  private:
		std::map<std::filesystem::path, std::vector<char>> m_files;
	};

	class RedoxModule {
	  public:
		static FARPROC GetFunction(const char* entryPoint) {
			HMODULE module = GetModule();
			FARPROC function = ::GetProcAddress(module, entryPoint);
			if (!function) {
				throw std::runtime_error(std::string("Failed to find REDox entry point: ") + entryPoint);
			}
			return function;
		}

	  private:
		static HMODULE GetModule() {
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

	// NativeAOT側の文書ハンドルを所有する移動専用型
	// DLLはプロセス終了まで保持され、ハンドルが先に解放される
	class RedoxDocument {
	  public:
		RedoxDocument() = default;
		explicit RedoxDocument(const std::vector<char>& json) {
			using CreateDocument = int(__cdecl*)(const unsigned char*, int, std::intptr_t*);
			auto createDocument =
				reinterpret_cast<CreateDocument>(RedoxModule::GetFunction("MyRedoxCreateDocument"));
			const int result = createDocument(reinterpret_cast<const unsigned char*>(json.data()),
				static_cast<int>(json.size()), &m_handle);
			if (result != 0 || m_handle == 0) {
				m_handle = 0;
				throw std::runtime_error(
					"Failed to parse JSON document. REDox error code: " + std::to_string(result));
			}
		}

		~RedoxDocument() {
			Reset();
		}
		RedoxDocument(const RedoxDocument&) = delete;
		RedoxDocument& operator=(const RedoxDocument&) = delete;

		RedoxDocument(RedoxDocument&& other) noexcept : m_handle(other.m_handle) {
			other.m_handle = 0;
		}

		RedoxDocument& operator=(RedoxDocument&& other) noexcept {
			if (this != &other) {
				Reset();
				m_handle = other.m_handle;
				other.m_handle = 0;
			}
			return *this;
		}

		std::intptr_t GetHandle() const {
			return m_handle;
		}

	  private:
		void Reset() noexcept {
			if (m_handle == 0) {
				return;
			}

			try {
				using ReleaseDocument = int(__cdecl*)(std::intptr_t);
				const auto releaseDocument =
					reinterpret_cast<ReleaseDocument>(RedoxModule::GetFunction("MyRedoxReleaseDocument"));
				releaseDocument(m_handle);
			} catch (...) {
				// デストラクタでは例外を送出せず、DLLを保持する
			}
			m_handle = 0;
		}

		std::intptr_t m_handle = 0;
	};

	// 設定読み込み単位のキャッシュ（キーは絶対・字句正規化済みパス）
	// 途中のファイル変更は反映せず、破棄時に全ハンドルを解放する
	class JsonDocumentCache {
	  public:
		const RedoxDocument& Get(const std::filesystem::path& jsonPath) {
			const std::filesystem::path key = Normalize(jsonPath);
			auto found = m_documents.find(key);
			if (found == m_documents.end()) {
				const std::vector<char>& json = m_files.Read(key);
				found = m_documents.emplace(key, RedoxDocument(json)).first;
			}
			return found->second;
		}

	  private:
		static std::filesystem::path Normalize(const std::filesystem::path& path) {
			std::error_code error;
			const std::filesystem::path absolute = std::filesystem::absolute(path, error);
			return (error ? path : absolute).lexically_normal();
		}

		JsonFileCache m_files;
		std::map<std::filesystem::path, RedoxDocument> m_documents;
	};
} // namespace Engine
