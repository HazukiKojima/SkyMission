#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "Game/Combat/CombatTypes.h"
#include "HudProjection.h"

namespace Game {
	class CombatHudPresenter {
	  public:
		void Configure(const Engine::HudSettings& settings) {
			m_settings = settings;
		}
		// エフェクト描画後のリストへ照準と文字を追記する
		void Build(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;

	  private:
		Engine::HudSettings m_settings;
	};
} // namespace Game
