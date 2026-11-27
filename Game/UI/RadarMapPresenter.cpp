#include "RadarMapPresenter.h"
#include "HudStyle.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <locale>

namespace Game {
	using namespace DirectX;
	namespace {
		constexpr float kMetersPerKilometer = 1000.0f;
		constexpr float kRadarDesignSize = 320.0f;
		constexpr float kMinimumTargetDistance = 0.001f;
		constexpr float kMinimumHorizontalHeadingLength = 0.001f;
		constexpr int kRadarGridDivisions = 8;
		constexpr float kRadarGridOpacity = 0.23f;
		constexpr float kRadarInnerBorderInset = 12.0f;
		constexpr float kRadarInnerRangeFraction = 0.5f;
	}

	std::string RadarMapPresenter::FormatRange(float meters) {
		std::ostringstream label;
		label.imbue(std::locale::classic());

		if (meters < kMetersPerKilometer) {
			label << std::fixed << std::setprecision(0) << meters << " m";
			return label.str();
		}

		label << std::fixed << std::setprecision(3) << meters / kMetersPerKilometer;
		auto value = label.str();

		// 距離表示の小数末尾に残るゼロを省く
		while (value.back() == '0')
			value.pop_back();
		if (value.back() == '.') value.pop_back();
		return value + " km";
	}

	RadarLayout RadarMapPresenter::Layout(const Engine::RenderView& view, const UiTuning& tuning) {
		RadarLayout layout;
		layout.rangeMeters = tuning.radarRangeMeters;

		if (view.width == 0 || view.height == 0) return layout;

		layout.scale = HudStyle::FitScale(view.width, view.height);
		layout.size = tuning.layout.radarSize * layout.scale;
		layout.origin = {tuning.layout.radarLeftMargin * layout.scale,
			view.height - tuning.layout.radarBottomMargin * layout.scale - layout.size};
		layout.center = {layout.origin.x + layout.size * 0.5f, layout.origin.y + layout.size * 0.5f};
		layout.radius = tuning.layout.radarRadius * (layout.size / kRadarDesignSize);
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

		// 範囲外の対象もレーダー外周へ収める
		point.valid = true;
		point.outside = distance > layout.rangeMeters;
		point.direction = distance > kMinimumTargetDistance
			? XMFLOAT2{dx / distance, dz / distance} : XMFLOAT2{0, -1};
		const float radius = std::min(distance / layout.rangeMeters, 1.0f) * layout.radius;
		point.pixel = {
			layout.center.x + point.direction.x * radius, layout.center.y + point.direction.y * radius};
		return point;
	}

