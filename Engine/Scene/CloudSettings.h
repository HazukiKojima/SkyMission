#pragma once
#include <DirectXMath.h>
namespace Engine {
	struct CloudSettings {
		bool enabled = false;
		float bottom = 4300;
		float top = 13000;
		float coverage = 0.18f;
		float density = 0.0015f;
		float scale = 3200;
		DirectX::XMFLOAT2 wind{18, 7};
		float maxDistance = 40000;
		unsigned steps = 48;
		unsigned seed = 1945;
	};
	void ValidateCloudSettings(const CloudSettings& settings);
} // namespace Engine
