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
			std::min(snapshot.rocketSmoke.size(), RocketEffectSettings::maxSmokeCount) * 6 +
			std::min(snapshot.rocketExhausts.size(), RocketEffectSettings::maxExhaustCount) * 222;
		if (!context.HasVertexBudget(otherBudget + count * 6))
			return;
		for (size_t index = 0; index < count; ++index) {
			if (!context.HasVertexBudget(6))
				break;
			const auto& smoke = snapshot.gunSmoke[index];
			if (smoke.age < 0 || smoke.age >= GunEffectSettings::smokeLifetimeSeconds)
				continue;
			const float progress = smoke.age / GunEffectSettings::smokeLifetimeSeconds;
			// 減衰速度の積分値を使い、FPSに依存しない煙の位置を求める
			const float driftSeconds =
				(1 - std::exp(-GunEffectSettings::smokeDrag * smoke.age)) / GunEffectSettings::smokeDrag;
			const auto position = XMLoadFloat3(&smoke.position) +
				XMLoadFloat3(&smoke.velocity) * (GunEffectSettings::smokeVelocityInheritance * driftSeconds) +
				XMVectorSet(0, GunEffectSettings::smokeRiseSpeed * smoke.age, 0, 0);
			const float radius =
				std::lerp(GunEffectSettings::smokeStartRadius, GunEffectSettings::smokeEndRadius, progress);
			const EffectRandom random(smoke.position);
			context.DrawCloud(position, radius, random.Sample(0) * XM_2PI, 11,
				{0.62f, 0.61f, 0.58f, GunEffectSettings::smokeOpacity * (1 - progress) * (1 - progress)});
		}
	}

	void CombatEffectPresenter::DrawGunFire(
		const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const {
		using namespace DirectX;
		const float scale = context.GetScale();
		const size_t tracerCount = std::min(snapshot.gunTracers.size(), GunEffectSettings::maxTracerCount);
		for (size_t index = 0; index < tracerCount; ++index) {
			if (!context.HasVertexBudget(GunEffectSettings::tracerVertexBudget))
				break;
			const auto& tracer = snapshot.gunTracers[index];
			if (const auto segment = context.ProjectClippedSegment(tracer.head, tracer.tail))
				context.DrawStreak(*segment, GunEffectSettings::tracerOuterWidth * scale,
					{1, 0.32f, 0.035f, 0.75f}, GunEffectSettings::tracerInnerWidth * scale,
					{1, 0.95f, 0.65f, 0.95f});
		}

		const size_t flashCount = std::min(snapshot.gunFlashes.size(), GunEffectSettings::maxFlashCount);
		for (size_t index = 0; index < flashCount; ++index) {
			if (!context.HasVertexBudget(GunEffectSettings::flashVertexBudget))
				break;
			const auto& flash = snapshot.gunFlashes[index];
			if (flash.age < 0 || flash.age >= GunEffectSettings::flashLifetimeSeconds)
				continue;
			const float fade = 1 - flash.age / GunEffectSettings::flashLifetimeSeconds;
			// 発射ごとの形を固定し、機体の移動による乱数のちらつきを防ぐ
			const float phase = float(flash.sequence % 1024) * 1.73f + float(flash.muzzleIndex) * 2.41f;
			const float variation = 0.9f + 0.1f * std::sin(phase);
			const auto muzzle = XMLoadFloat3(&flash.position);
			const auto forward = XMVector3Normalize(XMLoadFloat3(&flash.direction));
			const float radius = GunEffectSettings::flashRadius * variation;
			context.DrawGlow(muzzle, radius, {1, 0.35f, 0.035f, fade * 0.8f});
			context.DrawGlow(muzzle, radius * 0.45f, {1, 0.98f, 0.7f, fade});
			const auto flameTip = muzzle + forward * (GunEffectSettings::flameLength * variation);
			context.DrawCloud(muzzle + forward * (GunEffectSettings::flameLength * 0.4f), radius * 0.65f,
				phase, 31, {1, 0.55f, 0.08f, fade * 0.75f});
			XMFLOAT3 tip;
			XMStoreFloat3(&tip, flameTip);
			if (const auto segment = context.ProjectClippedSegment(tip, flash.position))
				context.DrawStreak(
					*segment, 5 * scale, {1, 0.4f, 0.04f, fade * 0.65f}, 2 * scale, {1, 0.95f, 0.65f, fade});
		}
	}
} // namespace Game
