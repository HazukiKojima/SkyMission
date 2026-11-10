#pragma once
#include "Game/Settings/GameplaySettings.h"
#include "Engine/Renderer/RendererDefinition.h"

namespace Game {
	struct BombDefinition {
		MissileSettings missile;
		Engine::ModelAssetDefinition ammunition;
		DirectX::XMFLOAT3 modelForward{1, 0, 0};
	};
} // namespace Game
