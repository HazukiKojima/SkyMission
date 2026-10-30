#pragma once
#include "CombatTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "Engine/Resources/JsonAssetReader.h"

namespace Game {
	CombatSettings LoadCombatSettings(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	Engine::HudSettings LoadHudSettings(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
}
