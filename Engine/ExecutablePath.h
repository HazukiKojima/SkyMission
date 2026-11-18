#pragma once
#include "EngineCommon.h"
#include <filesystem>
#include <vector>

namespace Engine {
	std::filesystem::path ExecutableDirectory();

	std::filesystem::path AssetsDirectory();

	std::filesystem::path ShaderDirectory();
} // namespace Engine
