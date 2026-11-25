#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>

namespace Game {
	void CombatEffectPresenter::Build(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
		Engine::HudDrawList& list) const {
		if (view.width == 0 || view.height == 0) return;

		EffectDrawContext context(view, list, m_settings);

		// 閃光と曳光弾の領域を先に確保し、煙で発射の手応えを押し出さない
		const auto gunBudget = std::min(snapshot.gunFlashes.size(), GunEffectSettings::maxFlashCount) *
				GunEffectSettings::flashVertexBudget +
			std::min(snapshot.gunTracers.size(), GunEffectSettings::maxTracerCount) *
				GunEffectSettings::tracerVertexBudget;
		context.SetAdditionalReservedVertices(gunBudget);
		DrawGunSmoke(snapshot, context);

		// 煙と火球を先に描き、命中の閃光と金属片を手前へ重ねる
		for (const auto& explosion : snapshot.explosions) {
			if (explosion.age < 0 || explosion.age >= context.Settings().explosion.lifetimeSeconds) continue;
			if (!context.HasVertexBudget(EffectDrawSettings::explosionVertexBudget)) continue;

			DrawExplosion(explosion, context, view.height / EffectDrawSettings::referenceHeight);
		}

		DrawDeckSmoke(snapshot, context);
		DrawRocketEffects(snapshot, context);
		context.SetAdditionalReservedVertices(0);
		DrawGunFire(snapshot, context);

		for (const auto& impact : snapshot.impacts) {
			if (!context.HasVertexBudget(EffectDrawSettings::impactVertexBudget)) break;
			if (impact.age < 0 || impact.age >= context.Settings().impact.lifetimeSeconds) continue;

			DrawImpact(impact, context);
		}

		for (const auto& vapor : snapshot.wingtipVapor) {
			if (!context.HasVertexBudget(12)) break;
			const auto segment = context.ProjectClippedSegment(vapor.head, vapor.tail);
			if (!segment) continue;

			const float remaining = 1.0f - vapor.age / WingtipVaporSegment::lifetimeSeconds;
			const float opacity = vapor.strength * remaining * remaining;
			const float width = (2.4f + (1.0f - remaining) * 1.6f) * context.GetScale();
			context.DrawStreak(*segment, width, {0.85f, 0.92f, 1.0f, opacity * 0.18f},
				width * 0.5f, {1.0f, 1.0f, 1.0f, opacity * 0.42f});
		}
	}
} // namespace Game
