#pragma once
#include "CombatEffectTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"

namespace Game {
	class EffectDrawContext;
	class CombatEffectPresenter {
	  public:
		// 呼び出し側で初期化した描画リストに追記し、照準より先に描く
		void Build(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
			Engine::HudDrawList& list) const;

	  private:
		void DrawExplosion(
			const ExplosionEffect& effect, EffectDrawContext& context, float fragmentScale) const;
		void DrawRocketEffects(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
		void DrawImpact(const ImpactEffect& effect, EffectDrawContext& context) const;
		void DrawGunSmoke(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
		void DrawGunFire(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
	};
} // namespace Game
