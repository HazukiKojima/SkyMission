#pragma once
#include <cstddef>

namespace Game {
	struct GunEffectSettings {
		static constexpr float flashLifetimeSeconds = 0.065f;
		static constexpr float flashRadius = 2.5f;
		static constexpr float flameLength = 8.0f;
		static constexpr size_t maxFlashCount = 4;
		static constexpr float tracerSeconds = 0.018f;
		static constexpr float tracerMaxLength = 120.0f;
		static constexpr float tracerOuterWidth = 4.0f;
		static constexpr float tracerInnerWidth = 1.5f;
		static constexpr size_t maxTracerCount = 256;
		static constexpr float smokeLifetimeSeconds = 0.35f;
		static constexpr float smokeStartRadius = 0.8f;
		static constexpr float smokeEndRadius = 2.8f;
		static constexpr float smokeOpacity = 0.16f;
		static constexpr float smokeVelocityInheritance = 0.25f;
		static constexpr float smokeDrag = 2.0f;
		static constexpr float smokeRiseSpeed = 3.0f;
		static constexpr size_t maxSmokeCount = 64;
		static constexpr size_t reservedVertices = 4000;
		static constexpr size_t flashVertexBudget = 222;
		static constexpr size_t tracerVertexBudget = 12;
	};
	// 時間は秒、距離はワールド単位、太さは1920x1080基準で指定する
	struct ImpactEffectSettings {
		static constexpr float lifetimeSeconds = 0.45f;
		static constexpr size_t maxCount = 64;
		static constexpr unsigned smokeCount = 8;
		static constexpr int fragmentCount = 28;
		static constexpr float flashSeconds = 0.1f;
		static constexpr float flashOuterRadius = 8.0f;
		static constexpr float flashInnerRadius = 4.8f;
		static constexpr float smokeFadeSeconds = 0.22f;
		static constexpr float fragmentMinLifetime = 0.16f;
		static constexpr float fragmentLifetimeRange = 0.28f;
		static constexpr float fragmentMinSpeed = 24.0f;
		static constexpr float fragmentSpeedRange = 60.0f;
		static constexpr float fragmentDrag = 3.0f;
		static constexpr float fragmentGravity = 9.8f;
		static constexpr float fragmentVelocityInheritance = 0.7f;
	};

	struct ExplosionEffectSettings {
		static constexpr float lifetimeSeconds = 3.5f;
		static constexpr size_t maxCount = 4;
		static constexpr int smokeCount = 22;
		static constexpr int fireCount = 17;
		static constexpr int fragmentCount = 24;
		static constexpr float flashSeconds = 0.09f;
		static constexpr float smokeFadeSeconds = 1.3f;
		static constexpr float minRadius = 18.0f;
		static constexpr float maxRadius = 65.0f;
		static constexpr float boundsRadiusScale = 0.65f;
		static constexpr float fragmentGravity = 18.0f;
		static constexpr float fragmentTrailSeconds = 0.018f;
		static constexpr float fragmentVelocityInheritance = 0.2f;
	};

	struct RocketEffectSettings {
		static constexpr float smokeLifetimeSeconds = 0.8f;
		static constexpr float emissionIntervalSeconds = 0.025f;
		static constexpr size_t maxSmokeCount = 1024;
		static constexpr size_t maxExhaustCount = 16;
	};

	struct CombatEffectSettings {
		static constexpr float maximumFrameSeconds = 0.25f;
	};

	struct EffectDrawSettings {
		// 視認性を保つため、エフェクトより後に描く照準と文字の頂点を確保する
		static constexpr size_t hudReservedVertices = 6000;
		static constexpr size_t explosionVertexBudget = 4800;
		static constexpr size_t impactVertexBudget = 600;
		static constexpr float referenceWidth = 1920.0f;
		static constexpr float referenceHeight = 1080.0f;
		static constexpr float goldenAngleRadians = 2.399963f;
	};
} // namespace Game
