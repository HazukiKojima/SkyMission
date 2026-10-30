#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"

namespace Game {
	void CombatEffectPresenter::Build(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
		Engine::HudDrawList& list) const {
		if (view.width == 0 || view.height == 0)
			return;
		EffectDrawContext context(view, list);
		// 煙と火球を先に描き、命中の閃光と金属片を手前へ重ねる
		for (const auto& explosion : snapshot.explosions) {
			if (explosion.age < 0 || explosion.age >= ExplosionEffectSettings::lifetimeSeconds)
				continue;
			if (!context.HasVertexBudget(EffectDrawSettings::explosionVertexBudget))
				continue;
			DrawExplosion(explosion, context, view.height / EffectDrawSettings::referenceHeight);
		}
		for (const auto& impact : snapshot.impacts) {
			if (!context.HasVertexBudget(EffectDrawSettings::impactVertexBudget))
				break;
			if (impact.age < 0 || impact.age >= ImpactEffectSettings::lifetimeSeconds)
				continue;
			DrawImpact(impact, context);
		}
	}
} // namespace Game
