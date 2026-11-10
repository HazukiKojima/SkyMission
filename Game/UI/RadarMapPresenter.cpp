#include "RadarMapPresenter.h"
#include "HudStyle.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <locale>

namespace Game {
	using namespace DirectX;
	std::string RadarMapPresenter::FormatRange(float meters) {
		std::ostringstream label;
		label.imbue(std::locale::classic());
		if (meters < 1000) {
			label << std::fixed << std::setprecision(0) << meters << " m";
			return label.str();
		}
		label << std::fixed << std::setprecision(3) << meters / 1000;
		auto value = label.str();
		while (value.back() == '0')
			value.pop_back();
		if (value.back() == '.') value.pop_back();
		return value + " km";
	}

	RadarLayout RadarMapPresenter::Layout(const Engine::RenderView& view, const UiTuning& tuning) {
		RadarLayout layout;
		layout.rangeMeters = tuning.radarRangeMeters;
		if (view.width == 0 || view.height == 0) return layout;
		layout.scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		layout.size = tuning.layout.radarSize * layout.scale;
		layout.origin = {tuning.layout.radarLeftMargin * layout.scale,
			view.height - tuning.layout.radarBottomMargin * layout.scale - layout.size};
		layout.center = {layout.origin.x + layout.size * 0.5f, layout.origin.y + layout.size * 0.5f};
		layout.radius = tuning.layout.radarRadius * (layout.size / 320);
		layout.statusOrigin = {
			layout.origin.x + layout.size + tuning.layout.radarStatusGap * layout.scale, layout.origin.y};
		return layout;
	}
	RadarPoint RadarMapPresenter::Project(XMFLOAT3 world, XMFLOAT3 player, const RadarLayout& layout) {
		RadarPoint point;
		// 左手座標系で-Zを北にすると、北向きカメラの右方向は-Xになる
		const float dx = player.x - world.x, dz = world.z - player.z;
		const float distance = std::hypot(dx, dz);
		if (layout.scale <= 0 || !std::isfinite(distance)) return point;
		point.valid = true;
		point.outside = distance > layout.rangeMeters;
		point.direction = distance > 0.001f ? XMFLOAT2{dx / distance, dz / distance} : XMFLOAT2{0, -1};
		const float radius = std::min(distance / layout.rangeMeters, 1.0f) * layout.radius;
		point.pixel = {
			layout.center.x + point.direction.x * radius, layout.center.y + point.direction.y * radius};
		return point;
	}
	void RadarMapPresenter::Build(const CombatSnapshot& snapshot, const Engine::RenderView& view,
		Engine::HudDrawList& list, const UiTuning& tuning) const {
		const auto layout = Layout(view, tuning);
		if (layout.scale <= 0) return;
		const float scale = layout.size / 320, lineWidth = std::max(1.0f, 1.4f * scale);
		const auto border = tuning.ink;
		const auto grid = HudStyle::Faded(border, 0.23f);
		const auto labelColor = border;
		const auto enemyColor = border, selectedColor = tuning.selected;
		const auto panelPoint = [&](float x, float y) {
			return XMFLOAT2{layout.origin.x + x * scale, layout.origin.y + y * scale};
		};
		list.FilledRect(layout.origin, {layout.size, layout.size}, {0.035f, 0.038f, 0.033f, 0.65f});
		for (float inset : {0.0f, 12.0f}) {
			const XMFLOAT2 corners[] = {panelPoint(inset, inset), panelPoint(320 - inset, inset),
				panelPoint(320 - inset, 320 - inset), panelPoint(inset, 320 - inset)};
			for (size_t i = 0; i < std::size(corners); ++i) {
				list.Line(corners[i], corners[(i + 1) % std::size(corners)], lineWidth, border);
			}
		}
		for (int tick = 1; tick < 8; ++tick) {
			const float offset = float(tick) * 40;
			list.Line(panelPoint(offset, 16), panelPoint(offset, 304), lineWidth, grid);
			list.Line(panelPoint(16, offset), panelPoint(304, offset), lineWidth, grid);
		}
		list.Circle(layout.center, layout.radius, 1, lineWidth, grid);
		list.Circle(layout.center, layout.radius * 0.5f, 1, lineWidth, grid);
		const float northFont = std::max(10.0f, 20 * scale);
		const float rangeFont = std::max(10.0f, 18 * scale);
		list.Text(
			{layout.center.x - northFont / 3, layout.origin.y + 14 * scale}, "N", northFont, labelColor);
		list.Text({layout.origin.x + 22 * scale, layout.origin.y + layout.size - 14 * scale - rangeFont},
			FormatRange(layout.rangeMeters), rangeFont, labelColor);
		list.Text(panelPoint(166, 218), FormatRange(layout.rangeMeters * 0.5f), std::max(9.0f, 16 * scale),
			labelColor);
		const auto aircraft = [&](XMFLOAT2 center, XMFLOAT3 heading, float size, XMFLOAT4 color) {
			const float length = std::hypot(heading.x, heading.z);
			const XMFLOAT2 dir = std::isfinite(length) && length > 0.001f
				? XMFLOAT2{-heading.x / length, heading.z / length}
				: XMFLOAT2{0, -1};
			const XMFLOAT2 tip{center.x + dir.x * size, center.y + dir.y * size};
			const XMFLOAT2 left{center.x - dir.x * size * 0.65f - dir.y * size * 0.6f,
				center.y - dir.y * size * 0.65f + dir.x * size * 0.6f};
			const XMFLOAT2 right{center.x - dir.x * size * 0.65f + dir.y * size * 0.6f,
				center.y - dir.y * size * 0.65f - dir.x * size * 0.6f};
			list.Line(tip, left, lineWidth, color);
			list.Line(left, right, lineWidth, color);
			list.Line(right, tip, lineWidth, color);
		};
		for (const auto& enemy : snapshot.enemies) {
			const auto point = Project(enemy.center, snapshot.playerPosition, layout);
			if (!point.valid) continue;
			const bool selected = enemy.id == snapshot.selected;
			const auto color = selected ? selectedColor : enemyColor;
			if (point.outside) {
				const auto& direction = point.direction;
				const XMFLOAT2 tail{
					point.pixel.x - direction.x * 9 * scale, point.pixel.y - direction.y * 9 * scale};
				list.Line(point.pixel, {tail.x - direction.y * 5 * scale, tail.y + direction.x * 5 * scale},
					lineWidth, color);
				list.Line(point.pixel, {tail.x + direction.y * 5 * scale, tail.y - direction.x * 5 * scale},
					lineWidth, color);
			} else {
				aircraft(point.pixel, enemy.heading, 7 * scale, color);
			}
			if (selected) {
				const float size = 12 * scale, corner = 5 * scale;
				for (float x : {-1.0f, 1.0f}) {
					for (float y : {-1.0f, 1.0f}) {
						const XMFLOAT2 cornerPoint{point.pixel.x + x * size, point.pixel.y + y * size};
						list.Line(cornerPoint, {cornerPoint.x - x * corner, cornerPoint.y}, lineWidth,
							selectedColor);
						list.Line(cornerPoint, {cornerPoint.x, cornerPoint.y - y * corner}, lineWidth,
							selectedColor);
					}
				}
			}
		}
		aircraft(layout.center, snapshot.playerHeading, 9 * scale, border);
	}
} // namespace Game
