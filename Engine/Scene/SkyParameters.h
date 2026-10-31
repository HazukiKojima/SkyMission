#pragma once

namespace Engine {
	struct SkyParameters {
		float radius;
		float sunDirection[3];
		float sunIntensity;
		float sunColor[3];
		float ambientIntensity;
		float ambientColor[3];
	};
} // namespace Engine
