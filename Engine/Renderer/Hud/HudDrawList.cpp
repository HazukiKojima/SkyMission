#include "HudDrawList.h"
#include "HudFont.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Engine {
	void HudDrawList::Quad(const DirectX::XMFLOAT2 (&positions)[4], DirectX::XMFLOAT2 uv0,
		DirectX::XMFLOAT2 uv1, DirectX::XMFLOAT4 color) {
		if (m_width == 0 || m_height == 0) return;
		if (m_vertices.size() + QuadVertexCount > MaxVertices) throw std::length_error("HUD vertex capacity exceeded");

		// 画面座標の四角形をクリップ空間の二つの三角形へ変換する
		const DirectX::XMFLOAT2 uv[] = {uv0, {uv1.x, uv0.y}, uv1, {uv0.x, uv1.y}};
		for (const int index : {0, 1, 2, 0, 2, 3})
			m_vertices.push_back(
				{{positions[index].x * 2 / m_width - 1, 1 - positions[index].y * 2 / m_height}, uv[index],
					color});
	}

	void HudDrawList::FilledRect(
		DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 size, DirectX::XMFLOAT4 color) {
		if (size.x <= 0 || size.y <= 0 || color.w <= 0) return;

		const DirectX::XMFLOAT2 p[] = {position, {position.x + size.x, position.y},
			{position.x + size.x, position.y + size.y}, {position.x, position.y + size.y}};
		Quad(p, HudFont::SolidUv, HudFont::SolidUv, color);
	}

	void HudDrawList::Line(DirectX::XMFLOAT2 a, DirectX::XMFLOAT2 b, float width, DirectX::XMFLOAT4 color) {
		constexpr float kMinimumLineLengthPixels = 0.001f;
		const float dx = b.x - a.x;
		const float dy = b.y - a.y;
		const float length = std::hypot(dx, dy);
		if (length < kMinimumLineLengthPixels) return;

		// 線分に垂直な方向へ広げて太さを付ける
		const float x = -dy / length * width * 0.5f;
		const float y = dx / length * width * 0.5f;
		const DirectX::XMFLOAT2 p[] = {
			{a.x + x, a.y + y}, {b.x + x, b.y + y}, {b.x - x, b.y - y}, {a.x - x, a.y - y}};
		Quad(p, HudFont::SolidUv, HudFont::SolidUv, color);
	}

	void HudDrawList::CloudPuff(
		DirectX::XMFLOAT2 center, float radius, float rotation, unsigned seed, DirectX::XMFLOAT4 color) {
		if (radius <= 0 || color.w <= 0) return;

		const float c = std::cos(rotation);
		const float s = std::sin(rotation);
		DirectX::XMFLOAT2 points[4];
		const DirectX::XMFLOAT2 corners[] = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
		for (int i = 0; i < 4; ++i)
			points[i] = {center.x + (corners[i].x * c - corners[i].y * s) * radius,
				center.y + (corners[i].x * s + corners[i].y * c) * radius};
		// 負のU座標を手続き的な煙と炎の描画に割り当てる
		const float offset = float(seed % CloudSeedVariants) * 4;
		Quad(points, {-3, offset}, {-1, offset + 2}, color);
	}

	void HudDrawList::SoftDisc(DirectX::XMFLOAT2 center, float radius, DirectX::XMFLOAT4 color) {
		if (m_width == 0 || m_height == 0 || radius <= 0 || color.w <= 0) return;

		if (m_vertices.size() + SoftDiscVertexCount > MaxVertices)
			throw std::length_error("HUD vertex capacity exceeded");
		const auto uv = HudFont::SolidUv;

		const auto vertex = [&](DirectX::XMFLOAT2 point, float alpha) {
			auto tint = color;
			tint.w *= alpha;
			m_vertices.push_back({{point.x * 2 / m_width - 1, 1 - point.y * 2 / m_height}, uv, tint});
		};

		// 中心から外周へ透明になる三角形を並べる
		for (size_t i = 0; i < SoftDiscSegmentCount; ++i) {
			const float a = DirectX::XM_2PI * i / SoftDiscSegmentCount;
			const float b = DirectX::XM_2PI * (i + 1) / SoftDiscSegmentCount;
			vertex(center, 1);
			vertex({center.x + std::cos(a) * radius, center.y + std::sin(a) * radius}, 0);
			vertex({center.x + std::cos(b) * radius, center.y + std::sin(b) * radius}, 0);
		}
	}

	void HudDrawList::Circle(
		DirectX::XMFLOAT2 center, float radius, float progress, float width, DirectX::XMFLOAT4 color) {
		// 進行率に応じた円弧を短い線分で描く
		const float arc = std::clamp(progress, 0.0f, 1.0f) * DirectX::XM_2PI;
		constexpr unsigned kCircleSegments = 64;
		constexpr float segmentRadians = DirectX::XM_2PI / kCircleSegments;
		for (float begin = 0; begin < arc; begin += segmentRadians) {
			const float end = std::min(begin + segmentRadians, arc);
			Line({center.x + std::sin(begin) * radius, center.y - std::cos(begin) * radius},
				{center.x + std::sin(end) * radius, center.y - std::cos(end) * radius}, width, color);
		}
	}

	void HudDrawList::Text(
		DirectX::XMFLOAT2 position, std::string_view text, float height, DirectX::XMFLOAT4 color) {
		while (!text.empty()) {
			// UTF-8から一文字分のグリフを取り出す
			const unsigned glyph = HudFont::Glyph(HudFont::Next(text));

			const float x = float(glyph % HudFont::Columns) * HudFont::Cell;
			const float y = float(glyph / HudFont::Columns) * HudFont::Cell;

			// 文字幅に合わせた四角形へグリフを貼る
			const float width = height * HudFont::GlyphWidth(glyph) / HudFont::Cell;
			const DirectX::XMFLOAT2 p[] = {{position.x, position.y}, {position.x + width, position.y},
				{position.x + width, position.y + height}, {position.x, position.y + height}};
			Quad(p, {x / HudFont::Width, y / HudFont::Height},
				{(x + HudFont::GlyphWidth(glyph)) / HudFont::Width, (y + HudFont::Cell) / HudFont::Height},
				color);
			position.x += height * HudFont::Advance(glyph) / HudFont::Cell;
		}
	}

	float HudDrawList::MeasureTextWidth(std::string_view text, float height) {
		float advance = 0, width = 0;
		while (!text.empty()) {
			const unsigned glyph = HudFont::Glyph(HudFont::Next(text));

			// 最後の文字の右端を文字列の幅にする
			width = advance + height * HudFont::GlyphWidth(glyph) / HudFont::Cell;
			advance += height * HudFont::Advance(glyph) / HudFont::Cell;
		}
		return width;
	}
} // namespace Engine
