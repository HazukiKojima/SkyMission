#include "CinematicPresenter.h"
#include "HudStyle.h"
#include <algorithm>

namespace Game {
	void CinematicPresenter::Build(const CinematicPresentation& presentation, const Engine::RenderView& view,
		Engine::HudDrawList& list) const {
		if (!presentation.active || view.width == 0 || view.height == 0) return;
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float textScale = scale * (presentation.cinematicId == "opening" ? 1.5f : 1.0f);
		if (presentation.briefingLine >= 0) {
			const auto& text = presentation.briefingLines;
			list.FilledRect({0, 0}, {float(view.width), float(view.height)}, {0, 0, 0, 1});
			const float widest = std::max(Engine::HudDrawList::MeasureTextWidth(text[0], 1),
				Engine::HudDrawList::MeasureTextWidth(text[1], 1));
			const float size = std::min(48 * textScale, (view.width - 120 * scale) / std::max(1.0f, widest));
			const float spacing = size * 1.6f;
			for (int row = 0; row < 2; ++row)
				list.Text({(view.width - Engine::HudDrawList::MeasureTextWidth(text[row], size)) * 0.5f,
							  (view.height - size - spacing) * 0.5f + row * spacing},
					text[row], size, HudStyle::Ink);
			return;
		}
		const float barHeight = view.height * 0.1f;
		list.FilledRect({0, 0}, {float(view.width), barHeight}, {0, 0, 0, 1});
		list.FilledRect({0, view.height - barHeight}, {float(view.width), barHeight}, {0, 0, 0, 1});
		if (presentation.formationShot) {
			const float titleSize = std::min(48 * textScale,
				(view.width - 96 * scale) /
					std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(presentation.title, 1)));
			const float subtitleSize = std::min(28 * textScale,
				(view.width - 96 * scale) /
					std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(presentation.subtitle, 1)));
			list.Text({48 * scale, view.height - barHeight - 120 * textScale}, presentation.title, titleSize,
				HudStyle::Ink);
			list.Text({48 * scale, view.height - barHeight - 58 * textScale}, presentation.subtitle, subtitleSize,
				HudStyle::Ink);
		}
		// フェードは映像と紹介文に重ね、スキップ案内は暗転中も読めるよう最後に描く
		if (presentation.fadeOpacity > 0)
			list.FilledRect(
				{0, 0}, {float(view.width), float(view.height)}, {0, 0, 0, presentation.fadeOpacity});
		constexpr std::string_view hint = "Enter：スキップ";
		const float fontSize = 24 * textScale;
		list.Text({view.width - 48 * scale - Engine::HudDrawList::MeasureTextWidth(hint, fontSize),
					  view.height - barHeight * 0.5f - fontSize * 0.5f},
			hint, fontSize, HudStyle::Ink);
	}
} // namespace Game
