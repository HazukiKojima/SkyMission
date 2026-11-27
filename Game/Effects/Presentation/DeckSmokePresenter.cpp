#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"

namespace Game {
	void CombatEffectPresenter::DrawDeckSmoke(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		if (!snapshot.carrierSmoke || snapshot.carrierSmoke->age < 0) return;

		const auto requiredVertices = context.Settings().explosion.smokeCount *
			carrierSmokeCountMultiplier * Engine::HudDrawList::QuadVertexCount;
		if (!context.HasVertexBudget(requiredVertices)) return;

		const auto& smoke = *snapshot.carrierSmoke;
		DrawExplosionSmoke({.position = smoke.position, .age = smoke.age,
			.radius = context.Settings().carrier.explosionRadius}, context, true);
	}
} // namespace Game
