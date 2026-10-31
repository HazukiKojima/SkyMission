#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"
#include <optional>

namespace Game {
	struct HudSegmentProjection {
		DirectX::XMFLOAT2 head;
		DirectX::XMFLOAT2 tail;
	};
	struct HudProjection {
		DirectX::XMFLOAT2 pixel;
		bool visible = false;
		bool behind = false;
	};

	class HudProjector {
	  public:
		explicit HudProjector(const Engine::RenderView& view);
		HudProjection Project(const DirectX::XMFLOAT3& point) const;
		std::optional<HudSegmentProjection> ProjectClippedSegment(
			const DirectX::XMFLOAT3& head, const DirectX::XMFLOAT3& tail) const;

	  private:
		DirectX::XMFLOAT4X4 m_viewProjection;
		unsigned m_width;
		unsigned m_height;
	};

	HudProjection ProjectHudPoint(const DirectX::XMFLOAT3& point, const Engine::RenderView& view);
} // namespace Game
