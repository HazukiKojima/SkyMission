#pragma once
#include <cstddef>
#include <cmath>
#include "Game/Settings/SettingsReader.h"

namespace Game {
	inline DirectX::XMFLOAT4 EffectTint(DirectX::XMFLOAT4 color, float alpha) {
		color.w *= alpha;
		return color;
	}

	inline DirectX::XMFLOAT4 BlendEffectColor(
		const DirectX::XMFLOAT4& cold, const DirectX::XMFLOAT4& hot, float factor) {
		return {std::lerp(cold.x, hot.x, factor), std::lerp(cold.y, hot.y, factor),
			std::lerp(cold.z, hot.z, factor), std::lerp(cold.w, hot.w, factor)};
	}

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

	struct CarrierEffectSettings {
		static constexpr float explosionRadius = 220;
		static constexpr float smokeLifetimeSeconds = 10;
		static constexpr float smokeIntervalSeconds = 0.15f;
		static constexpr float smokeStartRadius = 70, smokeGrowthPerSecond = 22;
		static constexpr float smokeRiseSpeed = 65;
		static constexpr size_t maxSmokeCount = 96;
	};

	struct RocketEffectSettings {
		static constexpr float smokeLifetimeSeconds = 0.8f;
		static constexpr float emissionIntervalSeconds = 0.025f;
		static constexpr size_t maxSmokeCount = 1024;
		static constexpr size_t maxExhaustCount = 20;
	};

	struct DeckSmokeSettings {
		static constexpr float lifetimeSeconds = CarrierEffectSettings::smokeLifetimeSeconds;
		static constexpr size_t maxCount = CarrierEffectSettings::maxSmokeCount;
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

	struct EffectSettings {
		struct GunEffectTuning : GunEffectSettings {
			DirectX::XMFLOAT4 smokeColor{0.62f, 0.61f, 0.58f, 1.0f};
			DirectX::XMFLOAT4 tracerOuterColor{1.0f, 0.32f, 0.035f, 0.75f};
			DirectX::XMFLOAT4 tracerInnerColor{1.0f, 0.95f, 0.65f, 0.95f};
			DirectX::XMFLOAT4 flashOuterColor{1.0f, 0.35f, 0.035f, 0.8f};
			DirectX::XMFLOAT4 flashInnerColor{1.0f, 0.98f, 0.7f, 1.0f};
			DirectX::XMFLOAT4 flameColor{1.0f, 0.55f, 0.08f, 0.75f};
			DirectX::XMFLOAT4 flameOuterColor{1.0f, 0.4f, 0.04f, 0.65f};
			DirectX::XMFLOAT4 flameInnerColor{1.0f, 0.95f, 0.65f, 1.0f};

			float flashLifetimeSeconds = GunEffectSettings::flashLifetimeSeconds;
			float flashRadius = GunEffectSettings::flashRadius;
			float flameLength = GunEffectSettings::flameLength;
			float tracerSeconds = GunEffectSettings::tracerSeconds;
			float tracerMaxLength = GunEffectSettings::tracerMaxLength;
			float tracerOuterWidth = GunEffectSettings::tracerOuterWidth;
			float tracerInnerWidth = GunEffectSettings::tracerInnerWidth;
			float smokeLifetimeSeconds = GunEffectSettings::smokeLifetimeSeconds;
			float smokeStartRadius = GunEffectSettings::smokeStartRadius;
			float smokeEndRadius = GunEffectSettings::smokeEndRadius;
			float smokeOpacity = GunEffectSettings::smokeOpacity;
			float smokeVelocityInheritance = GunEffectSettings::smokeVelocityInheritance;
			float smokeDrag = GunEffectSettings::smokeDrag;
			float smokeRiseSpeed = GunEffectSettings::smokeRiseSpeed;
		} gun;
		struct ImpactEffectTuning : ImpactEffectSettings {
			float smokeMaxOpacity = 0.7f;

