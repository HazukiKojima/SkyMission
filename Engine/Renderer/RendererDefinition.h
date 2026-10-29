#pragma once
#include "RetroFilmParameters.h"
#include "../Resources/Mesh/ModelLoadOptions.h"
#include <DirectXMath.h>
#include <filesystem>
#include <string>
#include <vector>
namespace Engine {
	struct OceanSettings {
		float size;
		float waveLength;
		float waveSteepness;
		float normalUvScale;
		float wavePatternUvScale;
		float fogStartDistance;
		float fogEndDistance;
		float edgeFadeStart;
		float edgeFadeWidth;
		float fogTop;
		float fogDensity;
	};

	struct SkySettings {
		float radius;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
	};

	struct RenderEnvironmentDefinition {
		std::filesystem::path skyTexture, oceanTexture, oceanNormalTexture, oceanNormalTexture2;
		OceanSettings ocean;
		SkySettings sky;
		RetroFilmParameters retroFilm;
	};
	struct ModelAssetDefinition {
		std::string id;
		std::filesystem::path file;
		ModelLoadOptions options;
	};
	struct RendererDefinition {
		RenderEnvironmentDefinition environment;
		std::vector<ModelAssetDefinition> models;
	};
} // namespace Engine
