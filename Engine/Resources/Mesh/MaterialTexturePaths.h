#pragma once
#include "ModelLoadOptions.h"

namespace Engine::MaterialTexturePaths {
	std::filesystem::path FindTexturePath(const std::filesystem::path& directory, const std::filesystem::path& texture,
										  const ModelLoadOptions& options);
	std::filesystem::path FindFallbackTexturePath(const std::filesystem::path& directory, const std::string& material,
												  const ModelLoadOptions& options);
	std::filesystem::path FindNamedMaterialTexturePath(const std::filesystem::path& directory,
													   const std::string& material,
													   const std::filesystem::path& baseColor,
													   const std::wstring& suffix, const ModelLoadOptions& options);
} // namespace Engine::MaterialTexturePaths
