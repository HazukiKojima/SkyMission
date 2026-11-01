#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"
#include <algorithm>
#include <cmath>

namespace Game::HudStyle {
	inline const DirectX::XMFLOAT4 Ink{230.0f / 255, 205.0f / 255, 164.0f / 255, 0.95f};
	inline const DirectX::XMFLOAT4 Selected{0.95f, 0.65f, 0.24f, 1};
	inline const DirectX::XMFLOAT4 Alert{0.83f, 0.34f, 0.23f, 1};
	inline const DirectX::XMFLOAT4 Caution{0.85f, 0.64f, 0.32f, 1};
	inline DirectX::XMFLOAT4 Faded(DirectX::XMFLOAT4 color, float alpha) {
		color.w *= alpha;
		return color;
	}
	// 装飾は画面端の細線と擦れだけに限定し、3D画面の色調には手を加えない
	inline void DrawFrame(const Engine::RenderView& view, Engine::HudDrawList& list, DirectX::XMFLOAT4 ink) {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float width = float(view.width), height = float(view.height);
		if (scale <= 0)
			return;
		for (float inset : {4.0f, 9.0f}) {
			const float d = inset * scale;
			const DirectX::XMFLOAT2 corners[] = {
				{d, d}, {width - d, d}, {width - d, height - d}, {d, height - d}};
			for (size_t i = 0; i < std::size(corners); ++i)
				list.Line(corners[i], corners[(i + 1) % std::size(corners)],
					std::max(0.7f, scale * (inset == 4 ? 2.2f : 0.75f)),
					Faded(ink, inset == 4 ? 0.9f : 0.25f));
		}
		for (int i = 0; i < 160; ++i) {
			const int edge = i % 4;
			const float along = (float(i / 4) + 0.35f + float((i * 17) % 11) / 22) / 40;
			const float depth = (2 + float((i * 13) % 9)) * scale;
			const float length = (4 + float((i * 7) % 20)) * scale;
			DirectX::XMFLOAT2 a, b;
			if (edge < 2) {
				a = {along * width, edge == 0 ? depth : height - depth};
				b = {a.x + length, a.y + (edge == 0 ? 1 : -1) * 1.5f * scale};
			} else {
				a = {edge == 2 ? depth : width - depth, along * height};
				b = {a.x + (edge == 2 ? 1 : -1) * 1.5f * scale, a.y + length};
			}
			list.Line(a, b, std::max(0.7f, scale * 1.2f), Faded(ink, 0.35f + float(i % 4) * 0.12f));
		}
		for (float sx : {-1.0f, 1.0f})
			for (float sy : {-1.0f, 1.0f})
				for (int i = 0; i < 16; ++i) {
					const float x = (5 + float((i * 7) % 22)) * scale;
					const float y = (5 + float((i * 11) % 22)) * scale;
					const DirectX::XMFLOAT2 a{sx < 0 ? x : width - x, sy < 0 ? y : height - y};
					list.Line(a, {a.x - sx * (2 + i % 4) * scale, a.y + sy * (2 + i % 3) * scale},
						std::max(0.7f, scale * 1.5f), Faded(ink, 0.6f));
				}
	}
} // namespace Game::HudStyle
