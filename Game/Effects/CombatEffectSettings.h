#pragma once
#include <cstddef>
#include "Game/Settings/SettingsReader.h"

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

	struct EffectSettings {
		struct GunEffectTuning : GunEffectSettings {
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
		struct CarrierEffectTuning : CarrierEffectSettings {
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
			float smokeGrowthPerSecond = 7.0f;
			float smokeStartRadius = 1.4f;
			float smokeRiseSpeed = 3.0f;
			float smokeOpacity = 0.32f;
			float smokeLifetimeSeconds = RocketEffectSettings::smokeLifetimeSeconds;
		} rocket;
		struct DeckSmokeTuning {
			float startRadius = 25, growthPerSecond = 32, driftSpeed = 12, riseSpeed = 65;
		} deck;
		void Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
			SettingsReader r(reader, path);
			r.Number("/deck/riseSpeed", deck.riseSpeed, 0, 1000000);
			r.Number("/deck/driftSpeed", deck.driftSpeed, 0, 1000000);
			r.Number("/deck/growthPerSecond", deck.growthPerSecond, 0, 1000000);
			r.Number("/deck/startRadius", deck.startRadius, 0, 1000000);
			r.Number("/rocket/smokeGrowthPerSecond", rocket.smokeGrowthPerSecond, 0, 1000000);
			r.Number("/rocket/smokeStartRadius", rocket.smokeStartRadius, 0, 1000000);
			r.Number("/rocket/smokeRiseSpeed", rocket.smokeRiseSpeed, 0, 1000000);
			r.Number("/rocket/smokeOpacity", rocket.smokeOpacity, 0, 1);
			r.Number("/carrier/smokeDriftSpeed", carrier.smokeDriftSpeed, 0, 1000000);
			r.Number("/carrier/smokeOpacity", carrier.smokeOpacity, 0, 1);
			r.Integer("/explosion/fragmentCount", explosion.fragmentCount, 24);
			r.Integer("/explosion/fireCount", explosion.fireCount, 17);
			r.Integer("/explosion/smokeCount", explosion.smokeCount, 22);
			r.Integer("/impact/fragmentCount", impact.fragmentCount, 28);
			r.Integer("/impact/smokeCount", impact.smokeCount, 8);
			r.Number("/gun/flashLifetimeSeconds", gun.flashLifetimeSeconds, 0.000001f, 1000000);
			r.Number("/gun/flashRadius", gun.flashRadius, 0.000001f, 1000000);
			r.Number("/gun/flameLength", gun.flameLength, 0.000001f, 1000000);
			r.Number("/gun/tracerSeconds", gun.tracerSeconds, 0.000001f, 1000000);
			r.Number("/gun/tracerMaxLength", gun.tracerMaxLength, 0.000001f, 1000000);
			r.Number("/gun/tracerOuterWidth", gun.tracerOuterWidth, 0.000001f, 1000000);
			r.Number("/gun/tracerInnerWidth", gun.tracerInnerWidth, 0.000001f, 1000000);
			r.Number("/gun/smokeLifetimeSeconds", gun.smokeLifetimeSeconds, 0.000001f, 1000000);
			r.Number("/gun/smokeStartRadius", gun.smokeStartRadius, 0.000001f, 1000000);
			r.Number("/gun/smokeEndRadius", gun.smokeEndRadius, 0.000001f, 1000000);
			r.Number("/gun/smokeOpacity", gun.smokeOpacity, 0.000001f, 1);
			r.Number("/gun/smokeVelocityInheritance", gun.smokeVelocityInheritance, 0, 1);
			r.Number("/gun/smokeDrag", gun.smokeDrag, 0.000001f, 1000000);
			r.Number("/gun/smokeRiseSpeed", gun.smokeRiseSpeed, 0, 1000000);
			r.Number("/impact/lifetimeSeconds", impact.lifetimeSeconds, 0.000001f, 1000000);
			r.Number("/impact/flashSeconds", impact.flashSeconds, 0.000001f, 1000000);
			r.Number("/impact/flashOuterRadius", impact.flashOuterRadius, 0.000001f, 1000000);
			r.Number("/impact/flashInnerRadius", impact.flashInnerRadius, 0.000001f, 1000000);
			r.Number("/impact/smokeFadeSeconds", impact.smokeFadeSeconds, 0.000001f, 1000000);
			r.Number("/impact/fragmentMinLifetime", impact.fragmentMinLifetime, 0.000001f, 1000000);
			r.Number("/impact/fragmentLifetimeRange", impact.fragmentLifetimeRange, 0.000001f, 1000000);
			r.Number("/impact/fragmentMinSpeed", impact.fragmentMinSpeed, 0.000001f, 1000000);
			r.Number("/impact/fragmentSpeedRange", impact.fragmentSpeedRange, 0.000001f, 1000000);
			r.Number("/impact/fragmentDrag", impact.fragmentDrag, 0.000001f, 1000000);
			r.Number("/impact/fragmentGravity", impact.fragmentGravity, 0.000001f, 1000000);
			r.Number("/impact/fragmentVelocityInheritance", impact.fragmentVelocityInheritance, 0, 1);
			r.Number("/explosion/lifetimeSeconds", explosion.lifetimeSeconds, 0.000001f, 1000000);
			r.Number("/explosion/flashSeconds", explosion.flashSeconds, 0.000001f, 1000000);
			r.Number("/explosion/smokeFadeSeconds", explosion.smokeFadeSeconds, 0.000001f, 1000000);
			r.Number("/explosion/minRadius", explosion.minRadius, 0.000001f, 1000000);
			r.Number("/explosion/maxRadius", explosion.maxRadius, 0.000001f, 1000000);
			r.Number("/explosion/boundsRadiusScale", explosion.boundsRadiusScale, 0.000001f, 1000000);
			r.Number("/explosion/fragmentGravity", explosion.fragmentGravity, 0.000001f, 1000000);
			r.Number("/explosion/fragmentTrailSeconds", explosion.fragmentTrailSeconds, 0.000001f, 1000000);
			r.Number("/explosion/fragmentVelocityInheritance", explosion.fragmentVelocityInheritance, 0, 1);
			r.Number("/carrier/explosionRadius", carrier.explosionRadius, 0.000001f, 1000000);
			r.Number("/carrier/smokeLifetimeSeconds", carrier.smokeLifetimeSeconds, 0.000001f, 1000000);
			r.Number("/carrier/smokeIntervalSeconds", carrier.smokeIntervalSeconds, 0.000001f, 1000000);
			r.Number("/carrier/smokeRiseSpeed", carrier.smokeRiseSpeed, 0, 1000000);
			r.Number("/carrier/smokeStartRadius", carrier.smokeStartRadius, 0.000001f, 1000000);
			r.Number("/carrier/smokeGrowthPerSecond", carrier.smokeGrowthPerSecond, 0.000001f, 1000000);
			r.Number("/rocket/smokeLifetimeSeconds", rocket.smokeLifetimeSeconds, 0.000001f, 1000000);
			r.Require(explosion.maxRadius >= explosion.minRadius, "effects", "inconsistent radii or widths");
			r.Require(impact.flashOuterRadius >= impact.flashInnerRadius, "effects",
				"inconsistent radii or widths");
			r.Require(
				gun.tracerOuterWidth >= gun.tracerInnerWidth, "effects", "inconsistent radii or widths");
			r.Require(gun.smokeEndRadius >= gun.smokeStartRadius, "effects", "inconsistent radii or widths");
		}
	};
} // namespace Game
