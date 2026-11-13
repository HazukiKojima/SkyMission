#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>

namespace Game {
	void CombatEffectPresenter::DrawDeckSmoke(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;
		for (const auto& smoke : snapshot.deckSmoke) {
			if (!context.HasVertexBudget(6)) break;
			const auto location = XMLoadFloat3(&smoke.position) +
				XMVectorSet(smoke.age * context.Settings().deck.driftSpeed,
					smoke.age * context.Settings().deck.riseSpeed, 0, 0);
			const EffectRandom random(smoke.position);
			context.DrawCloud(location,
				context.Settings().deck.startRadius + smoke.age * context.Settings().deck.growthPerSecond,
				random.Sample(0) * XM_2PI, 17,
				EffectTint(context.Settings().deck.smokeColor,
					std::clamp(1 - smoke.age / context.Settings().carrier.smokeLifetimeSeconds, 0.0f, 1.0f) *
						context.Settings().carrier.smokeOpacity));
		}
	}
} // namespace Game