	void RadarMapPresenter::Build(const CombatSnapshot& snapshot, const Engine::RenderView& view,
		Engine::HudDrawList& list, const UiTuning& tuning) const {
		constexpr float kRadarStrokeWidth = 1.4f;
		constexpr float kAircraftTailLengthFraction = 0.65f;
		constexpr float kAircraftTailHalfWidthFraction = 0.6f;
		constexpr float kEdgeArrowLength = 9.0f;
		constexpr float kEdgeArrowHalfWidth = 5.0f;
		constexpr float kEnemyMarkerSize = 7.0f;
		constexpr float kPlayerMarkerSize = 9.0f;
		constexpr float kSelectedMarkerHalfSize = 12.0f;
		constexpr float kSelectedMarkerCornerLength = 5.0f;
		const auto layout = Layout(view, tuning);

		if (layout.scale <= 0) return;

		const float scale = layout.size / kRadarDesignSize;
		const float lineWidth = std::max(1.0f, kRadarStrokeWidth * scale);
		const auto border = tuning.ink;
		const auto grid = HudStyle::Faded(border, kRadarGridOpacity);
		const auto labelColor = border;
		const auto enemyColor = border, selectedColor = tuning.selected;
		const auto panelPoint = [&](float x, float y) {
			return XMFLOAT2{layout.origin.x + x * scale, layout.origin.y + y * scale};
		};

		// 対象表示の下にレーダーの盤面を描く
		list.FilledRect(layout.origin, {layout.size, layout.size}, {0.035f, 0.038f, 0.033f, 0.65f});

		for (float inset : {0.0f, kRadarInnerBorderInset}) {
			const float outerEdge = kRadarDesignSize - inset;
			const XMFLOAT2 corners[] = {panelPoint(inset, inset), panelPoint(outerEdge, inset),
				panelPoint(outerEdge, outerEdge), panelPoint(inset, outerEdge)};
			for (size_t i = 0; i < std::size(corners); ++i) {
				list.Line(corners[i], corners[(i + 1) % std::size(corners)], lineWidth, border);
			}
		}

		constexpr float kRadarGridInset = 16.0f;
		constexpr float gridEnd = kRadarDesignSize - kRadarGridInset;
		for (int tick = 1; tick < kRadarGridDivisions; ++tick) {
			const float offset = float(tick) * (kRadarDesignSize / kRadarGridDivisions);
			list.Line(panelPoint(offset, kRadarGridInset), panelPoint(offset, gridEnd), lineWidth, grid);
			list.Line(panelPoint(kRadarGridInset, offset), panelPoint(gridEnd, offset), lineWidth, grid);
		}

		list.Circle(layout.center, layout.radius, 1, lineWidth, grid);
		list.Circle(layout.center, layout.radius * kRadarInnerRangeFraction, 1, lineWidth, grid);
		const float northFont = std::max(10.0f, 20 * scale);
		const float rangeFont = std::max(10.0f, 18 * scale);
		list.Text(
			{layout.center.x - northFont / 3, layout.origin.y + 14 * scale}, "N", northFont, labelColor);
		list.Text({layout.origin.x + 22 * scale, layout.origin.y + layout.size - 14 * scale - rangeFont},
			FormatRange(layout.rangeMeters), rangeFont, labelColor);
		list.Text(panelPoint(166, 218), FormatRange(layout.rangeMeters * kRadarInnerRangeFraction),
			std::max(9.0f, 16 * scale),
			labelColor);

		// 機首方位を三角形の向きで表す
		const auto aircraft = [&](XMFLOAT2 center, XMFLOAT3 heading, float size, XMFLOAT4 color) {
			const float length = std::hypot(heading.x, heading.z);
			const XMFLOAT2 dir = std::isfinite(length) && length > kMinimumHorizontalHeadingLength
				? XMFLOAT2{-heading.x / length, heading.z / length}
				: XMFLOAT2{0, -1};
			const XMFLOAT2 tip{center.x + dir.x * size, center.y + dir.y * size};
			const XMFLOAT2 left{center.x - dir.x * size * kAircraftTailLengthFraction -
					dir.y * size * kAircraftTailHalfWidthFraction,
				center.y - dir.y * size * kAircraftTailLengthFraction + dir.x * size * kAircraftTailHalfWidthFraction};
			const XMFLOAT2 right{center.x - dir.x * size * kAircraftTailLengthFraction +
					dir.y * size * kAircraftTailHalfWidthFraction,
				center.y - dir.y * size * kAircraftTailLengthFraction - dir.x * size * kAircraftTailHalfWidthFraction};
			list.Line(tip, left, lineWidth, color);
			list.Line(left, right, lineWidth, color);
			list.Line(right, tip, lineWidth, color);
		};

		// 範囲内は機体記号で範囲外は方向を示す
		for (const auto& enemy : snapshot.enemies) {
			const auto point = Project(enemy.center, snapshot.playerPosition, layout);

			if (!point.valid) continue;

			const bool selected = enemy.id == snapshot.selected;
			const auto color = selected ? selectedColor : enemyColor;

			if (point.outside) {
				const auto& direction = point.direction;
				const XMFLOAT2 tail{
					point.pixel.x - direction.x * kEdgeArrowLength * scale,
					point.pixel.y - direction.y * kEdgeArrowLength * scale};
				list.Line(point.pixel, {tail.x - direction.y * kEdgeArrowHalfWidth * scale,
						tail.y + direction.x * kEdgeArrowHalfWidth * scale},
					lineWidth, color);
				list.Line(point.pixel, {tail.x + direction.y * kEdgeArrowHalfWidth * scale,
						tail.y - direction.x * kEdgeArrowHalfWidth * scale},
					lineWidth, color);
			} else {
				aircraft(point.pixel, enemy.heading, kEnemyMarkerSize * scale, color);
			}

			// 選択中の対象に枠を重ねる
			if (selected) {
				const float size = kSelectedMarkerHalfSize * scale;
				const float corner = kSelectedMarkerCornerLength * scale;

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

		aircraft(layout.center, snapshot.playerHeading, kPlayerMarkerSize * scale, border);
	}
} // namespace Game
