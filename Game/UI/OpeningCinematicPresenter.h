#pragma once
#include "Game/Cinematics/OpeningCinematicTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "HudProjection.h"

namespace Game {
	class OpeningCinematicPresenter {
	  public:
		void Build(const OpeningCinematicPresentation& presentation, const Engine::RenderView& view,
			Engine::HudDrawList& list) const;
	};
} // namespace Game
