#pragma once
#include "Game/Combat/MissionState.h"
#include "Game/Weapons/BombFlight.h"

void TestMissionRules() {
	MissionState mission;
	mission.ConfirmDeckHit();
	Require(!mission.IsDeckDisabled(), "Deck hit before carrier phase advanced mission");
	Require(!mission.Advance(1, true), "Patrol skipped while enemy remained");
	Require(mission.Advance(0, true) && mission.GetPhase() == MissionPhase::CarrierAttack,
		"Patrol all-down must start bombing, not victory");
	Require(!mission.Advance(0, true), "Empty carrier phase was treated as all-down victory");
	mission.ConfirmDeckHit();
	mission.ConfirmDeckHit();
	Require(mission.Advance(0, true) && mission.GetPhase() == MissionPhase::FinalFighters,
		"Deck hit did not start final fighters");
	Require(!mission.Advance(2, true), "Deck hit spawned duplicate final fighters");
	Require(mission.Advance(0, true) && mission.GetPhase() == MissionPhase::Commander,
		"Commander did not follow fighters");
	Require(!mission.Advance(1, true), "Victory before last aircraft");
	auto simultaneous = mission;
	Require(simultaneous.Advance(0, false) && simultaneous.GetPhase() == MissionPhase::Failed,
		"Player defeat must win over simultaneous final kill");
	Require(mission.Advance(0, true) && mission.GetPhase() == MissionPhase::Complete,
		"Last aircraft did not complete mission");
	Require(!mission.Advance(0, true), "Completion was emitted twice");

	BombFlight bomb{{10, 1000, 20}, {200, 20, -300}};
	const auto expected = bomb.Predict(100);
	Require(expected.has_value(), "Bomb trajectory could not predict deck");
	const float time = *bomb.TimeToHeight(100);
	for (int i = 0; i < 100; ++i)
		bomb.Advance(time / 100);
	Require(std::abs(bomb.position.y - 100) < 0.02f && std::abs(bomb.position.x - expected->x) < 0.05f &&
			std::abs(bomb.position.z - expected->z) < 0.05f,
		"Bomb inherited velocity/gravity disagrees with impact prediction");
	Require(!BombFlight{{0, 10, 0}, {}}.Predict(100), "Bomb predicted a deck above release");

	ProjectileSystem hostile;
	GunSettings gun;
	gun.muzzleNames = {"a", "b", "c", "d"};
	gun.maxProjectiles = 4;
	hostile.Configure(gun, std::make_shared<Model>(), {0, 1, 0});
	Scene scene;
	DirectX::XMFLOAT4X4 world;
	DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(0, 100, -100));
	const CollisionTarget target{77, {{-10, -10, -10}, {10, 10, 10}}, {0, 100, -100}, world};
	hostile.FireSingle(scene, {0, 100, 0}, {0, 0, -1000}, AttackSource::AntiAircraft, 8);
	hostile.Update(scene, 0.2f, std::span(&target, 1));
	const auto hits = hostile.TakeHits();
	Require(hits.size() == 1 && hits[0].source == AttackSource::AntiAircraft && hits[0].damage == 8,
		"Hostile projectile lost attack source/damage or tunneled through player");
	for (int i = 0; i < 4; ++i)
		Require(hostile.FireSingle(scene, {0, 100, 0}, {0, 0, -1000}, AttackSource::EnemyAircraft, 5),
			"Projectile cap too small");
	Require(!hostile.FireSingle(scene, {0, 100, 0}, {0, 0, -1000}, AttackSource::EnemyAircraft, 5),
		"Hostile projectile cap ignored");
	std::cout
		<< "PASS: mission transitions, final-kill precedence, ballistic prediction and hostile damage/cap\n";
}

