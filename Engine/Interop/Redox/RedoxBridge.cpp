#include "RedoxBridge.h"

namespace Engine {
	RedoxDocument::RedoxDocument(const std::vector<char>& json) {
		using CreateDocument = int(__cdecl*)(const unsigned char*, int, std::intptr_t*);
		auto createDocument =
			reinterpret_cast<CreateDocument>(RedoxModule::GetFunction("MyRedoxCreateDocument"));
		const int result = createDocument(
			reinterpret_cast<const unsigned char*>(json.data()), static_cast<int>(json.size()), &m_handle);
		if (result != 0 || m_handle == 0) {
			m_handle = 0;
			throw std::runtime_error(
				"Failed to parse JSON document. REDox error code: " + std::to_string(result));
		}
	}

	std::vector<char> ReadJsonFile(const std::filesystem::path& jsonPath) {
		// JSONのサイズを検証する
		std::error_code error;
		const std::uintmax_t fileSize = std::filesystem::file_size(jsonPath, error);
		if (error) {
			throw std::runtime_error("Failed to determine JSON file size.");
		}

		if (fileSize == 0 || fileSize > MaxJsonFileSize ||
			fileSize > static_cast<std::uintmax_t>((std::numeric_limits<int>::max)())) {
			throw std::runtime_error("JSON file is empty or exceeds the size limit.");
		}

		// JSONの全データを読み込む
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

	const std::vector<char>& JsonFileCache::Read(const std::filesystem::path& jsonPath) {
		auto found = m_files.find(jsonPath);
		if (found == m_files.end()) {
			found = m_files.emplace(jsonPath, ReadJsonFile(jsonPath)).first;
		}
		return found->second;
	}

	FARPROC RedoxModule::GetFunction(const char* entryPoint) {
		HMODULE module = GetModule();
		FARPROC function = ::GetProcAddress(module, entryPoint);

		if (!function) {
			throw std::runtime_error(std::string("Failed to find REDox entry point: ") + entryPoint);
		}
		return function;
	}

	HMODULE RedoxModule::GetModule() {
		// DLLは一度だけ読み込み、文書より長く保持する
		static HMODULE module = [] {
			HMODULE loaded = ::LoadLibraryW(L"REDoxWrapper.dll");
			if (!loaded) {
				throw std::runtime_error("Failed to load REDoxWrapper.dll.");
			}
			return loaded;
		}();

		return module;
	}

	void RedoxDocument::Reset() noexcept {
		if (m_handle == 0) return;

		try {
			using ReleaseDocument = int(__cdecl*)(std::intptr_t);
			const auto releaseDocument =
				reinterpret_cast<ReleaseDocument>(RedoxModule::GetFunction("MyRedoxReleaseDocument"));
			releaseDocument(m_handle);
		} catch (...) {
			// 解放中の例外をデストラクタの外へ出さない
		}

		m_handle = 0;
	}

	const RedoxDocument& JsonDocumentCache::Get(const std::filesystem::path& jsonPath) {
		// 多くの設定は、同じ絶対パスから複数の項目を続けて読む
		// 相対パスは毎回解決し、作業ディレクトリの変更を反映する
		if (jsonPath.is_absolute() && m_lastDocument && jsonPath == m_lastPath) return *m_lastDocument;

		// 正規化したパスで文書を共有する
		const std::filesystem::path key = Normalize(jsonPath);
		auto found = m_documents.find(key);
		if (found == m_documents.end()) {
			const std::vector<char>& json = m_files.Read(key);
			found = m_documents.emplace(key, RedoxDocument(json)).first;
		}

		// 直近の絶対パスの参照を保持する
		if (jsonPath.is_absolute()) {
			m_lastPath = jsonPath;
			m_lastDocument = &found->second; // std::map insertions preserve references.
		}
		return found->second;
	}

	std::filesystem::path JsonDocumentCache::Normalize(const std::filesystem::path& path) {
		std::error_code error;
		const std::filesystem::path absolute = std::filesystem::absolute(path, error);
		return (error ? path : absolute).lexically_normal();
	}
} // namespace Engine
