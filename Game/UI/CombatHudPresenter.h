#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {
	struct HudProjection { DirectX::XMFLOAT2 pixel; bool visible = false, behind = false; };
	HudProjection ProjectHudPoint(const DirectX::XMFLOAT3& point, const Engine::RenderView& view);
	class CombatHudPresenter {
	public:
		void Configure(const Engine::HudSettings& settings) { m_settings = settings; }
		void Build(const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
	private:
		Engine::HudSettings m_settings;
	};
}
