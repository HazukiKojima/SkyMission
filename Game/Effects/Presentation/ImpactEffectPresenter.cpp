#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;

	namespace {
		constexpr float kMaximumNormalYForUpReference = 0.9f;

		void DrawImpactSmoke(const ImpactEffect& impact, EffectDrawContext& context) {
			constexpr float kSmokeSpinRate = 0.7f;
			constexpr float kSmokeSpreadRate = 9.0f;
			constexpr float kSmokeVelocityInheritance = 0.45f;
			constexpr float kSmokeSurfaceOffset = 1.0f;
			constexpr float kSmokeOutwardSpeed = 5.0f;
			constexpr float kSmokeRiseSpeed = 2.0f;
			constexpr float kSmokeOpacityGrowthRate = 24.0f;
			constexpr float kSmokeInitialRadius = 2.0f;
			constexpr float kSmokeMinimumRadiusGrowth = 9.0f;
			constexpr float kSmokeRadiusGrowthRange = 7.0f;
			// 入射方向の逆を基準に煙の広がる平面を作る
			const float ageSeconds = impact.age;
			const auto origin = XMLoadFloat3(&impact.position);
			const auto inherited = XMLoadFloat3(&impact.velocity);
			const auto normal = -XMVector3Normalize(XMLoadFloat3(&impact.direction));
			const auto reference =
				std::abs(XMVectorGetY(normal)) < kMaximumNormalYForUpReference ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
			const auto tangent = XMVector3Normalize(XMVector3Cross(normal, reference));
			const auto bitangent = XMVector3Cross(normal, tangent);
			const EffectRandom random(impact.position);
			const auto drawSmoke = [&](XMVECTOR location, float radius, XMFLOAT4 color, unsigned seed) {
				context.DrawCloud(location, radius, random.Sample(seed) * XM_2PI + ageSeconds * kSmokeSpinRate, seed,
					color, EffectClipMode::CenterVisible);
			};

			// 小さな灰色の煙を残し、火球にはしない
			for (unsigned i = 0; i < context.Settings().impact.smokeCount; ++i) {
				const auto offset = tangent * (random.Sample(i + 2) - 0.5f) * ageSeconds * kSmokeSpreadRate +
					bitangent * (random.Sample(i + 12) - 0.5f) * ageSeconds * kSmokeSpreadRate;
				const auto location = origin + inherited * (kSmokeVelocityInheritance * ageSeconds) +
					normal * (kSmokeSurfaceOffset + ageSeconds * kSmokeOutwardSpeed) + offset +
					XMVectorSet(0, ageSeconds * kSmokeRiseSpeed, 0, 0);
				const float alpha = std::min(
					ageSeconds * kSmokeOpacityGrowthRate, context.Settings().impact.smokeMaxOpacity) *
					std::clamp((context.Settings().impact.lifetimeSeconds - ageSeconds) /
							context.Settings().impact.smokeFadeSeconds,
						0.0f, 1.0f);
				const float radius = kSmokeInitialRadius +
					ageSeconds * (kSmokeMinimumRadiusGrowth + random.Sample(i + 22) * kSmokeRadiusGrowthRange);
				drawSmoke(location, radius,
					EffectTint(context.Settings().impact.smokeColor, alpha), i + 7);
			}
		}

		void DrawImpactFlash(const ImpactEffect& impact, EffectDrawContext& context) {
			const float ageSeconds = impact.age;
			const auto origin = XMLoadFloat3(&impact.position);
			const auto inherited = XMLoadFloat3(&impact.velocity);

			if (ageSeconds < context.Settings().impact.flashSeconds) {
				const float flash = 1 - ageSeconds / context.Settings().impact.flashSeconds;
				context.DrawGlow(origin + inherited * ageSeconds, context.Settings().impact.flashOuterRadius,
					EffectTint(context.Settings().impact.flashOuterColor, flash),
					EffectClipMode::CenterVisible);
				context.DrawGlow(origin + inherited * ageSeconds, context.Settings().impact.flashInnerRadius,
					EffectTint(context.Settings().impact.flashInnerColor, flash),
					EffectClipMode::CenterVisible);
			}
		}

		void DrawImpactFragments(const ImpactEffect& impact, EffectDrawContext& context) {
			constexpr float kMinimumOutwardComponent = 0.2f;
			constexpr float kOutwardComponentRange = 0.8f;
			constexpr float kMinimumTrailSeconds = 0.012f;
			constexpr float kTrailDurationRangeSeconds = 0.024f;
			constexpr float kMinimumTrailPixelWidth = 1.1f;
			constexpr float kBaseTrailWidth = 1.8f;
			constexpr float kTrailWidthRange = 2.2f;
			const float scale = context.GetScale();

			// 入射方向の逆を基準にして、金属片を機体の外側へ散らす
			const float ageSeconds = impact.age;
			const auto origin = XMLoadFloat3(&impact.position);
			const auto inherited = XMLoadFloat3(&impact.velocity);
			const auto normal = -XMVector3Normalize(XMLoadFloat3(&impact.direction));
			const auto reference =
				std::abs(XMVectorGetY(normal)) < kMaximumNormalYForUpReference ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
			const auto tangent = XMVector3Normalize(XMVector3Cross(normal, reference));
			const auto bitangent = XMVector3Cross(normal, tangent);
			const EffectRandom random(impact.position);

			for (int i = 0; i < context.Settings().impact.fragmentCount; ++i) {
				const float lifetimeSeconds = context.Settings().impact.fragmentMinLifetime +
					random.Sample(i + 32) * context.Settings().impact.fragmentLifetimeRange;
				if (ageSeconds >= lifetimeSeconds) continue;

				const float angle = random.Sample(i + 52) * XM_2PI;
				const float outward = kMinimumOutwardComponent + random.Sample(i + 72) * kOutwardComponentRange;
				const auto direction = XMVector3Normalize(
					normal * outward + tangent * std::cos(angle) + bitangent * std::sin(angle));
				const float speed = context.Settings().impact.fragmentMinSpeed +
					random.Sample(i + 92) * context.Settings().impact.fragmentSpeedRange;
				const auto positionAt = [&](float particleAgeSeconds) {
					const auto inheritedDisplacement = inherited *
						(particleAgeSeconds * context.Settings().impact.fragmentVelocityInheritance);
					const float travelDistance = speed *
						(1 - std::exp(-particleAgeSeconds * context.Settings().impact.fragmentDrag)) /
						context.Settings().impact.fragmentDrag;
					const auto fragmentDisplacement = direction * travelDistance;
					const auto gravityDisplacement = XMVectorSet(0,
						-context.Settings().impact.fragmentGravity * particleAgeSeconds * particleAgeSeconds,
						0, 0);
					return origin + inheritedDisplacement + fragmentDisplacement + gravityDisplacement;
				};

				const float trailSeconds = kMinimumTrailSeconds + random.Sample(i + 112) * kTrailDurationRangeSeconds;
				const auto segment = context.ProjectSegment(
					positionAt(ageSeconds), positionAt(std::max(0.0f, ageSeconds - trailSeconds)));
				if (!segment) continue;

				const float heat = 1 - ageSeconds / lifetimeSeconds;
				const float width = std::max(
					kMinimumTrailPixelWidth, scale * (kBaseTrailWidth + random.Sample(i + 132) * kTrailWidthRange));
				context.DrawStreak(*segment, width * 2,
					EffectTint(context.Settings().impact.fragmentOuterColor, heat), width,
					EffectTint(BlendEffectColor(context.Settings().impact.fragmentColdColor,
								   context.Settings().impact.fragmentHotColor, heat),
						std::sqrt(heat)));
			}
		}

	} // namespace

	void CombatEffectPresenter::DrawImpact(const ImpactEffect& impact, EffectDrawContext& context) const {
		DrawImpactSmoke(impact, context);
		DrawImpactFlash(impact, context);
		DrawImpactFragments(impact, context);
	}
} // namespace Game
