#pragma once
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace Engine {
	// Opt-in trace for separating engine loading from process launch/debugger overhead.
	class StartupTiming {
	  public:
		explicit StartupTiming(std::string_view stage) : m_stage(stage) {}
		void Mark(std::string_view stage) {
			Write();
			m_stage = stage;
			m_started = std::chrono::steady_clock::now();
		}
		~StartupTiming() {
			Write();
		}

	  private:
		void Write() const noexcept {
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
				// A diagnostic file must never prevent loading.
			}
		}
		std::string_view m_stage;
		std::chrono::steady_clock::time_point m_started = std::chrono::steady_clock::now();
	};
} // namespace Engine
