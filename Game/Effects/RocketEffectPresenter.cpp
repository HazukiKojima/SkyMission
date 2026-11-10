#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>

namespace Game {
	void CombatEffectPresenter::DrawRocketEffects(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;
		for (const auto& smoke : snapshot.deckSmoke) {
			if (!context.HasVertexBudget(6)) break;
			const auto location = XMLoadFloat3(&smoke.position) +
				XMVectorSet(smoke.age * context.Settings().carrier.smokeDriftSpeed,
					smoke.age * context.Settings().carrier.smokeRiseSpeed, 0, 0);
			const EffectRandom random(smoke.position);
			context.DrawCloud(location,
				context.Settings().carrier.smokeStartRadius +
					smoke.age * context.Settings().carrier.smokeGrowthPerSecond,
				random.Sample(0) * XM_2PI, 17,
				EffectTint(context.Settings().carrier.smokeColor,
					std::clamp(1 - smoke.age / context.Settings().carrier.smokeLifetimeSeconds, 0.0f, 1.0f) *
						context.Settings().carrier.smokeOpacity));
		}
		// 放出済みの煙は機体から独立して残し、時間とともに広げて消す
		for (const auto& smoke : snapshot.rocketSmoke) {
			if (!context.HasVertexBudget(6)) break;
			const float fade =
				std::clamp(1 - smoke.age / context.Settings().rocket.smokeLifetimeSeconds, 0.0f, 1.0f);
			const EffectRandom random(smoke.position);
			const auto location = XMLoadFloat3(&smoke.position) +
				XMVectorSet(0, smoke.age * context.Settings().rocket.smokeRiseSpeed, 0, 0);
			context.DrawCloud(location,
				context.Settings().rocket.smokeStartRadius +
					smoke.age * context.Settings().rocket.smokeGrowthPerSecond,
				random.Sample(0) * XM_2PI, 7,
				EffectTint(
					context.Settings().rocket.smokeColor, fade * context.Settings().rocket.smokeOpacity));
		}
		for (const auto& exhaust : snapshot.rocketExhausts) {
			if (!context.HasVertexBudget(222)) break;
			const auto tail = XMLoadFloat3(&exhaust.position);
			const auto direction = XMLoadFloat3(&exhaust.direction);
			context.DrawGlow(tail, 2.0f, context.Settings().rocket.exhaustCoreColor);
			context.DrawGlow(tail - direction * 2, 3.0f, context.Settings().rocket.exhaustGlowColor);
			context.DrawCloud(tail - direction * 4, 3.5f, 0, 31, context.Settings().rocket.exhaustFlameColor);
		}
	}
} // namespace Game