			DirectX::XMFLOAT4 smokeColor{0.36f, 0.34f, 0.3f, 1.0f};
			DirectX::XMFLOAT4 flashOuterColor{1.0f, 0.55f, 0.15f, 0.45f};
			DirectX::XMFLOAT4 flashInnerColor{1.0f, 0.95f, 0.75f, 1.0f};
			DirectX::XMFLOAT4 fragmentOuterColor{1.0f, 0.38f, 0.06f, 0.3f};
			DirectX::XMFLOAT4 fragmentColdColor{1.0f, 0.4f, 0.12f, 1.0f};
			DirectX::XMFLOAT4 fragmentHotColor{1.0f, 0.95f, 0.67f, 1.0f};

			unsigned smokeCount = 8;
			int fragmentCount = 28;
			float lifetimeSeconds = ImpactEffectSettings::lifetimeSeconds;
			float flashSeconds = ImpactEffectSettings::flashSeconds;
			float flashOuterRadius = ImpactEffectSettings::flashOuterRadius;
			float flashInnerRadius = ImpactEffectSettings::flashInnerRadius;
			float smokeFadeSeconds = ImpactEffectSettings::smokeFadeSeconds;
			float fragmentMinLifetime = ImpactEffectSettings::fragmentMinLifetime;
			float fragmentLifetimeRange = ImpactEffectSettings::fragmentLifetimeRange;
			float fragmentMinSpeed = ImpactEffectSettings::fragmentMinSpeed;
			float fragmentSpeedRange = ImpactEffectSettings::fragmentSpeedRange;
			float fragmentDrag = ImpactEffectSettings::fragmentDrag;
			float fragmentGravity = ImpactEffectSettings::fragmentGravity;
			float fragmentVelocityInheritance = ImpactEffectSettings::fragmentVelocityInheritance;
		} impact;
		struct ExplosionEffectTuning : ExplosionEffectSettings {
			float smokeMaxOpacity = 0.85f;

			DirectX::XMFLOAT4 smokeDarkColor{0.09f, 0.0855f, 0.0792f, 1.0f};
			DirectX::XMFLOAT4 smokeLightColor{0.19f, 0.1805f, 0.1672f, 1.0f};
			DirectX::XMFLOAT4 fireColdColor{1.0f, 0.27f, 0.025f, 0.95f};
			DirectX::XMFLOAT4 fireHotColor{1.0f, 0.62f, 0.115f, 0.95f};
			DirectX::XMFLOAT4 flashColor{1.0f, 0.85f, 0.55f, 0.8f};
			DirectX::XMFLOAT4 fragmentSmokeColor{0.12f, 0.11f, 0.1f, 0.5f};
			DirectX::XMFLOAT4 fragmentOuterColor{1.0f, 0.25f, 0.015f, 0.65f};
			DirectX::XMFLOAT4 fragmentInnerColor{1.0f, 0.85f, 0.4f, 1.0f};

