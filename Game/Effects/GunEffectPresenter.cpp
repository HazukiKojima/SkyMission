#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>
#include <cmath>

namespace Game {
	void CombatEffectPresenter::DrawGunSmoke(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;
		const size_t count = std::min(snapshot.gunSmoke.size(), GunEffectSettings::maxSmokeCount);
		// 混雑時は薄い発射煙から省き、ほかの演出と予約済みの発射光を優先する
		const size_t otherBudget = std::min(snapshot.explosions.size(), ExplosionEffectSettings::maxCount) *
				EffectDrawSettings::explosionVertexBudget +
			std::min(snapshot.impacts.size(), ImpactEffectSettings::maxCount) *
				EffectDrawSettings::impactVertexBudget +
			std::min(snapshot.deckSmoke.size(), DeckSmokeSettings::maxCount) * 6;
		if (!context.HasVertexBudget(otherBudget + count * 6)) return;
		for (size_t index = 0; index < count; ++index) {
			if (!context.HasVertexBudget(6)) break;
			const auto& smoke = snapshot.gunSmoke[index];
			if (smoke.age < 0 || smoke.age >= context.Settings().gun.smokeLifetimeSeconds) continue;
			const float progress = smoke.age / context.Settings().gun.smokeLifetimeSeconds;
			// 減衰速度の積分値を使い、FPSに依存しない煙の位置を求める
			const float driftSeconds = (1 - std::exp(-context.Settings().gun.smokeDrag * smoke.age)) /
				context.Settings().gun.smokeDrag;
			const auto position = XMLoadFloat3(&smoke.position) +
				XMLoadFloat3(&smoke.velocity) *
					(context.Settings().gun.smokeVelocityInheritance * driftSeconds) +
				XMVectorSet(0, context.Settings().gun.smokeRiseSpeed * smoke.age, 0, 0);
			const float radius = std::lerp(
				context.Settings().gun.smokeStartRadius, context.Settings().gun.smokeEndRadius, progress);
			const EffectRandom random(smoke.position);
			context.DrawCloud(position, radius, random.Sample(0) * XM_2PI, 11,
				EffectTint(context.Settings().gun.smokeColor,
					context.Settings().gun.smokeOpacity * (1 - progress) * (1 - progress)));
		}
	}

	void CombatEffectPresenter::DrawGunFire(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;
		const float scale = context.GetScale();
		const size_t tracerCount = std::min(snapshot.gunTracers.size(), GunEffectSettings::maxTracerCount);
		for (size_t index = 0; index < tracerCount; ++index) {
			if (!context.HasVertexBudget(GunEffectSettings::tracerVertexBudget)) break;
			const auto& tracer = snapshot.gunTracers[index];
			if (const auto segment = context.ProjectClippedSegment(tracer.head, tracer.tail))
				context.DrawStreak(*segment, context.Settings().gun.tracerOuterWidth * scale,
					context.Settings().gun.tracerOuterColor, context.Settings().gun.tracerInnerWidth * scale,
					context.Settings().gun.tracerInnerColor);
		}

		const size_t flashCount = std::min(snapshot.gunFlashes.size(), GunEffectSettings::maxFlashCount);
		for (size_t index = 0; index < flashCount; ++index) {
			if (!context.HasVertexBudget(GunEffectSettings::flashVertexBudget)) break;
			const auto& flash = snapshot.gunFlashes[index];
			if (flash.age < 0 || flash.age >= context.Settings().gun.flashLifetimeSeconds) continue;
			const float fade = 1 - flash.age / context.Settings().gun.flashLifetimeSeconds;
			// 発射ごとの形を固定し、機体の移動による乱数のちらつきを防ぐ
			const float phase = float(flash.sequence % 1024) * 1.73f + float(flash.muzzleIndex) * 2.41f;
			const float variation = 0.9f + 0.1f * std::sin(phase);
			const auto muzzle = XMLoadFloat3(&flash.position);
			const auto forward = XMVector3Normalize(XMLoadFloat3(&flash.direction));
			const float radius = context.Settings().gun.flashRadius * variation;
			context.DrawGlow(muzzle, radius, EffectTint(context.Settings().gun.flashOuterColor, fade));
			context.DrawGlow(
				muzzle, radius * 0.45f, EffectTint(context.Settings().gun.flashInnerColor, fade));
			const auto flameTip = muzzle + forward * (context.Settings().gun.flameLength * variation);
			context.DrawCloud(muzzle + forward * (context.Settings().gun.flameLength * 0.4f), radius * 0.65f,
				phase, 31, EffectTint(context.Settings().gun.flameColor, fade));
			XMFLOAT3 tip;
			XMStoreFloat3(&tip, flameTip);
			if (const auto segment = context.ProjectClippedSegment(tip, flash.position))
				context.DrawStreak(*segment, 5 * scale,
					EffectTint(context.Settings().gun.flameOuterColor, fade), 2 * scale,
					EffectTint(context.Settings().gun.flameInnerColor, fade));
		}
	}
} // namespace Game
