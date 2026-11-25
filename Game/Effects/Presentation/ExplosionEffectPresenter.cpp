#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;

	namespace {
		void DrawExplosionSmoke(const ExplosionEffect& effect, EffectDrawContext& context) {
			const float ageSeconds = effect.age;
			const float radiusScale = effect.radius;
			const auto origin = XMLoadFloat3(&effect.position);
			const auto drift = XMLoadFloat3(&effect.velocity) * (0.18f * (1 - std::exp(-ageSeconds * 2)));
			const bool backlit = effect.style == ExplosionStyle::BacklitSmoke;
			const float glow = backlit ? std::clamp(1.0f - ageSeconds / 0.24f, 0.0f, 1.0f) : 0.0f;
			const auto drawCloud = [&](XMVECTOR location, float radius, XMFLOAT4 color, unsigned seed) {
				const float rotation = seed * 1.71f + ageSeconds * (seed % 2 ? 0.12f : -0.09f);
				context.DrawCloud(location, radius, rotation, seed, color);
			};

			// 大きさと上昇速度の異なる煙塊を重ねて乱流を表現する
			for (int i = 0; i < context.Settings().explosion.smokeCount; ++i) {
				const float phase = i * EffectDrawSettings::goldenAngleRadians;
				const float variation = 0.55f + (i % 7) * 0.11f;
				const float spread =
					radiusScale * (0.2f + (1 - std::exp(-ageSeconds * 1.4f)) * 1.3f) * variation;
				const auto location = origin + drift +
					XMVectorSet(std::cos(phase) * spread,
						std::sin(phase) * spread * 0.6f +
							ageSeconds * radiusScale * (0.35f + (i % 4) * 0.12f),
						std::sin(phase * 2) * spread * 0.5f, 0);
				const float alpha = std::min((backlit ? 0.58f : 0.0f) + ageSeconds * 5,
					context.Settings().explosion.smokeMaxOpacity) *
					std::clamp((context.Settings().explosion.lifetimeSeconds - ageSeconds) /
							context.Settings().explosion.smokeFadeSeconds,
						0.0f, 1.0f);
				auto shade = BlendEffectColor(context.Settings().explosion.smokeDarkColor,
					context.Settings().explosion.smokeLightColor, float(i % 5) / (backlit ? 8.0f : 4.0f));
				if (backlit) {
					// 明るい煙と暗い煙を重ね、閃光が隙間から漏れるようにする
					const float illumination = glow * glow * (i % 3 == 0 ? 0.95f : 0.18f);
					shade = BlendEffectColor(shade, {1.0f, 0.30f, 0.035f, 1.0f}, illumination);
				}
				drawCloud(location, radiusScale * (0.38f + ageSeconds * 0.27f) * variation,
					EffectTint(shade, alpha), i + 1);
			}
		}

		void DrawExplosionFire(const ExplosionEffect& effect, EffectDrawContext& context) {
			const float ageSeconds = effect.age;
			const float radiusScale = effect.radius;
			const auto origin = XMLoadFloat3(&effect.position);
			const auto drift = XMLoadFloat3(&effect.velocity) * (0.18f * (1 - std::exp(-ageSeconds * 2)));
			const bool falling = effect.style == ExplosionStyle::FallingDebris;
			const auto fallDirection = XMVector3Normalize(XMLoadFloat3(&effect.velocity));
			const auto drawCloud = [&](XMVECTOR location, float radius, XMFLOAT4 color, unsigned seed) {
				const float rotation = seed * 1.71f + ageSeconds * (seed % 2 ? 0.12f : -0.09f);
				context.DrawCloud(location, radius, rotation, seed, color);
			};

			// 火炎の塊ごとに着火時刻と燃焼時間を変え、輪郭を崩す
			for (int i = 0; i < context.Settings().explosion.fireCount; ++i) {
				const float delay = i == 0 ? 0 : (i % 5) * 0.025f;
				const float particleAgeSeconds = ageSeconds - delay;
				const float lifetimeSeconds = 0.48f + (i % 5) * 0.095f;

				if (particleAgeSeconds < 0 || particleAgeSeconds >= lifetimeSeconds) continue;

				const float heat = 1 - particleAgeSeconds / lifetimeSeconds;
				const float phase = i * EffectDrawSettings::goldenAngleRadians;
				const float spread = radiusScale * (0.15f + particleAgeSeconds * 1.15f) * (i == 0 ? 0 : 0.9f);
				auto location = origin + drift +
					XMVectorSet(std::cos(phase) * spread,
						std::sin(phase) * spread * 0.8f + particleAgeSeconds * radiusScale * 0.55f,
						std::sin(phase * 2) * spread * 0.4f, 0);
				if (falling && i != 0) {
					const float distance = radiusScale * (0.35f + 2.6f * particleAgeSeconds) *
						(0.45f + 0.13f * (i % 7));
					location += fallDirection * distance;
					if (XMVectorGetY(location) < 0) continue;
				}
				const float radius =
					radiusScale * (0.28f + std::sqrt(particleAgeSeconds) * 0.5f) * (0.8f + (i % 3) * 0.22f);
				drawCloud(location, radius,
					EffectTint(BlendEffectColor(context.Settings().explosion.fireColdColor,
								   context.Settings().explosion.fireHotColor, heat),
						effect.style == ExplosionStyle::BacklitSmoke && i != 0
							? heat * std::clamp((ageSeconds - 0.045f) / 0.13f, 0.0f, 1.0f) : heat),
					i + 27);
			}
		}

		void DrawExplosionFlash(const ExplosionEffect& effect, EffectDrawContext& context) {
			const float ageSeconds = effect.age;
			const float radiusScale = effect.radius;
			const auto origin = XMLoadFloat3(&effect.position);

			// 白熱はごく短くし、均一な衝撃波の輪は描かない
			const float flashSeconds = context.Settings().explosion.flashSeconds * effect.flashDurationMultiplier;
			if (ageSeconds < flashSeconds) {
				const float flash = 1 - ageSeconds / flashSeconds;
				if (effect.flashDurationMultiplier > 1.0f) {
					context.DrawGlow(origin, radiusScale * (1.25f + ageSeconds * 3.0f),
						{1.0f, 0.36f, 0.04f, 0.8f * flash});
				}
				context.DrawGlow(origin, radiusScale * (0.65f + ageSeconds * 2),
					EffectTint(context.Settings().explosion.flashColor, flash));
			}
		}

		void DrawExplosionFragments(
			const ExplosionEffect& effect, EffectDrawContext& context, float fragmentScale) {
			const float ageSeconds = effect.age;
			const float radiusScale = effect.radius;
			const auto origin = XMLoadFloat3(&effect.position);
			const bool sparkRain = effect.style == ExplosionStyle::FallingDebris;
			const auto drawCloud = [&](XMVECTOR location, float radius, XMFLOAT4 color, unsigned seed) {
				const float rotation = seed * 1.71f + ageSeconds * (seed % 2 ? 0.12f : -0.09f);
				context.DrawCloud(location, radius, rotation, seed, color);
			};

			// 重力で落下する破片と燃える軌跡
			const int fragmentCount = context.Settings().explosion.fragmentCount;
			for (int i = 0; i < fragmentCount + (sparkRain ? 24 : 0); ++i) {
				if (!context.HasVertexBudget(30)) break;
				const bool ember = i >= fragmentCount;
				const float lifetimeSeconds = (sparkRain ? 1.34f : 1.1f) + (i % 7) * 0.22f;

				if (ageSeconds >= lifetimeSeconds) continue;

				const float phase = i * EffectDrawSettings::goldenAngleRadians;
				const float y = std::sin(i * 17.31f) * (ember ? 0.38f : 0.85f) + (ember ? 0.44f : 0.0f);
				const float radial = std::sqrt(1 - y * y);
				const float speed = radiusScale * (0.8f + (i % 9) * 0.21f) * (ember ? 0.68f : 1.0f);
				const auto velocity = XMVectorSet(
					std::cos(phase) * radial * speed, y * speed, std::sin(phase) * radial * speed, 0);
				const auto positionAt = [&](float particleAgeSeconds) {
					const auto inheritedDisplacement = XMLoadFloat3(&effect.velocity) *
						(particleAgeSeconds * context.Settings().explosion.fragmentVelocityInheritance *
							(ember ? 1.7f : 1.0f));
					const auto fragmentDisplacement = velocity * particleAgeSeconds;
					const auto gravityDisplacement = XMVectorSet(0,
						-context.Settings().explosion.fragmentGravity * particleAgeSeconds *
							particleAgeSeconds * (ember ? 2.1f : 1.0f),
						0, 0);
					return origin + inheritedDisplacement + fragmentDisplacement + gravityDisplacement;
				};

				const auto head = positionAt(ageSeconds);
				auto tail = positionAt(std::max(0.0f, ageSeconds - context.Settings().explosion.fragmentTrailSeconds));
				if (sparkRain && XMVectorGetY(head) < 0) continue;
				if (sparkRain) tail = XMVectorSetY(tail, std::max(0.0f, XMVectorGetY(tail)));
				const auto segment = context.ProjectSegment(head, tail);
				if (!segment) continue;

				const float fade = 1 - ageSeconds / lifetimeSeconds;

				if (!ember && i % 3 == 0) {
					for (int puff = 1; puff <= 3; ++puff) {
						const float particleAgeSeconds = std::max(0.0f, ageSeconds - puff * 0.07f);
						drawCloud(positionAt(particleAgeSeconds), radiusScale * (0.035f + puff * 0.014f),
							EffectTint(context.Settings().explosion.fragmentSmokeColor, fade), i + puff + 3);
					}
				}

				if (ember) {
					const float heat = fade * (0.84f + 0.16f * std::sin(ageSeconds * 38.0f + phase));
					context.DrawStreak(*segment, std::max(1.3f, fragmentScale * 2.6f),
						{1.0f, 0.21f + heat * 0.25f, 0.035f, heat},
						std::max(0.8f, fragmentScale), {1.0f, 0.56f + heat * 0.35f, 0.23f + heat * 0.5f, fade});
					continue;
				}
				context.DrawStreak(*segment, std::max(1.0f, fragmentScale * 3),
					EffectTint(context.Settings().explosion.fragmentOuterColor, fade),
					std::max(0.7f, fragmentScale * 1.2f),
					EffectTint(context.Settings().explosion.fragmentInnerColor, fade));
			}
		}

	} // namespace

	void CombatEffectPresenter::DrawExplosion(
		const ExplosionEffect& effect, EffectDrawContext& context, float fragmentScale) const {
		if (effect.style == ExplosionStyle::BacklitSmoke) DrawExplosionFlash(effect, context);
		DrawExplosionSmoke(effect, context);
		DrawExplosionFire(effect, context);
		if (effect.style != ExplosionStyle::BacklitSmoke) DrawExplosionFlash(effect, context);
		DrawExplosionFragments(effect, context, fragmentScale);
	}

	void CombatEffectPresenter::DrawExplosionScreenFlash(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
		const DirectX::XMFLOAT3& playerPosition, EffectDrawContext& context, Engine::HudDrawList& list) const {
		const auto& settings = context.Settings().explosionFeedback;
		const float flash = EvaluateExplosionFeedback(snapshot, playerPosition, settings).flash;
		if (flash > 0 && context.HasVertexBudget(6)) {
			list.FilledRect({0, 0}, {static_cast<float>(view.width), static_cast<float>(view.height)},
				EffectTint(settings.flashColor, flash));
		}
	}
} // namespace Game
