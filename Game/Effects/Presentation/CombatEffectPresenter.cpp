#include "CombatEffectPresenter.h"
#include "EffectDrawContext.h"
#include <algorithm>

namespace Game {
	void CombatEffectPresenter::Build(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
		Engine::HudDrawList& list) const {
		if (view.width == 0 || view.height == 0) return;

		EffectDrawContext context(view, list, m_settings);

		// 閃光と曳光弾の領域を先に確保し、煙で発射の手応えを押し出さない
		const auto gunBudget = std::min(snapshot.gunFlashes.size(), GunEffectSettings::maxFlashCount) *
				GunEffectSettings::flashVertexBudget +
			std::min(snapshot.gunTracers.size(), GunEffectSettings::maxTracerCount) *
				GunEffectSettings::tracerVertexBudget;
		context.SetAdditionalReservedVertices(gunBudget);
		DrawGunSmoke(snapshot, context);

		// 煙と火球を先に描き、命中の閃光と金属片を手前へ重ねる
		for (const auto& explosion : snapshot.explosions) {
			if (explosion.age < 0 || explosion.age >= context.Settings().explosion.lifetimeSeconds) continue;
			if (!context.HasVertexBudget(EffectDrawSettings::explosionVertexBudget)) continue;

			DrawExplosion(explosion, context, view.height / EffectDrawSettings::referenceHeight);
		}

		DrawDeckSmoke(snapshot, context);
		DrawRocketEffects(snapshot, context);
		context.SetAdditionalReservedVertices(0);
		DrawGunFire(snapshot, context);

		for (const auto& impact : snapshot.impacts) {
			if (!context.HasVertexBudget(EffectDrawSettings::impactVertexBudget)) break;
			if (impact.age < 0 || impact.age >= context.Settings().impact.lifetimeSeconds) continue;

			DrawImpact(impact, context);
		}

		for (const auto& vapor : snapshot.wingtipVapor) {
			if (!context.HasVertexBudget(12)) break;
			const auto segment = context.ProjectClippedSegment(vapor.head, vapor.tail);
			if (!segment) continue;

			const float remaining = 1.0f - vapor.age / WingtipVaporSegment::lifetimeSeconds;
			const float depth = vapor.head.x * view.view._13 + vapor.head.y * view.view._23 +
				vapor.head.z * view.view._33 + view.view._43;
			const float diameter = 0.35f + vapor.age * 1.2f;
			const float width = diameter * view.projection._22 * (view.height * 0.5f) /
				std::max(depth, 1.0f);
			const float opacity = vapor.strength * remaining * remaining * std::min(width, 1.0f);
			context.DrawStreak(*segment, width, {0.85f, 0.92f, 1.0f, opacity * 0.18f},
				width * 0.5f, {1.0f, 1.0f, 1.0f, opacity * 0.42f});
		}

		const float minimumContrailRadius = std::max(0.85f, 1.15f * context.GetScale());
		const auto viewMatrix = DirectX::XMLoadFloat4x4(&view.view);
		for (const auto& puff : snapshot.contrails) {
			if (!context.HasVertexBudget(6)) break;

			// 翼端を離れた粒が少し後方で見え始めるようにする
			constexpr float visibleAfterSeconds = 0.12f;
			constexpr float formationSeconds = 0.20f;
			if (puff.age <= visibleAfterSeconds) continue;
			const float formation = std::clamp((puff.age - visibleAfterSeconds) / formationSeconds, 0.0f, 1.0f);
			const float formationOpacity = formation * formation * (3.0f - 2.0f * formation);

			using namespace DirectX;
			const EffectRandom random(puff.position);
			const float progress = puff.age / ContrailPuff::lifetimeSeconds;
			const float diffusion = std::sqrt(progress);
			const float variation = random.Sample(0);
			const float radius = (1.6f + 1.2f * diffusion) * (0.85f + 0.3f * variation);
			const float patch = 0.5f + 0.5f * std::sin(
				puff.position.x * 0.021f + puff.position.y * 0.031f + puff.position.z * 0.017f);
			// 隣り合う粒はまとまって薄れ、粒ごとの差で雲の端がほどける
			const float fadeStart = 0.14f + 0.26f * patch + 0.09f * random.Sample(6);
			const float fadeEnd = 0.83f + 0.17f * random.Sample(7);
			const float fade = std::clamp((progress - fadeStart) / (fadeEnd - fadeStart), 0.0f, 1.0f);
			const float remaining = 1.0f - fade * fade * (3.0f - 2.0f * fade);
			const auto position = XMLoadFloat3(&puff.position);
			const auto sightline = XMVector3Normalize(XMVector3TransformCoord(position, viewMatrix));
			const auto tangent = XMVector3TransformNormal(XMLoadFloat3(&puff.direction), viewMatrix);
			const float alignment = std::abs(XMVectorGetX(XMVector3Dot(sightline, tangent)));
			const float headOn = std::clamp((alignment - 0.92f) / 0.08f, 0.0f, 1.0f);
			const float overlap = headOn * headOn * (3.0f - 2.0f * headOn);
			// 前後から見ると重なる粒を細くし、遠距離での最小表示幅も同じ比率で縮める
			const float widthScale = std::lerp(1.0f, 0.35f, overlap);
			const float opacity = formationOpacity * remaining * (1.0f - progress) * (0.34f + 0.12f * variation) *
				std::lerp(1.0f, 0.16f, overlap);

			// 雲は機体の航跡に残り、風で流されながら拡散する
			const auto drift = XMVectorSet(6.12f, -1.17f, 2.7f, 0) * progress;
			const auto turbulence = XMVectorSet(random.Sample(1) - 0.5f, random.Sample(2) - 0.5f,
				random.Sample(3) - 0.5f, 0) * (radius * progress * 0.7f);
			context.DrawCloud(position + drift + turbulence, radius * widthScale,
				random.Sample(4) * XM_2PI, static_cast<unsigned>(random.Sample(5) * 64.0f),
				{0.80f, 0.85f, 0.92f, opacity}, EffectClipMode::BoundsVisible,
				minimumContrailRadius * widthScale);
		}
	}
} // namespace Game
