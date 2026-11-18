#include "StartupTiming.h"

namespace Engine {
	void StartupTiming::Write() const noexcept {
		try {
			static const std::filesystem::path path = [] {
				wchar_t value[32768]{};

				const DWORD length =
					GetEnvironmentVariableW(L"SKYMISSION_STARTUP_LOG", value, _countof(value));
				return length && length < _countof(value) ? std::filesystem::path(value)
														  : std::filesystem::path{};
			}();

			if (path.empty()) return;

			static std::mutex mutex;
			const double milliseconds =
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_started)
					.count();
			std::lock_guard lock(mutex);
			std::ofstream output(path, std::ios::app);
			output << m_stage << '\t' << milliseconds << " ms\n";
		} catch (...) {
			// 診断ファイルの失敗を、読み込みの妨げにしない
		}
	}
} // namespace Engine
