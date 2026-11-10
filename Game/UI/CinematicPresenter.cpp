#include "CinematicPresenter.h"
#include "HudStyle.h"
#include <algorithm>

namespace Game {
	void CinematicPresenter::Build(const CinematicPresentation& presentation, const Engine::RenderView& view,
		Engine::HudDrawList& list) const {
		if (!presentation.active || view.width == 0 || view.height == 0) return;
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const auto& textSettings = presentation.text;
		const float textScale = scale * textSettings.textScale;
		if (presentation.briefingLine >= 0) {
			const auto& text = presentation.briefingLines;
			list.FilledRect({0, 0}, {float(view.width), float(view.height)}, {0, 0, 0, 1});
			const float widest = std::max(Engine::HudDrawList::MeasureTextWidth(text[0], 1),
				Engine::HudDrawList::MeasureTextWidth(text[1], 1));
			const float size = std::min({textSettings.briefingFontSize * textScale,
				(view.height - 120 * scale) / (1 + textSettings.lineSpacing),
				(view.width - 120 * scale) / std::max(1.0f, widest)});
			const float spacing = size * textSettings.lineSpacing;
			for (int row = 0; row < 2; ++row)
				list.Text({(view.width - Engine::HudDrawList::MeasureTextWidth(text[row], size)) * 0.5f,
							  (view.height - size - spacing) * 0.5f + row * spacing},
					text[row], size, m_ink);
			return;
		}
		const float barHeight = view.height * textSettings.barHeightFraction;
		list.FilledRect({0, 0}, {float(view.width), barHeight}, {0, 0, 0, 1});
		list.FilledRect({0, view.height - barHeight}, {float(view.width), barHeight}, {0, 0, 0, 1});
		if (presentation.formationShot) {
			const float titleSize = std::min(textSettings.titleFontSize * textScale,
				(view.width - 96 * scale) /
					std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(presentation.title, 1)));
			const float subtitleSize = std::min(textSettings.subtitleFontSize * textScale,
				(view.width - 96 * scale) /
					std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(presentation.subtitle, 1)));
			list.Text({48 * scale, view.height - barHeight - textSettings.titleBottomOffset * textScale},
				presentation.title, titleSize, m_ink);
			list.Text({48 * scale, view.height - barHeight - textSettings.subtitleBottomOffset * textScale},
				presentation.subtitle, subtitleSize, m_ink);
		}
		// フェードは映像と紹介文に重ね、スキップ案内は暗転中も読めるよう最後に描く
		if (presentation.fadeOpacity > 0)
			list.FilledRect(
				{0, 0}, {float(view.width), float(view.height)}, {0, 0, 0, presentation.fadeOpacity});
		const auto& hint = textSettings.skipHint;
		const float fontSize = std::min({textSettings.hintFontSize * textScale, barHeight * 0.8f,
			(view.width - 96 * scale) / std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(hint, 1))});
		list.Text({view.width - 48 * scale - Engine::HudDrawList::MeasureTextWidth(hint, fontSize),
					  view.height - barHeight * 0.5f - fontSize * 0.5f},
			hint, fontSize, m_ink);
	}
} // namespace Game
