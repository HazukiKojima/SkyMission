#pragma once

#include <DirectXMath.h>
#include <cstddef>
#include <type_traits>

namespace Engine {
	struct RetroFilmParameters {
		DirectX::XMFLOAT4 colorDesaturationTint;
		DirectX::XMFLOAT4 colorGainOffset;
		DirectX::XMFLOAT4 blackLift;
		DirectX::XMFLOAT4 highlight;
		DirectX::XMFLOAT4 highlightAddLuminanceR;
		DirectX::XMFLOAT4 luminanceGBVignette;
		DirectX::XMFLOAT4 vignetteJitterX;
		DirectX::XMFLOAT4 jitterYGrain;
		DirectX::XMFLOAT4 dust;
		DirectX::XMFLOAT4 dustScratch;
		DirectX::XMFLOAT4 scratch;
		DirectX::XMFLOAT4 scratchFlicker;
		DirectX::XMFLOAT4 overlayColor;
	};

	static_assert(std::is_standard_layout_v<RetroFilmParameters>);
	static_assert(std::is_trivially_copyable_v<RetroFilmParameters>);
	static_assert(sizeof(DirectX::XMFLOAT4) == sizeof(float) * 4);
	static_assert(sizeof(RetroFilmParameters) == sizeof(float) * 52);
}
