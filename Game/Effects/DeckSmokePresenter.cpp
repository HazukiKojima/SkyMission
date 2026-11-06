#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>

namespace Game {
	void CombatEffectPresenter::DrawDeckSmoke(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;
		for (const auto& smoke : snapshot.deckSmoke) {
			if (!context.HasVertexBudget(6)) break;
			const auto location =
				XMLoadFloat3(&smoke.position) + XMVectorSet(smoke.age * 12, smoke.age * 65, 0, 0);
			const EffectRandom random(smoke.position);
			context.DrawCloud(location, 25 + smoke.age * 32, random.Sample(0) * XM_2PI, 17,
				{0.18f, 0.17f, 0.16f,
					std::clamp(1 - smoke.age / DeckSmokeSettings::lifetimeSeconds, 0.0f, 1.0f) * 0.6f});
		}
	}
} // namespace Game
