#pragma once
#include "Engine/Renderer/RendererDefinition.h"

namespace Game {
	struct BombDefinition {
		Engine::ModelAssetDefinition ammunition;
		DirectX::XMFLOAT3 modelForward{1, 0, 0};
	};
} // namespace Game
