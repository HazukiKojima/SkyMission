#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"
#include <algorithm>
#include <cmath>

namespace Game::HudStyle {
	// HUDとレーダーで共有するRGBA色。各成分は0～1の範囲で指定する
	inline const DirectX::XMFLOAT4 Ink{232.0f / 255, 217.0f / 255, 181.0f / 255, 0.95f};
	inline const DirectX::XMFLOAT4 Selected{0.95f, 0.65f, 0.24f, 1};
	inline const DirectX::XMFLOAT4 Alert{0.83f, 0.34f, 0.23f, 1};
	inline const DirectX::XMFLOAT4 Caution{0.85f, 0.64f, 0.32f, 1};
	// RGBを変えずに既存の不透明度へ係数を掛ける。alphaのクランプは行わない
	inline DirectX::XMFLOAT4 Faded(DirectX::XMFLOAT4 color, float alpha) {
		color.w *= alpha;
		return color;
	}
} // namespace Game::HudStyle
