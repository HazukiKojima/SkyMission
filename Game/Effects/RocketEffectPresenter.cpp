#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>

namespace Game {
	void CombatEffectPresenter::DrawRocketEffects(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;
		for (const auto& smoke : snapshot.deckSmoke) {
			if (!context.HasVertexBudget(6))
				break;
			const auto location =
				XMLoadFloat3(&smoke.position) + XMVectorSet(smoke.age * 12, smoke.age * 65, 0, 0);
			const EffectRandom random(smoke.position);
			context.DrawCloud(location, 25 + smoke.age * 32, random.Sample(0) * XM_2PI, 17,
				{0.18f, 0.17f, 0.16f, std::clamp(1 - smoke.age / 4, 0.0f, 1.0f) * 0.6f});
		}
		// 放出済みの煙は機体から独立して残し、時間とともに広げて消す
		for (const auto& smoke : snapshot.rocketSmoke) {
			if (!context.HasVertexBudget(6))
				break;
			const float fade =
				std::clamp(1 - smoke.age / RocketEffectSettings::smokeLifetimeSeconds, 0.0f, 1.0f);
			const EffectRandom random(smoke.position);
			const auto location = XMLoadFloat3(&smoke.position) + XMVectorSet(0, smoke.age * 3, 0, 0);
			context.DrawCloud(location, 1.4f + smoke.age * 7, random.Sample(0) * XM_2PI, 7,
				{0.65f, 0.65f, 0.62f, fade * 0.32f});
		}
		for (const auto& exhaust : snapshot.rocketExhausts) {
			if (!context.HasVertexBudget(222))
				break;
			const auto tail = XMLoadFloat3(&exhaust.position);
			const auto direction = XMLoadFloat3(&exhaust.direction);
			context.DrawGlow(tail, 2.0f, {1, 0.75f, 0.25f, 0.85f});
			context.DrawGlow(tail - direction * 2, 3.0f, {1, 0.25f, 0.02f, 0.5f});
			context.DrawCloud(tail - direction * 4, 3.5f, 0, 31, {1, 0.45f, 0.06f, 0.8f});
		}
	}
} // namespace Game
