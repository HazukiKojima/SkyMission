#pragma once
#include <DirectXMath.h>
#include <filesystem>
#include <string>
#include <array>
#include <vector>

namespace Engine {
	class JsonAssetReader;
}
namespace Game {
	struct OpeningBriefingEntry {
		std::array<std::string, 2> lines;
		std::filesystem::path audio;
		float fallbackSeconds = 5;
	};
	struct OpeningAudioSettings {
		std::filesystem::path bgm = L"../Sound/Opening/空襲BGM.wav";
		float bgmVolume = 0.12f;
		float briefingVolume = 0.4f;
		float flightVolumeScale = 0.4f;
	};
	struct OpeningCinematicSettings {
		std::vector<OpeningBriefingEntry> briefing{
			{{"敵母艦と護衛機を確認。", "基地を襲った部隊の発進元だ。"},
				L"../Sound/Opening/敵母艦と護衛機を確認。基地を襲った部隊の発進元だ.wav", 5},
			{{"基地は現在も攻撃を受けており、", "増援は出せない。"},
				L"../Sound/Opening/基地は現在も攻撃を受けており、増援は出せない.wav", 5},
			{{"君、一機で護衛機を排除し、", "母艦を撃沈せよ。"},
				L"../Sound/Opening/君、一機で護衛機を排除し、母艦を撃沈せよ.wav", 5}};
		OpeningAudioSettings audio;
		std::vector<DirectX::XMFLOAT3> formationOffsets{{0, 0, 0}, {-1, 0, -1}, {1, 0, -1}, {0, 0.35f, -2}};
		DirectX::XMFLOAT3 originOffsetMeters{0, 0, -5000};
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
