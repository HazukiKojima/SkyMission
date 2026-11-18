#pragma once
#include "Game/Cinematics/CinematicTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include <algorithm>

namespace Game {
	// 演出や全画面の背景なしでも、戦闘中の無線を描画できる
	class RadioCaptionPresenter {
	  public:
		void Build(const RadioCaption& caption, const Engine::RenderView& view,
			Engine::HudDrawList& list, float topPixels = 0) const;
	};
} // namespace Game