void TestMissionScene(const fs::path& repository, const fs::path& fixture) {
	using namespace DirectX;
	const auto assets = repository / L"Assets";
	const auto definition = FlightSceneLoader{}.Load({.environment = assets / L"Environment/environment.json",
		.playerAircraft = assets / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = assets / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = assets / L"Environment/retroFilm.json",
		.mission = assets / L"Combat/mission.json",
		.bomb = assets / L"Ammunition/Bomb/ammunition.json"});
	Require(definition.mission.patrolCount == 2 && definition.mission.bombReloadSeconds == 10,
		"Mission config not loaded");
	const auto window = CreateWindowW(L"STATIC", L"Mission regression (hidden)", WS_OVERLAPPEDWINDOW, 0, 0,
		64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Mission test window failed");
	{
		RenderDevice device;
		device.Initialize(window, 64, 64);
		CommandContext context;
		context.Initialize(&device);
		context.BeginFrame();
		auto player = std::make_shared<Model>();
		auto box = std::make_shared<Model>();
		Require(player->Load(device.GetDevice(), context.GetCommandList(), &device,
					(fixture / L"muzzles.dae").wstring()),
			"Mission player fixture failed");
		ModelLoadOptions options;
		options.origin = ModelOrigin::BoundsCenter;
		Require(box->Load(device.GetDevice(), context.GetCommandList(), &device,
					(fixture / L"combat-box.obj").wstring(), options),
			"Mission carrier fixture failed");
		// 本番と同じ生成経路で、初回更新前の表示とリトライを検証する。
		auto missionDefinition = definition;
		missionDefinition.ships.resize(3);
		for (size_t i = 0; i < missionDefinition.ships.size(); ++i) {
			auto& ship = missionDefinition.ships[i];
			ship.asset.id = "fixtureShip" + std::to_string(i);
			ship.position = {0, 0, -20000.0f - float(i) * 4000};
		}
		const auto resolveModel = [&](const std::string& id) {
			return id == PlayerModelId || id == AmmunitionModelId ? player : box;
		};
		auto initialized = FlightScene::CreateMission(missionDefinition, resolveModel, 1024, 768, 100000);
		Require(initialized.GetCombatSnapshot().enemies.size() == definition.mission.patrolCount &&
				initialized.GetCombatSnapshot().playerAltitudeMeters == 1500,
			"Mission factory returned an incomplete initial snapshot");
		const auto initialPlayerId = initialized.GetObjects().front().id;
		const auto initialEnemyPosition = initialized.GetCombatSnapshot().enemies.front().center;
		initialized.RemoveObject(initialPlayerId);
		initialized.Update({}, 0.01f);
		Require(initialized.GetCombatSnapshot().result == CombatResult::Defeat,
			"Removing player before first update must fail mission");
		initialized.OnResize(1600, 900);
		for (int retry = 0; retry < 2; ++retry) {
			initialized.Update({}, 0.01f, {}, {.retry = true});
			const auto& restored = initialized.GetCombatSnapshot();
			Require(restored.result == CombatResult::Active &&
					restored.missionPhase == MissionPhase::Patrol &&
					restored.playerHealth == definition.combat.playerHealth &&
					restored.enemies.size() == definition.mission.patrolCount &&
					restored.enemies.front().center.z == initialEnemyPosition.z &&
					initialized.GetProjectileCount() == 0 && initialized.GetHostileProjectileCount() == 0 &&
					initialized.GetBombCount() == 0 && initialized.GetEffectSnapshot().explosions.empty(),
				"Factory retry did not restore the completed initial state");
			XMFLOAT4X4 projection;
			XMStoreFloat4x4(&projection, initialized.GetRenderScene().GetCamera().GetProjection());
			Require(std::abs(projection._22 / projection._11 - 1600.0f / 900.0f) < 0.0001f &&
					initialized.GetObjects().front().model == player,
				"Retry restored an old viewport or replaced shared GPU assets");
			initialized.RemoveObject(initialPlayerId);
			initialized.Update({}, 0.01f);
		}
		bool rejectedMissingModel = false;
		try {
			FlightScene::CreateMission(
				missionDefinition,
				[](const std::string&) {
					return std::shared_ptr<Model>{};
				},
				1024, 768, 100000);
		} catch (const std::invalid_argument& error) {
			rejectedMissingModel = std::string(error.what()).find(PlayerModelId) != std::string::npos;
		}
		Require(rejectedMissingModel, "Mission factory accepted a missing player model");
		// Enemy weapon setup must not depend on whether player/mission setup ran first.
		auto weaponSettings = definition.mission;
		weaponSettings.maxHostileProjectiles = 4;
		weaponSettings.enemyGunInterval = 0.07f;
		const auto configuredScene = [&](bool weaponsFirst) {
			FlightScene candidate;
			candidate.Initialize(1, 1, 100000);
			auto combat = definition.combat;
			combat.playerHealth = 100000;
			candidate.ConfigureCombat(combat);
			candidate.SetPlayerAircraftPosition({0, 1500, 0});
			candidate.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
			candidate.AddPlayerObject(player);
			const auto configureWeapons = [&] {
				candidate.ConfigureEnemyWeapons(definition.playerWeapon, weaponSettings, player);
				candidate.ConfigurePlayerWeapon(definition.playerWeapon, player);
				candidate.ConfigureBombWeapon(definition.bomb, weaponSettings.bombReloadSeconds, box);
			};
			if (weaponsFirst)
				configureWeapons();
			candidate.ConfigureMission(weaponSettings, box, 1, definition.enemyAircraft.flightParameters,
				definition.enemyAIParameters);
			if (!weaponsFirst)
				configureWeapons();
			return candidate;
		};
		auto weaponsFirst = configuredScene(true);
		auto missionFirst = configuredScene(false);
		size_t mostBullets = 0;
		for (int frame = 0; frame < 3600; ++frame) {
			weaponsFirst.Update({}, 1.0f / 60);
			missionFirst.Update({}, 1.0f / 60);
			mostBullets = std::max(mostBullets, weaponsFirst.GetHostileProjectileCount());
			Require(weaponsFirst.GetHostileProjectileCount() == missionFirst.GetHostileProjectileCount() &&
					weaponsFirst.GetCombatSnapshot().playerHealth ==
						missionFirst.GetCombatSnapshot().playerHealth,
				"Initialization order changed enemy weapons");
			Require(mostBullets <= weaponSettings.maxHostileProjectiles,
				"Configured hostile projectile cap was ignored");
		}
		Require(mostBullets > 0, "Gun AI did not fire during the close-range pursuit simulation");

		FlightScene scene;
		scene.Initialize(1, 1, 100000);
		auto combat = definition.combat;
		combat.playerHealth = 100000;
		scene.ConfigureCombat(combat);
		scene.SetPlayerAircraftPosition({0, 1500, 0});
		scene.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
		const auto playerId = scene.AddPlayerObject(player);
		scene.ConfigureMission(definition.mission, box, 1, definition.enemyAircraft.flightParameters,
			definition.enemyAIParameters);
		scene.ConfigureBombWeapon(definition.bomb, definition.mission.bombReloadSeconds, box);
		scene.ConfigureEnemyWeapons(definition.playerWeapon, definition.mission, player);
		scene.ConfigurePlayerWeapon(definition.playerWeapon, player);
		XMFLOAT4X4 carrierWorld;
		XMStoreFloat4x4(&carrierWorld,
			XMMatrixScaling(3000, 100, 3000) * XMMatrixRotationY(0.4f) * XMMatrixTranslation(0, 100, -6000));
		const auto carrierId = scene.AddObject(box, carrierWorld);
		scene.RegisterMissionShip(carrierId, true);
		scene.Update({}, 0.01f);
		Require(
			scene.GetCombatSnapshot().enemies.size() == 2, "Mission did not start with two patrol aircraft");
		const auto checkDefeatRetry = [&](FlightScene checkpoint) {
			checkpoint.RemoveObject(playerId);
			checkpoint.Update({}, 0.01f);
			Require(checkpoint.GetCombatSnapshot().result == CombatResult::Defeat,
				"Mission phase did not fail when player was removed");
			CombatInput retryInput;
			retryInput.retry = true;
			checkpoint.Update({}, 0.01f, {}, retryInput);
			Require(checkpoint.GetCombatSnapshot().missionPhase == MissionPhase::Patrol &&
					checkpoint.GetCombatSnapshot().enemies.size() == 2 &&
					!checkpoint.GetCombatSnapshot().deckDisabled &&
					checkpoint.GetCombatSnapshot().bombCooldown == 0 &&
					checkpoint.GetEffectSnapshot().explosions.empty() &&
					checkpoint.GetCombatSnapshot().result == CombatResult::Active,
				"Defeat retry retained phase, deck, ordnance or effects");
		};
		checkDefeatRetry(scene);
		const auto removeEnemies = [&] {
			const auto enemies = scene.GetCombatSnapshot().enemies;
			for (const auto& enemy : enemies)
				scene.RemoveObject(enemy.id);
			scene.Update({}, 0.01f);
		};
		removeEnemies();
		Require(scene.GetCombatSnapshot().missionPhase == MissionPhase::CarrierAttack &&
				scene.GetCombatSnapshot().result == CombatResult::Active,
			"Patrol clear ended mission");
		const auto carrierPhase = scene;
		scene.RemoveObject(playerId);
		scene.Update({}, 0.01f);
		Require(
			scene.GetCombatSnapshot().result == CombatResult::Defeat, "Missing player did not fail mission");
		CombatInput retry;
		retry.retry = true;
		scene.Update({}, 0.01f, {}, retry);
		Require(scene.GetCombatSnapshot().missionPhase == MissionPhase::Patrol &&
				scene.GetCombatSnapshot().enemies.size() == 2 && scene.GetCombatSnapshot().bombCooldown == 0,
			"Defeat retry did not restore mission");
		scene = carrierPhase;
		CombatInput gunfire;
		gunfire.weapon.fire = true;
		for (int i = 0; i < 20; ++i)
			scene.Update({}, 0.05f, {}, gunfire);
		Require(!scene.GetCombatSnapshot().deckDisabled &&
				scene.GetCombatSnapshot().missionPhase == MissionPhase::CarrierAttack,
			"Cannon fire disabled the flight deck");

		// Missing the rotated deck consumes a bomb, but reload permits another attempt.
		scene.SetPlayerAircraftPosition({100000, 1500, 0});
		CombatInput drop;
		drop.bombPressCount = 1;
		scene.Update({}, 0.01f, {}, drop);
		Require(scene.GetCombatSnapshot().bombCooldown == 10, "Bomb was not released/reloaded");
		for (int i = 0; i < 80; ++i)
			scene.Update({}, 0.25f);
		Require(scene.GetCombatSnapshot().missionPhase == MissionPhase::CarrierAttack &&
				scene.GetCombatSnapshot().bombCooldown == 0,
			"Miss did not allow bombing retry");
		Require(scene.GetObjects().size() == 2, "Missed bomb did not expire at sea");
		scene.SetPlayerAircraftPosition({0, 1500, 0});
		scene.Update({}, 0.01f, {}, drop);
		for (int i = 0; i < 120 && scene.GetCombatSnapshot().missionPhase == MissionPhase::CarrierAttack; ++i)
			scene.Update({}, 0.25f);
		Require(scene.GetCombatSnapshot().deckDisabled && scene.GetCombatSnapshot().enemies.size() == 2 &&
				scene.GetCombatSnapshot().missionPhase == MissionPhase::FinalFighters,
			"Bomb did not hit rotated deck/spawn final fighters once");
		scene.Update({}, 0.01f);
		Require(scene.GetCombatSnapshot().enemies.size() == 2, "Final fighters duplicated");
		checkDefeatRetry(scene);
		removeEnemies();
		Require(scene.GetCombatSnapshot().enemies.size() == 1 &&
				scene.GetCombatSnapshot().enemies[0].name == "SQUADRON LEADER" &&
				scene.GetCombatSnapshot().enemies[0].health == combat.enemyHealth * 1.5f,
			"Commander settings/phase incorrect");
		checkDefeatRetry(scene);
		removeEnemies();
		Require(scene.GetCombatSnapshot().result == CombatResult::Victory, "Last kill did not win mission");
		scene.Update({}, 0.01f, {}, retry);
		Require(scene.GetCombatSnapshot().missionPhase == MissionPhase::Patrol &&
				!scene.GetCombatSnapshot().deckDisabled && scene.GetCombatSnapshot().enemies.size() == 2 &&
				scene.GetCombatSnapshot().result == CombatResult::Active,
			"Victory retry retained mission state");
		context.EndFrame();
		context.WaitForGpu();
	}
	DestroyWindow(window);
	std::cout << "PASS: full mission scene, rotated deck bombing, miss/reload, one-time waves and retry\n";
}
