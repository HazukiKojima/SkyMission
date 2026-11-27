#include "RadioCaptionPresenter.h"
#include "HudStyle.h"

namespace Game {
	void RadioCaptionPresenter::Build(const RadioCaption& caption, const Engine::RenderView& view,
		Engine::HudDrawList& list, float topPixels) const {
		constexpr float kCaptionTotalHorizontalPadding = 64.0f;
		constexpr float kCaptionTopInset = 16.0f;
		constexpr float kCaptionBodyOffset = 42.0f;
		constexpr float kCaptionSpeakerFontSize = 30.0f;
		constexpr float kCaptionBodyFontSize = 32.0f;
		constexpr float kCaptionShadowOffset = 2.0f;
		if (view.width == 0 || view.height == 0 || caption.text.empty()) return;

		const float scale = HudStyle::FitScale(view.width, view.height);
		const DirectX::XMFLOAT4 accent = caption.side == "enemy" ? DirectX::XMFLOAT4{1, 0.35f, 0.3f, 1}
																 : DirectX::XMFLOAT4{0.2f, 0.8f, 1, 1};

		// 影を重ねて背景上でも字幕を読めるようにする
		const auto shadowText = [&](float x, float y, std::string_view text, float size,
									DirectX::XMFLOAT4 color) {
			list.Text({x + kCaptionShadowOffset * scale, y + kCaptionShadowOffset * scale},
				text, size, {0, 0, 0, 0.85f});
			list.Text({x, y}, text, size, color);
		};

		// 話者名と本文を画面幅に収まる大きさで配置する
		const float available = view.width - kCaptionTotalHorizontalPadding * scale;
		const float speakerSize = std::min(kCaptionSpeakerFontSize * scale,
			available / std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(caption.speaker, 1)));
		const float y = topPixels + kCaptionTopInset * scale;
		shadowText((view.width - Engine::HudDrawList::MeasureTextWidth(caption.speaker, speakerSize)) / 2, y,
			caption.speaker, speakerSize, accent);
		const std::string line = "≪ " + caption.text + " ≫";
		const float size =
			std::min(kCaptionBodyFontSize * scale,
				available / std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(line, 1)));
		const float x = (view.width - Engine::HudDrawList::MeasureTextWidth(line, size)) / 2;
		const float bodyX = x + Engine::HudDrawList::MeasureTextWidth("≪ ", size);
		const float bodyY = y + kCaptionBodyOffset * scale;
		shadowText(x, bodyY, "≪", size, accent);
		shadowText(bodyX, bodyY, caption.text, size, {0.95f, 0.98f, 1, 1});
		shadowText(x + Engine::HudDrawList::MeasureTextWidth("≪ " + caption.text + " ", size), bodyY,
			"≫", size, accent);
	}
} // namespace Game
