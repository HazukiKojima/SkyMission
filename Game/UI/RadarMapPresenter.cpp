#include "RadarMapPresenter.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;
	RadarLayout RadarMapPresenter::Layout(const Engine::RenderView& view) {
		RadarLayout layout;
		if (view.width == 0 || view.height == 0)
			return layout;
		layout.scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		layout.size = 320 * layout.scale;
		layout.origin = {48 * layout.scale, view.height - (48 + 320) * layout.scale};
		layout.center = {layout.origin.x + layout.size * 0.5f, layout.origin.y + layout.size * 0.5f};
		layout.radius = 126 * layout.scale;
		layout.statusOrigin = {layout.origin.x + layout.size + 24 * layout.scale, layout.origin.y};
		return layout;
	}
	RadarPoint RadarMapPresenter::Project(XMFLOAT3 world, XMFLOAT3 player, const RadarLayout& layout) {
		RadarPoint point;
		// 左手座標系で-Zを北にすると、北向きカメラの右方向は-Xになる
		const float dx = player.x - world.x, dz = world.z - player.z;
		const float distance = std::hypot(dx, dz);
		if (layout.scale <= 0 || !std::isfinite(distance))
			return point;
		point.valid = true;
		point.outside = distance > RangeMeters;
		point.direction = distance > 0.001f ? XMFLOAT2{dx / distance, dz / distance} : XMFLOAT2{0, -1};
		const float radius = std::min(distance / RangeMeters, 1.0f) * layout.radius;
		point.pixel = {
			layout.center.x + point.direction.x * radius, layout.center.y + point.direction.y * radius};
		return point;
	}
	void RadarMapPresenter::Build(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const auto layout = Layout(view);
		if (layout.scale <= 0)
			return;
		const float scale = layout.scale, lineWidth = std::max(1.0f, 1.4f * scale);
		const XMFLOAT4 border{0.2f, 0.65f, 0.35f, 0.75f};
		const XMFLOAT4 grid{0.12f, 0.55f, 0.26f, 0.23f};
		const XMFLOAT4 labelColor{0.32f, 0.85f, 0.48f, 0.9f};
		const XMFLOAT4 enemyColor{0.3f, 1, 0.6f, 1}, selectedColor{1, 0.85f, 0.25f, 1};
		const auto panelPoint = [&](float x, float y) {
			return XMFLOAT2{layout.origin.x + x * scale, layout.origin.y + y * scale};
		};
		list.FilledRect(layout.origin, {layout.size, layout.size}, {0.015f, 0.075f, 0.03f, 0.86f});
		for (float inset : {0.0f, 12.0f}) {
			const XMFLOAT2 corners[] = {panelPoint(inset, inset), panelPoint(320 - inset, inset),
				panelPoint(320 - inset, 320 - inset), panelPoint(inset, 320 - inset)};
			for (size_t i = 0; i < std::size(corners); ++i)
				list.Line(corners[i], corners[(i + 1) % std::size(corners)], lineWidth, border);
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
			"10 km", rangeFont, labelColor);
		list.Text(panelPoint(166, 218), "5 km", std::max(9.0f, 16 * scale), labelColor);
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
			if (!point.valid)
				continue;
			const bool selected = enemy.id == snapshot.selected;
			const auto color = selected ? selectedColor : enemyColor;
			if (point.outside) {
				const auto& d = point.direction;
				const XMFLOAT2 tail{point.pixel.x - d.x * 9 * scale, point.pixel.y - d.y * 9 * scale};
				list.Line(
					point.pixel, {tail.x - d.y * 5 * scale, tail.y + d.x * 5 * scale}, lineWidth, color);
				list.Line(
					point.pixel, {tail.x + d.y * 5 * scale, tail.y - d.x * 5 * scale}, lineWidth, color);
			} else {
				aircraft(point.pixel, enemy.heading, 7 * scale, color);
			}
			if (selected) {
				const float size = 12 * scale, corner = 5 * scale;
				for (float x : {-1.0f, 1.0f})
					for (float y : {-1.0f, 1.0f}) {
						const XMFLOAT2 p{point.pixel.x + x * size, point.pixel.y + y * size};
						list.Line(p, {p.x - x * corner, p.y}, lineWidth, selectedColor);
						list.Line(p, {p.x, p.y - y * corner}, lineWidth, selectedColor);
					}
			}
		}
		for (const auto& missile : snapshot.incomingMissiles) {
			const auto point = Project(missile.position, snapshot.playerPosition, layout);
			if (!point.valid)
				continue;
			const float radius = 4 * scale;
			const XMFLOAT2 corners[] = {{point.pixel.x, point.pixel.y - radius},
				{point.pixel.x + radius, point.pixel.y}, {point.pixel.x, point.pixel.y + radius},
				{point.pixel.x - radius, point.pixel.y}};
			for (size_t i = 0; i < std::size(corners); ++i)
				list.Line(corners[i], corners[(i + 1) % std::size(corners)], lineWidth, {1, 0.22f, 0.15f, 1});
		}
		aircraft(layout.center, snapshot.playerHeading, 9 * scale, {0.92f, 1, 0.95f, 1});
	}
} // namespace Game
