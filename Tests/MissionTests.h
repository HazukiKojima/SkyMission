#pragma once
#include "Game/Combat/MissionState.h"
#include "Game/Weapons/BombFlight.h"

void TestCarrierMissileFlight() {
	Game::CarrierMissileFlight missile{{0, 1500, 0}, {0, -1000, -6000}};
	const auto impact = missile.Predict(500);
	Require(impact && impact->z == -6000, "Missile impact prediction did not use launch speed");
	missile.Advance(1);
	Require(missile.position.y == 500 && missile.position.z == -6000 && missile.velocity.y == -1000,
		"Carrier missile was slowed by gravity");
	Require(!Game::CarrierMissileFlight{{0, 1500, 0}, {0, 0, -6000}}.Predict(500),
		"Level missile predicted an impossible lower impact");
	Require(!Game::CarrierMissileFlight{{0, 1500, 0}, {0, 1000, -6000}}.TimeToHeight(0),
		"Climbing missile expired at a sea crossing behind it");
	std::cout << "PASS: fast linear carrier missile, impact prediction and sea crossing\n";
}

void TestMissionRules() {
	// HPの塗り幅とゼロ時の空バーを、通常・小画面・横長の各解像度で確認する。
	for (const auto dimensions :
		{DirectX::XMUINT2{1920, 1080}, DirectX::XMUINT2{800, 600}, DirectX::XMUINT2{2560, 1080}}) {
		Engine::RenderView view;
		view.width = dimensions.x;
		view.height = dimensions.y;
		DirectX::XMStoreFloat4x4(&view.view, DirectX::XMMatrixIdentity());
		DirectX::XMStoreFloat4x4(&view.projection,
			DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, float(view.width) / view.height, 1, 10000));
		for (float health : {300.0f, 200.0f, 0.0f}) {
			CombatSnapshot snapshot;
			snapshot.missionEnabled = snapshot.carrierPresent = true;
			snapshot.carrierMaxHealth = 300;
			snapshot.carrierHealth = health;
			Engine::HudDrawList list;
			list.Reset(view.width, view.height);
			CombatHudPresenter{}.Build(snapshot, view, list);
			// 頂点はNDC座標なので、画面左側では右端も負になる
			float left = std::numeric_limits<float>::infinity();
			float right = -std::numeric_limits<float>::infinity();
			size_t fillVertices = 0;
			for (const auto& vertex : list.GetVertices())
				if (vertex.color.x == HudStyle::Caution.x && vertex.color.y == HudStyle::Caution.y &&
					vertex.color.w == HudStyle::Caution.w) {
					left = std::min(left, vertex.position.x);
					right = std::max(right, vertex.position.x);
					++fillVertices;
				}
			const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
			Require(health == 0 ? fillVertices == 0
								: fillVertices == 6 &&
						std::abs((right - left) * view.width * 0.5f - 760 * scale * health / 300) < 0.01f,
				"Carrier HP fill width did not follow health or viewport scale");
		}
	}

	MissionState mission;
	mission.ApplyCarrierDamage(100);
	Require(!mission.IsDeckDisabled(), "Deck hit before carrier phase advanced mission");
	Require(!mission.Advance(1, true), "Patrol skipped while enemy remained");
	Require(mission.Advance(0, true) && mission.GetPhase() == MissionPhase::CarrierAttack,
		"Patrol all-down must start bombing, not victory");
	Require(!mission.Advance(0, true), "Empty carrier phase was treated as all-down victory");
	Require(!mission.ApplyCarrierDamage(0) && !mission.ApplyCarrierDamage(-1) &&
			!mission.ApplyCarrierDamage(std::numeric_limits<float>::quiet_NaN()),
		"Invalid carrier damage accepted");
	Require(mission.ApplyCarrierDamage(100) && mission.GetCarrierHealth() == 200 && !mission.IsDeckDisabled(),
		"First bomb did not reduce carrier health");
	Require(!mission.Advance(0, true), "Partial damage advanced carrier phase");
	mission.ApplyCarrierDamage(100);
	mission.ApplyCarrierDamage(1000);
	Require(mission.GetCarrierHealth() == 0 && mission.IsDeckDisabled() && !mission.ApplyCarrierDamage(100),
		"Carrier overkill or repeated defeat was accepted");
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
		.mission = assets / L"Combat/mission.json"});
	Require(definition.mission.patrolCount == 4 && definition.mission.finalFighterCount == 4 &&
			definition.mission.bombReloadSeconds == 4,
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

		{
			auto tuned = missionDefinition;
			tuned.mission.playerStartPosition = {120, 2500, -200};
			tuned.camera.fovDegrees = 75;
			tuned.bomb.missile.ammoCount = 3;
			tuned.playerAircraft.animation.propellerAngularSpeedPerSpeed = 0;
			tuned.enemyAircraft.animation.propellerAngularSpeedPerSpeed = 0;
			auto custom = FlightScene::CreateMission(tuned, resolveModel, 1024, 768, 100000);
			Require(custom.GetCombatSnapshot().playerAltitudeMeters == 2500,
				"Configured player spawn was ignored by mission factory");
			Require(custom.GetCombatSnapshot().carrierMissileAmmo == 3,
				"Configured missile ammo was ignored by mission factory");
			Require(custom.GetCombatSnapshot().carrierMissileCapacity == 3,
				"Configured missile HUD capacity ignored");
			custom.Update({}, 0.1f);
			Require(std::all_of(custom.GetObjects().begin(), custom.GetObjects().end(),
						[](const auto& object) {
							return object.nodeRotationRadians == 0;
						}),
				"Stationary propeller setting was ignored by the mission factory");
			custom.RemoveObject(custom.GetObjects().front().id);
			custom.Update({}, 0.01f);
			custom.Update({}, 0.01f, {}, {.retry = true});
			custom.Update({}, 0.1f);
			Require(custom.GetCombatSnapshot().carrierMissileCapacity == 3 &&
					std::all_of(custom.GetObjects().begin(), custom.GetObjects().end(),
						[](const auto& object) {
							return object.nodeRotationRadians == 0;
						}),
				"Retry lost configured animation or missile HUD capacity");
		}
		auto initialized = FlightScene::CreateMission(missionDefinition, resolveModel, 1024, 768, 100000);
		Require(initialized.GetCombatSnapshot().enemies.size() == definition.mission.patrolCount &&
				initialized.GetCombatSnapshot().playerAltitudeMeters == 1500,
			"Mission factory returned an incomplete initial snapshot");
		const auto initialPlayerId = initialized.GetObjects().front().id;
		const auto initialEnemyPosition = initialized.GetCombatSnapshot().enemies.front().center;
		{
			auto natural = initialized;
			natural.EnableOpening(definition.opening, box, 20, 100000);
			auto skipped = natural;
			const auto initial = natural.GetCombatSnapshot();
			CombatInput noise;
			noise.weapon.fire = true;
			noise.missilePressCount = 2;
			noise.nextTarget = true;
			for (int frame = 0; frame < 14; ++frame)
				natural.Update({.pitch = 1, .throttle = 1}, 0.25f, {}, noise);
			Require(natural.GetCombatSnapshot().cinematic.active &&
					natural.GetCombatSnapshot().playerPosition.z == initial.playerPosition.z &&
					natural.GetCombatSnapshot().enemies.front().center.z ==
						initial.enemies.front().center.z &&
					natural.GetCombatSnapshot().playerHealth == initial.playerHealth &&
					natural.GetCombatSnapshot().acquireProgress == initial.acquireProgress &&
					natural.GetProjectileCount() == 0 && natural.GetHostileProjectileCount() == 0 &&
					natural.GetCombatSnapshot().carrierMissileAmmo == 2,
				"Opening advanced the battle or accepted combat input");
			// 自機のカットでは無入力飛行を続け、終了後の通常更新へ連続してつなぐ
			auto neutral = natural;
			for (int frame = 0; frame < 6; ++frame) {
				natural.Update({.pitch = 1, .throttle = 1}, 0.25f, {}, noise);
				neutral.Update({}, 0.25f);
			}
			Require(!natural.GetCombatSnapshot().cinematic.active &&
					natural.GetCombatSnapshot().playerPosition.z < initial.playerPosition.z &&
					natural.GetCombatSnapshot().playerPosition.z ==
						neutral.GetCombatSnapshot().playerPosition.z &&
					natural.GetCombatSnapshot().playerPosition.y ==
						neutral.GetCombatSnapshot().playerPosition.y &&
					natural.GetProjectileCount() == 0 && natural.GetHostileProjectileCount() == 0 &&
					natural.GetCombatSnapshot().playerHealth == initial.playerHealth &&
					natural.GetCombatSnapshot().carrierMissileAmmo == 2,
				"Opening handoff froze flight or accepted control/weapon input");
			const auto handoffPosition = natural.GetCombatSnapshot().playerPosition;
			natural.Update({}, 0.01f);
			Require(natural.GetCombatSnapshot().playerPosition.z < handoffPosition.z,
				"Player stopped moving after the opening handoff");
			skipped.OnResize(2560, 1080);
			noise.skipOpening = true;
			skipped.Update({}, 0.1f, {}, noise);
			{
				auto registered = initialized;
				registered.ConfigureCinematics(
					{{"opening", definition.opening}},
					[&](const std::string&) {
						return FlightScene::CinematicModel{box, 20};
					},
					100000);
				Require(registered.PlayCinematic("opening") &&
						registered.GetCombatSnapshot().cinematic.briefingLine == 0,
					"Registered opening did not enable its briefing");
				registered.Update({}, 0.1f, {}, {.skipOpening = true});
				XMFLOAT4X4 actual, expected;
				XMStoreFloat4x4(&actual, registered.GetRenderScene().GetCamera().GetView());
				XMStoreFloat4x4(&expected, skipped.GetRenderScene().GetCamera().GetView());
				for (size_t row = 0; row < 4; ++row)
					for (size_t column = 0; column < 4; ++column)
						Require(std::abs(actual.m[row][column] - expected.m[row][column]) < 0.001f,
							"Shared opening API did not prepare the battle camera before skip");
			}
			Require(!skipped.GetCombatSnapshot().cinematic.active && skipped.GetProjectileCount() == 0 &&
					skipped.GetCombatSnapshot().elapsedSeconds == initial.elapsedSeconds &&
					skipped.GetCombatSnapshot().playerPosition.z == initial.playerPosition.z,
				"Skipped opening consumed inputs or changed the battle start camera");
			skipped.RemoveObject(initialPlayerId);
			skipped.Update({}, 0.01f);
			skipped.Update({}, 0.01f, {}, {.retry = true});
			Require(skipped.GetCombatSnapshot().cinematic.active &&
					skipped.GetCombatSnapshot().cinematic.elapsedSeconds == 0 &&
					skipped.GetCombatSnapshot().playerHealth == initial.playerHealth,
				"Retry did not restart the opening from the initial battle state");
		}
		{
			auto reusable = initialized;
			auto clips = definition.cinematics;
			clips.erase("opening");
			reusable.ConfigureCinematics(
				clips,
				[&](const std::string& id) {
					return FlightScene::CinematicModel{resolveModel(id), 1};
				},
				100000);
			Require(!reusable.PlayCinematic("unknown"), "Unknown cinematic ID started playback");
			const auto removePatrol = [&] {
				const auto enemies = reusable.GetCombatSnapshot().enemies;
				for (const auto& enemy : enemies)
					reusable.RemoveObject(enemy.id);
				reusable.Update({}, 0.01f);
			};
			removePatrol();
			Require(reusable.GetCombatSnapshot().cinematic.active &&
					reusable.GetCombatSnapshot().cinematic.cinematicId == "midMission",
				"Carrier-attack transition did not start its cinematic");
			const auto paused = reusable.GetCombatSnapshot();
			for (int frame = 0; frame < 3; ++frame)
				reusable.Update({.pitch = 1, .throttle = 1}, 0.25f, {},
					{.weapon = {.fire = true}, .missilePressCount = 2});
			Require(reusable.GetCombatSnapshot().elapsedSeconds == paused.elapsedSeconds &&
					reusable.GetCombatSnapshot().playerPosition.z == paused.playerPosition.z &&
					reusable.GetCombatSnapshot().playerHealth == paused.playerHealth &&
					reusable.GetProjectileCount() == 0 && reusable.GetCarrierMissileCount() == 0 &&
					!reusable.PlayCinematic("ending"),
				"Cinematic changed battle state or allowed overlapping playback");
			reusable.Update({}, 0.01f, {}, {.skipOpening = true});
			Require(reusable.TakeCinematicCompletion() == CinematicCompletion::Skipped,
				"Mission host lost the skip completion");
			reusable.Update({}, 0.01f);
			Require(!reusable.GetCombatSnapshot().cinematic.active &&
					reusable.GetCombatSnapshot().elapsedSeconds > paused.elapsedSeconds,
				"Battle did not resume or repeated its intermediate cinematic");
			reusable.RemoveObject(initialPlayerId);
			reusable.Update({}, 0.01f);
			Require(reusable.GetCombatSnapshot().cinematic.cinematicId == "defeat" &&
					reusable.GetCombatSnapshot().cinematic.active,
				"Defeat did not select its ending");
			for (int frame = 0; frame < 16; ++frame)
				reusable.Update({}, 0.25f);
			Require(reusable.TakeCinematicCompletion() == CinematicCompletion::Completed &&
					!reusable.GetCombatSnapshot().cinematic.active,
				"Natural ending did not finish");
			reusable.Update({}, 0.01f);
			Require(!reusable.GetCombatSnapshot().cinematic.active, "Ending repeated after completion");
			reusable.Update({}, 0.01f, {}, {.retry = true});
			removePatrol();
			Require(reusable.GetCombatSnapshot().cinematic.active &&
					reusable.GetCombatSnapshot().cinematic.cinematicId == "midMission",
				"Retry did not reset cinematic trigger state");
		}
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
					initialized.GetCarrierMissileCount() == 0 &&
					initialized.GetEffectSnapshot().explosions.empty(),
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
				candidate.ConfigureCarrierMissileWeapon(
					definition.bomb, weaponSettings.bombReloadSeconds, box);
			};
			if (weaponsFirst) configureWeapons();
			candidate.ConfigureMission(weaponSettings, box, 1, definition.enemyAircraft.flightParameters,
				definition.enemyAIParameters);
			if (!weaponsFirst) configureWeapons();
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
		scene.ConfigureCarrierMissileWeapon(definition.bomb, definition.mission.bombReloadSeconds, box);
		scene.ConfigureEnemyWeapons(definition.playerWeapon, definition.mission, player);
		scene.ConfigurePlayerWeapon(definition.playerWeapon, player);
		XMFLOAT4X4 carrierWorld;
		XMStoreFloat4x4(&carrierWorld,
			XMMatrixScaling(3000, 100, 3000) * XMMatrixRotationY(0.4f) * XMMatrixTranslation(0, 100, -6000));
		const auto carrierId = scene.AddObject(box, carrierWorld);
		scene.RegisterMissionShip(carrierId, true);
		scene.Update({}, 0.01f);
		Require(scene.GetCombatSnapshot().enemies.size() == definition.mission.patrolCount,
			"Mission did not start with six patrol aircraft");
		{
			const auto& patrol = scene.GetCombatSnapshot().enemies;
			float minAltitude = patrol.front().center.y, maxAltitude = minAltitude;
			float minDepth = patrol.front().center.z, maxDepth = minDepth;
			for (size_t i = 0; i < patrol.size(); ++i) {
				minAltitude = std::min(minAltitude, patrol[i].center.y);
				maxAltitude = std::max(maxAltitude, patrol[i].center.y);
				minDepth = std::min(minDepth, patrol[i].center.z);
				maxDepth = std::max(maxDepth, patrol[i].center.z);
				for (size_t j = i + 1; j < patrol.size(); ++j)
					Require(std::hypot(patrol[i].center.x - patrol[j].center.x,
								patrol[i].center.z - patrol[j].center.z) > 1000,
						"Patrol formation spawned in a tight cluster");
			}
			Require(maxAltitude - minAltitude > 1000 && maxDepth - minDepth > 2000,
				"Patrol formation did not stagger altitude and approach distance");
		}
		// A nearby escort firing into the enlarged player fixture guarantees a gun hit.
		{
			auto aaScene = scene;
			aaScene.RemoveObject(playerId);
			aaScene.AddPlayerObject(box);
			aaScene.SetPlayerAircraftVisualScale(1000);
			XMFLOAT4X4 escortWorld;
			XMStoreFloat4x4(&escortWorld, XMMatrixTranslation(0, 1000, 100));
			aaScene.RegisterMissionShip(aaScene.AddObject(box, escortWorld), false);
			bool heardGunHit = false;
			for (int frame = 0; frame < 60 && !heardGunHit; ++frame) {
				const float health = aaScene.GetCombatSnapshot().playerHealth;
				aaScene.Update({}, 0.1f);
				const auto& state = aaScene.GetCombatSnapshot();
				heardGunHit = state.audio.gunHits > 0;
				Require((state.playerHealth < health) == heardGunHit && !state.audio.missileHits,
					"Anti-aircraft damage/audio classification mismatch");
			}
			Require(heardGunHit, "Anti-aircraft fixture did not produce gun hit audio");
		}
		const auto checkDefeatRetry = [&](FlightScene checkpoint) {
			checkpoint.RemoveObject(playerId);
			checkpoint.Update({}, 0.01f);
			Require(checkpoint.GetCombatSnapshot().result == CombatResult::Defeat,
				"Mission phase did not fail when player was removed");
			CombatInput retryInput;
			retryInput.retry = true;
			checkpoint.Update({}, 0.01f, {}, retryInput);
			Require(checkpoint.GetCombatSnapshot().missionPhase == MissionPhase::Patrol &&
					checkpoint.GetCombatSnapshot().enemies.size() == definition.mission.patrolCount &&
					!checkpoint.GetCombatSnapshot().deckDisabled &&
					checkpoint.GetCombatSnapshot().carrierHealth == 300 &&
					checkpoint.GetEffectSnapshot().deckSmoke.empty() &&
					checkpoint.GetCombatSnapshot().missileCooldownSeconds == 0 &&
					checkpoint.GetCombatSnapshot().carrierMissileAmmo == 2 &&
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
		Require(scene.GetCombatSnapshot().selected == carrierId,
			"Carrier was not automatically selected on entering mission two");
		const auto carrierPhase = scene;
		scene.RemoveObject(playerId);
		scene.Update({}, 0.01f);
		Require(
			scene.GetCombatSnapshot().result == CombatResult::Defeat, "Missing player did not fail mission");
		CombatInput retry;
		retry.retry = true;
		scene.Update({}, 0.01f, {}, retry);
		Require(scene.GetCombatSnapshot().missionPhase == MissionPhase::Patrol &&
				scene.GetCombatSnapshot().enemies.size() == definition.mission.patrolCount &&
				scene.GetCombatSnapshot().missileCooldownSeconds == 0,
			"Defeat retry did not restore mission");
		scene = carrierPhase;
		CombatInput gunfire;
		gunfire.weapon.fire = true;
		for (int i = 0; i < 20; ++i)
			scene.Update({}, 0.05f, {}, gunfire);
		Require(!scene.GetCombatSnapshot().deckDisabled &&
				scene.GetCombatSnapshot().missionPhase == MissionPhase::CarrierAttack,
			"Cannon fire disabled the flight deck");

		// 後方・遠距離でも任務目標を保持し、未ロックの発射は弾数を消費しない
		scene.SetPlayerAircraftPosition({100000, 1500, -10000});
		CombatInput drop;
		drop.missilePressCount = 1;
		scene.Update({}, 0.25f, {}, drop);
		Require(scene.GetCombatSnapshot().selected == carrierId &&
				scene.GetCombatSnapshot().lockState == LockState::Selected &&
				scene.GetCombatSnapshot().carrierMissileAmmo == 2 &&
				scene.GetCombatSnapshot().audio.missileLaunches == 0,
			"Distant rear carrier was not selected or unlocked missile consumed ammunition");
		const auto lockCarrier = [&](FlightScene& combatScene) {
			combatScene.SetPlayerAircraftPosition({100000, 1500, 0});
			combatScene.Update({}, 0.25f);
			combatScene.SetPlayerAircraftPosition({0, 1500, 2000});
			combatScene.Update({}, 0.1f);
			Require(combatScene.GetCombatSnapshot().lockState == LockState::Acquiring &&
					combatScene.GetCombatSnapshot().acquireProgress > 0 &&
					combatScene.GetCombatSnapshot().acquireProgress < 1,
				"Carrier lock completed without the required acquisition time");
			for (int i = 0; i < 23; ++i)
				combatScene.Update({}, 0.1f);
			Require(combatScene.GetCombatSnapshot().selected == carrierId &&
					combatScene.GetCombatSnapshot().lockState == LockState::Locked &&
					combatScene.GetCombatSnapshot().acquireProgress == 1 &&
					combatScene.GetCombatSnapshot().enemies.empty(),
				"Carrier did not lock without being counted as an enemy aircraft");
		};
		{
			auto lockLoss = scene;
			lockCarrier(lockLoss);
			lockLoss.SetPlayerAircraftPosition({100000, 1500, 0});
			lockLoss.Update({}, 0.25f);
			Require(lockLoss.GetCombatSnapshot().lockState == LockState::Selected &&
					lockLoss.GetCombatSnapshot().selected == carrierId &&
					lockLoss.GetCombatSnapshot().acquireProgress == 0 &&
					lockLoss.GetCombatSnapshot().carrierMissileAmmo == 2,
				"Distant carrier retained a stale lock or aiming consumed ammunition");
		}
		{
			auto volley = scene;
			lockCarrier(volley);
			CombatInput doubleClick;
			doubleClick.missilePressCount = 3;
			volley.Update({}, 0.01f, {}, doubleClick);
			Require(volley.GetCombatSnapshot().audio.missileLaunches == 2 &&
					volley.GetCombatSnapshot().carrierMissileAmmo == 0,
				"Queued double-click did not fire exactly the two loaded missiles");
			// Guidance retains the launched target even after the aircraft breaks its lock.
			volley.SetPlayerAircraftPosition({100000, 1500, 0});
			{
				auto reloading = volley;
				for (int frame = 0; frame < 15; ++frame)
					reloading.Update({}, 0.25f);
				Require(reloading.GetCombatSnapshot().carrierMissileAmmo == 0 &&
						reloading.GetCombatSnapshot().missileCooldownSeconds > 0,
					"Missile magazine reloaded before four seconds");
				reloading.Update({}, 0.25f);
				Require(reloading.GetCombatSnapshot().carrierMissileAmmo == 2 &&
						reloading.GetCombatSnapshot().missileCooldownSeconds == 0,
					"Missile magazine did not reload after four seconds");
			}
			for (int frame = 0; frame < 32 && !volley.GetCombatSnapshot().deckDisabled; ++frame)
				volley.Update({}, 0.25f);
			Require(volley.GetCombatSnapshot().deckDisabled &&
					volley.GetCombatSnapshot().missionPhase == MissionPhase::FinalFighters &&
					volley.GetCombatSnapshot().audio.shipExplosions == 1,
				"Two fast missiles did not destroy the rotated carrier exactly once");
		}
		for (int missileIndex = 0; missileIndex < 2; ++missileIndex) {
			lockCarrier(scene);
			const float before = scene.GetCombatSnapshot().carrierHealth;
			scene.Update({}, 0.01f, {}, drop);
			for (int i = 0; i < 32 && scene.GetCombatSnapshot().carrierHealth == before; ++i)
				scene.Update({}, 0.25f);
			Require(scene.GetCombatSnapshot().carrierHealth == 150 - missileIndex * 150,
				"Carrier did not require exactly two missile hits");
			if (missileIndex == 0) {
				Require(scene.GetCombatSnapshot().missionPhase == MissionPhase::CarrierAttack &&
						!scene.GetCombatSnapshot().audio.shipExplosions &&
						scene.GetEffectSnapshot().deckSmoke.empty(),
					"First missile prematurely destroyed carrier");
				checkDefeatRetry(scene);
			}
		}
		Require(scene.GetCombatSnapshot().deckDisabled &&
				scene.GetCombatSnapshot().enemies.size() == definition.mission.finalFighterCount &&
				scene.GetCombatSnapshot().missionPhase == MissionPhase::FinalFighters,
			"Missiles did not hit carrier/spawn final fighters once");
		Require(scene.GetCombatSnapshot().audio.shipExplosions == 1, "Deck audio missing or duplicated");
		const auto& carrierExplosion = scene.GetEffectSnapshot().explosions.back();
		Require(carrierExplosion.radius == CarrierEffectSettings::explosionRadius &&
				std::abs(carrierExplosion.position.x) < 0.01f &&
				std::abs(carrierExplosion.position.y - 100) < 0.01f &&
				std::abs(carrierExplosion.position.z + 6000) < 0.01f,
			"Carrier explosion was not large or centered on the transformed model");
		Require(!scene.GetEffectSnapshot().deckSmoke.empty(), "Carrier defeat did not emit smoke");
		scene.Update({}, 0.01f);
		Require(scene.GetCombatSnapshot().enemies.size() == definition.mission.finalFighterCount,
			"Final fighters duplicated");
		for (const auto& enemy : scene.GetCombatSnapshot().enemies)
			Require(std::hypot(enemy.center.x, enemy.center.z + 6000) < 8000,
				"Reinforcement spawned outside carrier defense area");
		Require(!scene.GetCombatSnapshot().audio.shipExplosions, "Deck audio persisted across frames");
		checkDefeatRetry(scene);
		removeEnemies();
		{
			auto victoryMovie = scene;
			victoryMovie.ConfigureCinematics(
				definition.cinematics,
				[&](const std::string& id) {
					return FlightScene::CinematicModel{resolveModel(id), 1};
				},
				100000);
			const auto enemies = victoryMovie.GetCombatSnapshot().enemies;
			for (const auto& enemy : enemies)
				victoryMovie.RemoveObject(enemy.id);
			victoryMovie.Update({}, 0.01f);
			Require(victoryMovie.GetCombatSnapshot().result == CombatResult::Victory &&
					victoryMovie.GetCombatSnapshot().cinematic.active &&
					victoryMovie.GetCombatSnapshot().cinematic.cinematicId == "ending",
				"Victory did not select its ending cinematic");
		}
		Require(scene.GetCombatSnapshot().enemies.size() == 1 &&
				scene.GetCombatSnapshot().enemies[0].name == "敵編隊長機" &&
				scene.GetCombatSnapshot().enemies[0].health == combat.enemyHealth * 1.5f,
			"Commander settings/phase incorrect");
		checkDefeatRetry(scene);
		removeEnemies();
		Require(scene.GetCombatSnapshot().result == CombatResult::Victory, "Last kill did not win mission");
		for (int frame = 0; frame < 100; ++frame)
			scene.Update({}, 0.25f);
		Require(!scene.GetEffectSnapshot().deckSmoke.empty() &&
				scene.GetEffectSnapshot().deckSmoke.size() <= CarrierEffectSettings::maxSmokeCount &&
				scene.GetEffectSnapshot().deckSmoke.back().age == 0 &&
				scene.GetEffectSnapshot().explosions.empty(),
			"Wreck smoke stopped after victory or grew unbounded");
		const auto smokeOrigin = scene.GetEffectSnapshot().deckSmoke.back().position;
		Require(std::abs(smokeOrigin.x) < 0.01f && std::abs(smokeOrigin.y - 100) < 0.01f &&
				std::abs(smokeOrigin.z + 6000) < 0.01f,
			"Wreck smoke was not emitted from carrier center");
		scene.Update({}, 0.01f, {}, retry);
		Require(scene.GetCombatSnapshot().missionPhase == MissionPhase::Patrol &&
				!scene.GetCombatSnapshot().deckDisabled &&
				scene.GetCombatSnapshot().enemies.size() == definition.mission.patrolCount &&
				scene.GetCombatSnapshot().result == CombatResult::Active,
			"Victory retry retained mission state");
		context.EndFrame();
		context.WaitForGpu();
	}
	DestroyWindow(window);
	std::cout << "PASS: full mission scene, automatic carrier selection, timed lock, launch gate, two-hit "
				 "carrier, one-time waves "
				 "and retry\n";
}
