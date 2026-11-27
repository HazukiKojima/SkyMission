#include "ExecutablePath.h"

namespace Engine {
	std::filesystem::path ExecutableDirectory() {
		constexpr size_t kInitialPathBufferCharacters = 512;
		std::vector<wchar_t> buffer(kInitialPathBufferCharacters);
		for (;;) {
			const DWORD count = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));

			if (count == 0) ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
			if (count < buffer.size())
				return std::filesystem::path(std::wstring(buffer.data(), count)).parent_path();

			// パスが切れた場合はバッファを広げて取得し直す
			buffer.resize(buffer.size() * 2);
		}
	}

	std::filesystem::path AssetsDirectory() {
		const auto directory = ExecutableDirectory();

		// 配布版のRuntime内、EXE直下、開発ビルドの順で解決する
		for (const auto& candidate :
			{directory.parent_path() / L"Assets", directory / L"Assets", directory / L"../../Assets"}) {
			if (std::filesystem::is_directory(candidate)) return candidate;
		}

		throw std::runtime_error("Assets directory not found beside the game distribution.");
	}

	std::filesystem::path ShaderDirectory() {
		const auto directory = ExecutableDirectory();
		return std::filesystem::is_directory(directory / L"Shaders") ? directory / L"Shaders" : directory;
	}
} // namespace Engine
