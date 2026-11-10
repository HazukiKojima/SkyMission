#pragma once
#include "Game/UI/HudStyle.h"
#include "Game/Cinematics/CinematicTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "HudProjection.h"

namespace Game {
	class CinematicPresenter {
	  public:
		explicit CinematicPresenter(DirectX::XMFLOAT4 ink = HudStyle::Ink) : m_ink(ink) {}
		void Build(const CinematicPresentation& presentation, const Engine::RenderView& view,
			Engine::HudDrawList& list) const;

	  private:
		DirectX::XMFLOAT4 m_ink;
	};
} // namespace Game
