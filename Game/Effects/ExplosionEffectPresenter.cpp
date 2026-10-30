#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;
	void CombatEffectPresenter::DrawExplosion(
		const ExplosionEffect& effect, EffectDrawContext& context, float fragmentScale) const {
		const float ageSeconds = effect.age, radiusScale = effect.radius;
		const auto origin = XMLoadFloat3(&effect.position);
		const auto drift = XMLoadFloat3(&effect.velocity) * (0.18f * (1 - std::exp(-ageSeconds * 2)));
		const auto drawCloud = [&](XMVECTOR location, float radius, XMFLOAT4 color, unsigned seed) {
			const float rotation = seed * 1.71f + ageSeconds * (seed % 2 ? 0.12f : -0.09f);
			context.DrawCloud(location, radius, rotation, seed, color);
		};
		// 大きさと上昇速度の異なる煙塊を重ねて乱流を表現する
		for (int i = 0; i < ExplosionEffectSettings::smokeCount; ++i) {
			const float phase = i * EffectDrawSettings::goldenAngleRadians,
						variation = 0.55f + (i % 7) * 0.11f;
			const float spread = radiusScale * (0.2f + (1 - std::exp(-ageSeconds * 1.4f)) * 1.3f) * variation;
			const auto location = origin + drift +
				XMVectorSet(std::cos(phase) * spread,
					std::sin(phase) * spread * 0.6f + ageSeconds * radiusScale * (0.35f + (i % 4) * 0.12f),
					std::sin(phase * 2) * spread * 0.5f, 0);
			const float alpha = std::min(ageSeconds * 5, 0.85f) *
				std::clamp((ExplosionEffectSettings::lifetimeSeconds - ageSeconds) /
						ExplosionEffectSettings::smokeFadeSeconds,
					0.0f, 1.0f);
			const float shade = 0.09f + (i % 5) * 0.025f;
			drawCloud(location, radiusScale * (0.38f + ageSeconds * 0.27f) * variation,
				{shade, shade * 0.95f, shade * 0.88f, alpha}, i + 1);
		}
		// 火炎の塊ごとに着火時刻と燃焼時間を変え、輪郭を崩す
		for (int i = 0; i < ExplosionEffectSettings::fireCount; ++i) {
			const float delay = i == 0 ? 0 : (i % 5) * 0.025f;
			const float particleAgeSeconds = ageSeconds - delay, lifetimeSeconds = 0.48f + (i % 5) * 0.095f;
			if (particleAgeSeconds < 0 || particleAgeSeconds >= lifetimeSeconds)
				continue;
			const float heat = 1 - particleAgeSeconds / lifetimeSeconds,
						phase = i * EffectDrawSettings::goldenAngleRadians;
			const float spread = radiusScale * (0.15f + particleAgeSeconds * 1.15f) * (i == 0 ? 0 : 0.9f);
			const auto location = origin + drift +
				XMVectorSet(std::cos(phase) * spread,
					std::sin(phase) * spread * 0.8f + particleAgeSeconds * radiusScale * 0.55f,
					std::sin(phase * 2) * spread * 0.4f, 0);
			const float radius =
				radiusScale * (0.28f + std::sqrt(particleAgeSeconds) * 0.5f) * (0.8f + (i % 3) * 0.22f);
			drawCloud(
				location, radius, {1, 0.27f + heat * 0.35f, 0.025f + heat * 0.09f, heat * 0.95f}, i + 27);
		}
		// 白熱はごく短くし、均一な衝撃波の輪は描かない
		if (ageSeconds < ExplosionEffectSettings::flashSeconds) {
			const float flash = 1 - ageSeconds / ExplosionEffectSettings::flashSeconds;
			context.DrawGlow(origin, radiusScale * (0.65f + ageSeconds * 2), {1, 0.85f, 0.55f, flash * 0.8f});
		}
		// 重力で落下する破片と燃える軌跡
		for (int i = 0; i < ExplosionEffectSettings::fragmentCount; ++i) {
			const float lifetimeSeconds = 1.1f + (i % 7) * 0.22f;
			if (ageSeconds >= lifetimeSeconds)
				continue;
			const float phase = i * EffectDrawSettings::goldenAngleRadians, y = std::sin(i * 17.31f) * 0.85f;
			const float radial = std::sqrt(1 - y * y), speed = radiusScale * (0.8f + (i % 9) * 0.21f);
			const auto velocity =
				XMVectorSet(std::cos(phase) * radial * speed, y * speed, std::sin(phase) * radial * speed, 0);
			const auto positionAt = [&](float particleAgeSeconds) {
				return origin +
					XMLoadFloat3(&effect.velocity) *
					(particleAgeSeconds * ExplosionEffectSettings::fragmentVelocityInheritance) +
					velocity * particleAgeSeconds +
					XMVectorSet(0,
						-ExplosionEffectSettings::fragmentGravity * particleAgeSeconds * particleAgeSeconds,
						0, 0);
			};
			const auto segment = context.ProjectSegment(positionAt(ageSeconds),
				positionAt(std::max(0.0f, ageSeconds - ExplosionEffectSettings::fragmentTrailSeconds)));
			if (!segment)
				continue;
			const float fade = 1 - ageSeconds / lifetimeSeconds;
			if (i % 3 == 0) {
				for (int puff = 1; puff <= 3; ++puff) {
					const float particleAgeSeconds = std::max(0.0f, ageSeconds - puff * 0.07f);
					drawCloud(positionAt(particleAgeSeconds), radiusScale * (0.035f + puff * 0.014f),
						{0.12f, 0.11f, 0.1f, fade * 0.5f}, i + puff + 3);
				}
			}
			context.DrawStreak(*segment, std::max(1.0f, fragmentScale * 3), {1, 0.25f, 0.015f, fade * 0.65f},
				std::max(0.7f, fragmentScale * 1.2f), {1, 0.85f, 0.4f, fade});
		}
	}
} // namespace Game
