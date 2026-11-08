#pragma once
#include <DirectXMath.h>
#include <filesystem>
#include <string>

namespace Engine {
	class JsonAssetReader;
}
namespace Game {
	struct OpeningCinematicSettings {
		float durationSeconds = 5;
		float fadeOutStartSeconds = 3.6f;
		float cutSeconds = 3.8f;
		float fadeInStartSeconds = 4;
		float fadeInSeconds = 0.7f;
		float formationSpacingMeters = 180;
		float formationDepthMeters = 200;
		float flightSpeedMetersPerSecond = 180;
		float propellerRadiansPerSecond = 35;
		// 先頭機を原点とするワールド軸オフセット。敵機は+Z方向へ飛ぶ
		DirectX::XMFLOAT3 cameraStartOffsetMeters{180, 100, 360};
		DirectX::XMFLOAT3 cameraEndOffsetMeters{100, 75, 320};
		DirectX::XMFLOAT3 lookAtOffsetMeters{0, 0, -50};
		float verticalFovDegrees = 55;
		std::string title = "敵哨戒編隊";
		std::string subtitle = "正面より接近";
		void Validate() const;
	};
	OpeningCinematicSettings LoadOpeningCinematicSettings(
		const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
} // namespace Game
