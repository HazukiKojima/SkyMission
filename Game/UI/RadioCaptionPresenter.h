#pragma once
#include "Game/Cinematics/CinematicTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include <algorithm>

namespace Game {
	// Can also draw battle chatter without a cinematic or a full-screen background.
	class RadioCaptionPresenter {
	  public:
		void Build(const RadioCaption& caption, const Engine::RenderView& view,
			Engine::HudDrawList& list, float topPixels = 0) const {
			if (view.width == 0 || view.height == 0 || caption.text.empty()) return;
			const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
			const DirectX::XMFLOAT4 accent = caption.side == "enemy"
				? DirectX::XMFLOAT4{1, 0.35f, 0.3f, 1} : DirectX::XMFLOAT4{0.2f, 0.8f, 1, 1};
			const auto shadowText = [&](float x, float y, std::string_view text, float size,
				DirectX::XMFLOAT4 color) {
				list.Text({x + 2 * scale, y + 2 * scale}, text, size, {0, 0, 0, 0.85f});
				list.Text({x, y}, text, size, color);
			};
			const float available = view.width - 64 * scale;
			const float speakerSize = std::min(30 * scale, available /
				std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(caption.speaker, 1)));
			const float y = topPixels + 16 * scale;
			shadowText((view.width - Engine::HudDrawList::MeasureTextWidth(caption.speaker, speakerSize)) / 2,
				y, caption.speaker, speakerSize, accent);
			const std::string line = "≪ " + caption.text + " ≫";
			const float size = std::min(32 * scale, available /
				std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(line, 1)));
			const float x = (view.width - Engine::HudDrawList::MeasureTextWidth(line, size)) / 2;
			const float bodyX = x + Engine::HudDrawList::MeasureTextWidth("≪ ", size);
			shadowText(x, y + 42 * scale, "≪", size, accent);
			shadowText(bodyX, y + 42 * scale, caption.text, size, {0.95f, 0.98f, 1, 1});
			shadowText(x + Engine::HudDrawList::MeasureTextWidth("≪ " + caption.text + " ", size),
				y + 42 * scale, "≫", size, accent);
		}
	};
} // namespace Game
