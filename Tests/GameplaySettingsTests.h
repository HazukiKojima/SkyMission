#pragma once
#include "Game/Settings/GameplaySettings.h"
#include "Game/Settings/UiTextSettings.h"
#include "Game/UI/RadarMapPresenter.h"

void TestGameplaySettings(const fs::path& repository, const fs::path& fixture) {
	const JsonAssetReader reader;
	const auto file = fixture / L"gameplay-settings.json";
	const auto write = [&](const std::string& text) {
		std::ofstream(file) << text;
	};
	const auto rejects = [&](const std::string& json, auto load) {
		write(json);
		bool rejected = false;
		try {
			load();
		} catch (const std::exception& error) {
			rejected = std::string(error.what()).find("gameplay-settings.json") != std::string::npos;
		}
		if (!rejected) throw std::runtime_error("Invalid tuning accepted or missing source context: " + json);
	};
	CameraSettings camera;
	camera.Load(reader, fixture / L"missing-camera.json");
	Require(camera.followDistance == 240, "Missing tuning lost camera defaults");
	write(R"({"followDistance":300,"maxFollowDistance":450,"fovDegrees":75})");
	camera.Load(reader, file);
	Require(camera.followDistance == 300 && camera.mouseSensitivity == 0.0025f,
		"Partial camera tuning lost defaults");
	rejects(R"({"followDistance":500,"maxFollowDistance":300})", [&] {
		CameraSettings{}.Load(reader, file);
	});
	rejects(R"({"mouseSensitivity":"fast"})", [&] {
		CameraSettings{}.Load(reader, file);
	});
	rejects(R"({"fovDegrees":180})", [&] {
		CameraSettings{}.Load(reader, file);
	});
	rejects(R"({"ammoCount":2.5})", [&] {
		MissileSettings{}.Load(reader, file);
	});
	rejects(R"({"ammoCount":17})", [&] {
		MissileSettings{}.Load(reader, file);
	});
	rejects(R"({"radarRangeMeters":0})", [&] {
		UiTuning{}.Load(reader, file);
	});
	rejects(R"({"flightVolume":1.5})", [&] {
		AudioSettings{}.Load(reader, file);
	});
	rejects(R"({"passLegsMeters":[1,2]})", [&] {
		EnemyAITuning{}.Load(reader, file);
	});
	rejects(R"({"minimumLeadFraction":0.9,"leadVariation":0.3})", [&] {
		EnemyGunTuning{}.Load(reader, file);
	});
	rejects(R"({"gun":{"smokeDrag":0}})", [&] {
		EffectSettings{}.Load(reader, file);
	});
	rejects(R"({"explosion":{"fragmentCount":25}})", [&] {
		EffectSettings{}.Load(reader, file);
	});
	rejects(R"({"textScale":0})", [&] {
		CinematicTextSettings{}.Load(reader, file);
	});
	rejects("{", [&] {
		CameraSettings{}.Load(reader, file);
	});
	write(R"({"passLegsMeters":[100,200,300,400,500,600]})");
	EnemyAITuning ai;
	ai.Load(reader, file);
	Require(ai.passLegsMeters[5] == 600, "Long numeric array pointers were not loaded");
	write(R"({"gun":{"flashLifetimeSeconds":0.01},"explosion":{"minRadius":30,"maxRadius":80}})");
	EffectSettings effects;
	effects.Load(reader, file);
	CombatEffectSystem customized, defaults;
	customized.Configure(effects);
	GunShotEvent shot{};
	shot.direction = {0, 0, -1};
	customized.SpawnGunShot(shot);
	defaults.SpawnGunShot(shot);
	customized.Update(0.02f);
	defaults.Update(0.02f);
	Require(customized.GetSnapshot().gunFlashes.empty() && !defaults.GetSnapshot().gunFlashes.empty(),
		"Effect lifetime tuning was ignored or leaked between instances");
	customized.SpawnExplosion({0, 100, 0}, {});
	Require(customized.GetSnapshot().explosions.front().radius == 30, "Explosion tuning not used");
	write(R"({"radarRangeMeters":2000})");
	UiTuning ui;
	ui.Load(reader, file);
	RenderView view;
	view.width = 1920;
	view.height = 1080;
	const auto layout = RadarMapPresenter::Layout(view, ui);
	Require(RadarMapPresenter::Project({2500, 0, 0}, {}, layout).outside &&
			!RadarMapPresenter::Project({2500, 0, 0}, {}, RadarMapPresenter::Layout(view)).outside,
		"Radar configuration ignored or leaked between instances");
	write(R"({"ammoCount":3,"lifetimeSeconds":12,"releaseOffsetMeters":20})");
	MissileSettings missile;
	missile.Load(reader, file);
	Require(CarrierMissileSystem::RequiredModelCapacity(4, missile) == 12,
		"Missile pool ignores ammo count/lifetime");
	CarrierMissileSystem weapon;
	weapon.ConfigureSettings(missile);
	Require(weapon.BuildLaunchFlight({0, 100, 0}, {}, {0, 0, -1}).position.y == 80,
		"Missile release offset ignored");
	write(R"({"damage":7,"initialDelaySeconds":0.01,"initialDelayRangeSeconds":0})");
	EnemyGunTuning gun;
	gun.Load(reader, file);
	EnemyGunPattern pattern;
	Require(pattern.Update(0.02f, 0, gun), "Enemy gun burst tuning ignored");
	const auto assets = repository / L"Assets";
	AudioSettings audio;
	audio.Load(reader, assets / L"Sound/audio.json");
	Require(fs::is_regular_file(assets / L"Sound" /
				fs::path(std::u8string(
					reinterpret_cast<const char8_t*>(audio.flightFile.data()), audio.flightFile.size()))),
		"Audio asset path not loaded");
	UiTextSettings text;
	text.Load(reader, assets / L"UI/labels.json");
	write(R"({"labels":{"操作方法":"Controls"}})");
	text.Load(reader, file);
	Require(text.Text("操作方法") == "Controls", "HUD labels not loaded");
	const auto opening = LoadCinematicSettings(reader, assets / L"Cinematics/opening.json");
	CinematicController controller;
	controller.Start(opening);
	Require(controller.GetPresentation().text.textScale == 1.5f, "Opening typography not propagated");
	const auto ending = LoadCinematicSettings(reader, assets / L"Cinematics/ending.json");
	Require(ending.text.textScale == 1, "Opening typography leaked to ending");
	write(R"({"playerStartPosition":[10,2500,-300],"antiAircraftDamage":15})");
	MissionSettings mission;
	mission.LoadTuning(reader, file);
	Require(mission.playerStartPosition.y == 2500 && mission.antiAircraftDamage == 15,
		"Mission tuning not loaded");
	std::cout << "PASS: gameplay JSON defaults, validation, instance isolation and runtime tuning\n";
}
