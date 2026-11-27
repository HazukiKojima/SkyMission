#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>

namespace Game {
	void CombatEffectPresenter::DrawRocketEffects(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;

		// 放出済みの煙は機体から独立して残し、時間とともに広げて消す
		for (const auto& smoke : snapshot.rocketSmoke) {
			if (!context.HasVertexBudget(Engine::HudDrawList::QuadVertexCount)) break;

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

		// 飛行中のロケット後端へ発光と炎を追従させる
		constexpr size_t kExhaustVertexReservation = 222;
		constexpr float kExhaustCoreRadius = 2.0f;
		constexpr float kExhaustGlowOffset = 2.0f;
		constexpr float kExhaustGlowRadius = 3.0f;
		constexpr float kExhaustFlameOffset = 4.0f;
		constexpr float kExhaustFlameRadius = 3.5f;
		constexpr unsigned kExhaustFlameTextureSeed = 31;
		for (const auto& exhaust : snapshot.rocketExhausts) {
			if (!context.HasVertexBudget(kExhaustVertexReservation)) break;

			const auto tail = XMLoadFloat3(&exhaust.position);
			const auto direction = XMLoadFloat3(&exhaust.direction);
			context.DrawGlow(tail, kExhaustCoreRadius, context.Settings().rocket.exhaustCoreColor);
			context.DrawGlow(tail - direction * kExhaustGlowOffset, kExhaustGlowRadius,
				context.Settings().rocket.exhaustGlowColor);
			context.DrawCloud(tail - direction * kExhaustFlameOffset, kExhaustFlameRadius, 0, kExhaustFlameTextureSeed,
				context.Settings().rocket.exhaustFlameColor);
		}
	}
} // namespace Game
