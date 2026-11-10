#pragma once
#include <DirectXMath.h>
#include <string>
#include <array>
#include <cstdint>

namespace Game {
	enum class CinematicVoiceStatus { Unavailable, Playing, Finished };
	enum class CinematicCompletion { Completed, Skipped };
	// HUDへ渡す値だけを保持し、描画側に再生制御やシーンの所有権を渡さない
	struct CinematicPresentation {
		bool active = false;
		bool formationShot = true;
		float elapsedSeconds = 0;
		float fadeOpacity = 0;
		std::string title;
		std::string subtitle;
		int briefingLine = -1; // 会話項目の番号。-1は映像演出。
		std::array<std::string, 2> briefingLines;
		std::string cinematicId = "opening";
		size_t shotIndex = 0;
		uint64_t playbackSerial = 0;
	};
	struct CinematicPose {
		DirectX::XMFLOAT3 cameraOffsetMeters;
		float travelMeters = 0;
		float propellerRadians = 0;
	};
} // namespace Game
