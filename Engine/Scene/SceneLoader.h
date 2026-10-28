#pragma once

#include "../Aircraft/AircraftAIController.h"
#include "../Aircraft/AircraftFlightParameters.h"
#include "../Renderer/RetroFilmParameters.h"
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

	struct SceneSourcePaths {
		std::filesystem::path environment;
		std::filesystem::path playerAircraft;
		std::filesystem::path enemyAircraft;
		std::filesystem::path retroFilm;
	};

	struct AircraftDefinition {
		std::filesystem::path model;
		float visualScale = 1.0f;
		AircraftFlightParameters flightParameters;
	};

	struct SceneDefinition {
		std::filesystem::path skyTexture;
		std::filesystem::path oceanTexture;
		std::filesystem::path oceanNormalTexture;
		std::filesystem::path oceanNormalTexture2;
		AircraftDefinition playerAircraft;
		AircraftDefinition enemyAircraft;
		AircraftAIParameters enemyAIParameters;
		OceanSettings ocean;
		SkySettings sky;
		RetroFilmParameters retroFilm;
	};

	class SceneLoader {
	public:
		SceneDefinition Load(const SceneSourcePaths& paths) const;
	};
}
