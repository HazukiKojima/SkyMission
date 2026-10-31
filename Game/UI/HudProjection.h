#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"

namespace Game {
	struct HudProjection {
		DirectX::XMFLOAT2 pixel;
		bool visible = false;
		bool behind = false;
	};

	class HudProjector {
	  public:
		explicit HudProjector(const Engine::RenderView& view);
		HudProjection Project(const DirectX::XMFLOAT3& point) const;

	  private:
		DirectX::XMFLOAT4X4 m_viewProjection;
		unsigned m_width;
		unsigned m_height;
	};

	HudProjection ProjectHudPoint(const DirectX::XMFLOAT3& point, const Engine::RenderView& view);
} // namespace Game
