#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;
	namespace {
		void DrawImpactSmoke(const ImpactEffect& impact, EffectDrawContext& context) {
			// 入射方向の逆を基準にして、金属片を機体の外側へ散らす
			const float ageSeconds = impact.age;
			const auto origin = XMLoadFloat3(&impact.position);
			const auto inherited = XMLoadFloat3(&impact.velocity);
			const auto normal = -XMVector3Normalize(XMLoadFloat3(&impact.direction));
			const auto reference =
				std::abs(XMVectorGetY(normal)) < 0.9f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
			const auto tangent = XMVector3Normalize(XMVector3Cross(normal, reference));
			const auto bitangent = XMVector3Cross(normal, tangent);
			const EffectRandom random(impact.position);
			const auto drawSmoke = [&](XMVECTOR location, float radius, XMFLOAT4 color, unsigned seed) {
				context.DrawCloud(location, radius, random.Sample(seed) * XM_2PI + ageSeconds * 0.7f, seed,
					color, EffectClipMode::CenterVisible);
			};
			// 小さな灰色の煙を残し、火球にはしない
			for (unsigned i = 0; i < context.Settings().impact.smokeCount; ++i) {
				const auto offset = tangent * (random.Sample(i + 2) - 0.5f) * ageSeconds * 9 +
					bitangent * (random.Sample(i + 12) - 0.5f) * ageSeconds * 9;
				const auto location = origin + inherited * (0.45f * ageSeconds) +
					normal * (1 + ageSeconds * 5) + offset + XMVectorSet(0, ageSeconds * 2, 0, 0);
				const float alpha = std::min(ageSeconds * 24, 0.7f) *
					std::clamp((context.Settings().impact.lifetimeSeconds - ageSeconds) /
							context.Settings().impact.smokeFadeSeconds,
						0.0f, 1.0f);
				drawSmoke(location, 2.0f + ageSeconds * (9 + random.Sample(i + 22) * 7),
					{0.36f, 0.34f, 0.3f, alpha}, i + 7);
			}
		}

		void DrawImpactFlash(const ImpactEffect& impact, EffectDrawContext& context) {
			const float ageSeconds = impact.age;
			const auto origin = XMLoadFloat3(&impact.position);
			const auto inherited = XMLoadFloat3(&impact.velocity);
			if (ageSeconds < context.Settings().impact.flashSeconds) {
				const float flash = 1 - ageSeconds / context.Settings().impact.flashSeconds;
				context.DrawGlow(origin + inherited * ageSeconds, context.Settings().impact.flashOuterRadius,
					{1, 0.55f, 0.15f, flash * 0.45f}, EffectClipMode::CenterVisible);
				context.DrawGlow(origin + inherited * ageSeconds, context.Settings().impact.flashInnerRadius,
					{1, 0.95f, 0.75f, flash}, EffectClipMode::CenterVisible);
			}
		}

		void DrawImpactFragments(const ImpactEffect& impact, EffectDrawContext& context) {
			const float scale = context.GetScale();
			// 入射方向の逆を基準にして、金属片を機体の外側へ散らす
			const float ageSeconds = impact.age;
			const auto origin = XMLoadFloat3(&impact.position);
			const auto inherited = XMLoadFloat3(&impact.velocity);
			const auto normal = -XMVector3Normalize(XMLoadFloat3(&impact.direction));
			const auto reference =
				std::abs(XMVectorGetY(normal)) < 0.9f ? XMVectorSet(0, 1, 0, 0) : XMVectorSet(1, 0, 0, 0);
			const auto tangent = XMVector3Normalize(XMVector3Cross(normal, reference));
			const auto bitangent = XMVector3Cross(normal, tangent);
			const EffectRandom random(impact.position);
			for (int i = 0; i < context.Settings().impact.fragmentCount; ++i) {
				const float lifetimeSeconds = context.Settings().impact.fragmentMinLifetime +
					random.Sample(i + 32) * context.Settings().impact.fragmentLifetimeRange;
				if (ageSeconds >= lifetimeSeconds) continue;
				const float angle = random.Sample(i + 52) * XM_2PI;
				const float outward = 0.2f + random.Sample(i + 72) * 0.8f;
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
				const auto segment = context.ProjectSegment(positionAt(ageSeconds),
					positionAt(std::max(0.0f, ageSeconds - (0.012f + random.Sample(i + 112) * 0.024f))));
				if (!segment) continue;
				const float heat = 1 - ageSeconds / lifetimeSeconds;
				const float width = std::max(1.1f, scale * (1.8f + random.Sample(i + 132) * 2.2f));
				context.DrawStreak(*segment, width * 2, {1, 0.38f, 0.06f, heat * 0.3f}, width,
					{1, 0.4f + heat * 0.55f, 0.12f + heat * 0.55f, std::sqrt(heat)});
			}
		}

	} // namespace

	void CombatEffectPresenter::DrawImpact(const ImpactEffect& impact, EffectDrawContext& context) const {
		DrawImpactSmoke(impact, context);
		DrawImpactFlash(impact, context);
		DrawImpactFragments(impact, context);
	}
} // namespace Game