			int smokeCount = 22;
			int fireCount = 17;
			int fragmentCount = 24;
			float lifetimeSeconds = ExplosionEffectSettings::lifetimeSeconds;
			float flashSeconds = ExplosionEffectSettings::flashSeconds;
			float smokeFadeSeconds = ExplosionEffectSettings::smokeFadeSeconds;
			float minRadius = ExplosionEffectSettings::minRadius;
			float maxRadius = ExplosionEffectSettings::maxRadius;
			float boundsRadiusScale = ExplosionEffectSettings::boundsRadiusScale;
			float fragmentGravity = ExplosionEffectSettings::fragmentGravity;
			float fragmentTrailSeconds = ExplosionEffectSettings::fragmentTrailSeconds;
			float fragmentVelocityInheritance = ExplosionEffectSettings::fragmentVelocityInheritance;
		} explosion;
		struct ExplosionFeedbackTuning {
			float nearDistanceMeters = 150.0f;
			float farDistanceMeters = 1200.0f;
			float initialFlashSeconds = 0.16f;
			float finalFlashSeconds = 0.22f;
			float initialStrength = 0.72f;
			float cameraShakeMultiplier = 4.0f;
			DirectX::XMFLOAT4 flashColor{1.0f, 0.77f, 0.46f, 0.28f};
		} explosionFeedback;
		struct AircraftCrashProfile {
			float durationSeconds;
			float downwardAcceleration;
			float drag;
			float turnRate;
			float bank;
			float rollRate;
			float spinRate;
		};
		struct AircraftCrashTuning {
			float horizontalSpeedRetention = 0.92f;
			float initialDownwardSpeed = 62.0f;
			float initialExplosionRadiusMultiplier = 2.5f;
			float initialFlashDurationMultiplier = 2.2f;
			float finalExplosionRadiusMultiplier = 2.8f;
			std::array<float, 2> engineBurstProgress{0.22f, 0.56f};
			static constexpr float fuelFireStartProgress = 0.20f;
			float fuelFireFlashProgress = 0.82f;
			std::array<AircraftCrashProfile, 3> profiles{{
				{2.145f, 164.0f, 0.53f, 0.12f, 0.65f, 5.4f, 0.0f},
				{2.21f, 136.0f, 0.65f, 0.85f, 1.65f, 0.55f, 0.0f},
				{2.08f, 149.0f, 1.25f, 0.04f, 0.48f, 0.0f, 5.8f},
			}};
		} aircraftCrash;
		struct AircraftTrailTuning {
			float contrailSpacingMeters = 4.0f;
			float smokeIntervalSeconds = 0.028f;
			float smokeStartRadius = 3.5f;
			float smokeRadiusGrowth = 14.0f;
			DirectX::XMFLOAT4 smokeColor{0.075f, 0.066f, 0.060f, 0.88f};
		} aircraftTrail;
		struct CarrierEffectTuning : CarrierEffectSettings {
			DirectX::XMFLOAT4 smokeColor{0.18f, 0.17f, 0.16f, 1.0f};

			float smokeDriftSpeed = 12.0f;
			float smokeOpacity = 0.6f;
			float explosionRadius = CarrierEffectSettings::explosionRadius;
			float smokeLifetimeSeconds = CarrierEffectSettings::smokeLifetimeSeconds;
			float smokeIntervalSeconds = CarrierEffectSettings::smokeIntervalSeconds;
			float smokeRiseSpeed = CarrierEffectSettings::smokeRiseSpeed;
			float smokeStartRadius = CarrierEffectSettings::smokeStartRadius;
			float smokeGrowthPerSecond = CarrierEffectSettings::smokeGrowthPerSecond;
		} carrier;
		struct RocketEffectTuning : RocketEffectSettings {
			DirectX::XMFLOAT4 smokeColor{0.65f, 0.65f, 0.62f, 1.0f};
			DirectX::XMFLOAT4 exhaustCoreColor{1.0f, 0.75f, 0.25f, 0.85f};
			DirectX::XMFLOAT4 exhaustGlowColor{1.0f, 0.25f, 0.02f, 0.5f};
			DirectX::XMFLOAT4 exhaustFlameColor{1.0f, 0.45f, 0.06f, 0.8f};

			float smokeGrowthPerSecond = 7.0f;
			float smokeStartRadius = 1.4f;
			float smokeRiseSpeed = 3.0f;
			float smokeOpacity = 0.32f;
			float smokeLifetimeSeconds = RocketEffectSettings::smokeLifetimeSeconds;
		} rocket;
		struct DeckSmokeTuning {
			DirectX::XMFLOAT4 smokeColor{0.18f, 0.17f, 0.16f, 1.0f};

			float startRadius = 25, growthPerSecond = 32, driftSpeed = 12, riseSpeed = 65;
		} deck;
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path);
	};
} // namespace Game
