#pragma once
#include <DirectXMath.h>
#include <string>

namespace Game {
	// HUDへ渡す値だけを保持し、描画側に再生制御やシーンの所有権を渡さない
	struct OpeningCinematicPresentation {
		bool active = false;
		bool formationShot = true;
		float elapsedSeconds = 0;
		float fadeOpacity = 0;
		std::string title;
		std::string subtitle;
	};
	struct OpeningCinematicPose {
		DirectX::XMFLOAT3 cameraOffsetMeters;
		float travelMeters = 0;
		float propellerRadians = 0;
	};
} // namespace Game
