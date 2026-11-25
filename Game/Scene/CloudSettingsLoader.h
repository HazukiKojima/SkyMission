#pragma once
#include "Engine/Renderer/Environment/CloudSettings.h"
#include "Engine/Resources/JsonAssetReader.h"
namespace Game {
	Engine::CloudSettings LoadCloudSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
}
