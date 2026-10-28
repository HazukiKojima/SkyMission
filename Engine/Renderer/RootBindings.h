#pragma once
#include "../EngineCommon.h"
namespace Engine {
	enum class RootSignatureLayout {
		Textured,
		NodeRotation
	};
	namespace RootBinding {
		inline constexpr UINT TextureTable = 0;
		inline constexpr UINT FrameConstants = 1;
		inline constexpr UINT NodeRotation = 2;
		inline constexpr UINT NodeRotationDwords = 8; // angle, pivot.xyz, axis.xyz, enabled
		inline constexpr UINT TexturedParameterCount = 2;
		inline constexpr UINT NodeRotationParameterCount = 3;
	} // namespace RootBinding
} // namespace Engine
