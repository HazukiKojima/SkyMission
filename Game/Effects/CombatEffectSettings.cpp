#include "CombatEffectSettings.h"

namespace Game {
	void EffectSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);
		r.Number("/explosion/smokeMaxOpacity", explosion.smokeMaxOpacity, 0, 1);

		r.Number("/impact/smokeMaxOpacity", impact.smokeMaxOpacity, 0, 1);

		r.Color("/deck/smokeColor", deck.smokeColor);

		r.Color("/rocket/smokeColor", rocket.smokeColor);
		r.Color("/rocket/exhaustCoreColor", rocket.exhaustCoreColor);
		r.Color("/rocket/exhaustGlowColor", rocket.exhaustGlowColor);
		r.Color("/rocket/exhaustFlameColor", rocket.exhaustFlameColor);

		r.Color("/carrier/smokeColor", carrier.smokeColor);

		r.Color("/explosion/smokeDarkColor", explosion.smokeDarkColor);
		r.Color("/explosion/smokeLightColor", explosion.smokeLightColor);
		r.Color("/explosion/fireColdColor", explosion.fireColdColor);
		r.Color("/explosion/fireHotColor", explosion.fireHotColor);
		r.Color("/explosion/flashColor", explosion.flashColor);
		r.Color("/explosion/fragmentSmokeColor", explosion.fragmentSmokeColor);
		r.Color("/explosion/fragmentOuterColor", explosion.fragmentOuterColor);
		r.Color("/explosion/fragmentInnerColor", explosion.fragmentInnerColor);

		r.Color("/impact/smokeColor", impact.smokeColor);
		r.Color("/impact/flashOuterColor", impact.flashOuterColor);
		r.Color("/impact/flashInnerColor", impact.flashInnerColor);
		r.Color("/impact/fragmentOuterColor", impact.fragmentOuterColor);
		r.Color("/impact/fragmentColdColor", impact.fragmentColdColor);
		r.Color("/impact/fragmentHotColor", impact.fragmentHotColor);

		r.Color("/gun/smokeColor", gun.smokeColor);
		r.Color("/gun/tracerOuterColor", gun.tracerOuterColor);
		r.Color("/gun/tracerInnerColor", gun.tracerInnerColor);
		r.Color("/gun/flashOuterColor", gun.flashOuterColor);
		r.Color("/gun/flashInnerColor", gun.flashInnerColor);
		r.Color("/gun/flameColor", gun.flameColor);
		r.Color("/gun/flameOuterColor", gun.flameOuterColor);
		r.Color("/gun/flameInnerColor", gun.flameInnerColor);

		// 煙の動きと粒子数を読み込む
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

		// 発射演出の寸法と寿命を読み込む
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

		// 着弾演出の寿命と破片の動きを読み込む
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

		// 爆発演出の寿命と煙の広がりを読み込む
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

		// 内外の半径と幅が逆転していないか確認する
		r.Require(explosion.maxRadius >= explosion.minRadius, "effects", "inconsistent radii or widths");
		r.Require(
			impact.flashOuterRadius >= impact.flashInnerRadius, "effects", "inconsistent radii or widths");
		r.Require(gun.tracerOuterWidth >= gun.tracerInnerWidth, "effects", "inconsistent radii or widths");
		r.Require(gun.smokeEndRadius >= gun.smokeStartRadius, "effects", "inconsistent radii or widths");

		r.Number("/explosionFeedback/nearDistanceMeters", explosionFeedback.nearDistanceMeters, 0);
		r.Number("/explosionFeedback/farDistanceMeters", explosionFeedback.farDistanceMeters);
		r.Require(explosionFeedback.farDistanceMeters > explosionFeedback.nearDistanceMeters,
			"/explosionFeedback/farDistanceMeters", "must exceed nearDistanceMeters");
		r.Number("/explosionFeedback/initialFlashSeconds", explosionFeedback.initialFlashSeconds, 0.01f, 5);
		r.Number("/explosionFeedback/finalFlashSeconds", explosionFeedback.finalFlashSeconds, 0.01f, 5);
		r.Number("/explosionFeedback/initialStrength", explosionFeedback.initialStrength, 0, 1);
		r.Number("/explosionFeedback/cameraShakeMultiplier", explosionFeedback.cameraShakeMultiplier, 0, 10);
		r.Color("/explosionFeedback/flashColor", explosionFeedback.flashColor);

		r.Number("/aircraftCrash/horizontalSpeedRetention", aircraftCrash.horizontalSpeedRetention, 0, 1);
		r.Number("/aircraftCrash/initialDownwardSpeed", aircraftCrash.initialDownwardSpeed, 0, 1000);
		r.Number("/aircraftCrash/initialExplosionRadiusMultiplier", aircraftCrash.initialExplosionRadiusMultiplier, 0.01f, 10);
		r.Number("/aircraftCrash/initialFlashDurationMultiplier", aircraftCrash.initialFlashDurationMultiplier, 0.01f, 10);
		r.Number("/aircraftCrash/finalExplosionRadiusMultiplier", aircraftCrash.finalExplosionRadiusMultiplier, 0.01f, 10);
		constexpr std::array profileNames{"rollingDive", "bankedDive", "spin"};
		for (size_t i = 0; i < profileNames.size(); ++i) {
			const auto path = std::string("/aircraftCrash/") + profileNames[i];
			auto& profile = aircraftCrash.profiles[i];
			r.Number(path + "/durationSeconds", profile.durationSeconds, 0.1f, 30);
			r.Number(path + "/downwardAcceleration", profile.downwardAcceleration, 0, 1000);
			r.Number(path + "/drag", profile.drag, 0, 20);
			r.Number(path + "/turnRate", profile.turnRate, 0, 20);
			r.Number(path + "/bank", profile.bank, 0, DirectX::XM_PI);
			r.Number(path + "/rollRate", profile.rollRate, 0, 20);
			r.Number(path + "/spinRate", profile.spinRate, 0, 20);
		}

		r.Number("/aircraftTrail/contrailSpacingMeters", aircraftTrail.contrailSpacingMeters, 1, 100);
		r.Number("/aircraftTrail/smokeIntervalSeconds", aircraftTrail.smokeIntervalSeconds, 0.01f, 1);
		r.Number("/aircraftTrail/smokeStartRadius", aircraftTrail.smokeStartRadius, 0.01f, 100);
		r.Number("/aircraftTrail/smokeRadiusGrowth", aircraftTrail.smokeRadiusGrowth, 0, 100);
		r.Color("/aircraftTrail/smokeColor", aircraftTrail.smokeColor);
	}
} // namespace Game
