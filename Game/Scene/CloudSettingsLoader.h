#pragma once
#include "Engine/Scene/CloudSettings.h"
#include "Engine/Resources/JsonAssetReader.h"
namespace Game {
	Engine::CloudSettings LoadCloudSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
}
