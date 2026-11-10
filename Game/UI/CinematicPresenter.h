#pragma once
#include "Game/Cinematics/CinematicTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "HudProjection.h"

namespace Game {
	class CinematicPresenter {
	  public:
		void Build(const CinematicPresentation& presentation, const Engine::RenderView& view,
			Engine::HudDrawList& list) const;
	};
} // namespace Game
