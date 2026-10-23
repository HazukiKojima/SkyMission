#pragma once

#include "AircraftFlightParameters.h"
#include <filesystem>
#include <DirectXMath.h>

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
	};

	struct SkySettings {
		float radius;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
	};

	struct SceneAssetPaths {
		std::filesystem::path skyTexture;
		std::filesystem::path oceanTexture;
		std::filesystem::path oceanNormalTexture;
		std::filesystem::path oceanNormalTexture2;
		std::filesystem::path aircraftModel;
		float aircraftVisualScale = 1.0f;
		OceanSettings ocean;
		SkySettings sky;
		AircraftFlightParameters flightParameters;
	};

	class SceneLoader {
	public:
		SceneAssetPaths LoadAssets(
			const std::filesystem::path& environmentJsonPath,
			const std::filesystem::path& aircraftJsonPath) const;
	};
}
