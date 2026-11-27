#pragma once
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace Engine {
	// 読み込みの段階ごとに所要時間を記録する任意の診断
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
		void Write() const noexcept;

		std::string_view m_stage;
		std::chrono::steady_clock::time_point m_started = std::chrono::steady_clock::now();
	};
} // namespace Engine
