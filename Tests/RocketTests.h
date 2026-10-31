#pragma once
#include "Game/Weapons/RocketSystem.h"
#include "Game/Weapons/RocketLauncher.h"

void TestRocketInputAndFlight() {
	using namespace DirectX;
	RawInputState raw;
	raw.SetActive(true);
	const auto device = reinterpret_cast<HANDLE>(std::uintptr_t(41));
	const auto key = [&](USHORT flags) {
		RAWKEYBOARD packet{};
		packet.VKey = 'F';
		packet.MakeCode = 0x21;
		packet.Flags = flags;
		raw.ProcessKeyboard(device, packet);
	};
	key(RI_KEY_MAKE);
	key(RI_KEY_MAKE);
	key(RI_KEY_BREAK);
	key(RI_KEY_MAKE);
	const auto input = CombatController{}.ReadInput(raw.ConsumeFrame());
	Require(input.rocketPressCount == 2 && !input.weapon.fire, "F double press or repeat handling failed");
	Require(CombatController{}.ReadInput(raw.ConsumeFrame()).rocketPressCount == 0,
		"Held F repeated rocket fire");
	key(RI_KEY_BREAK);
	key(RI_KEY_MAKE);
	raw.SetActive(false);
	raw.SetActive(true);
	Require(raw.ConsumeFrame().GetKeyPressCount('F') == 0, "Focus loss retained rocket press");
	RocketSettings settings;
	for (float invalid :
		{0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
		auto bad = settings;
		bad.turnDegreesPerSecond = invalid;
		bool rejected = false;
		try {
			ValidateRocketSettings(bad);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Invalid rocket turn rate accepted");
	}
	for (float invalid : {0.0f, -1.0f, 181.0f, std::numeric_limits<float>::quiet_NaN()}) {
		auto bad = settings;
		bad.seekerAngleDegrees = invalid;
		bool rejected = false;
		try {
			ValidateRocketSettings(bad);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Invalid missile seeker angle accepted");
	}
	XMFLOAT4X4 world;
	XMStoreFloat4x4(&world, XMMatrixTranslation(0, 100, 0));
	auto model = std::make_shared<Model>();
	RocketSystem rockets;
	Scene scene;
	rockets.Configure(settings, model, {1, 0, 0});
	Require(rockets.Fire(scene, {}, world, {0, 0, -1}, {10, 0, -100}, InvalidObjectId),
		"Unguided rocket failed to fire");
	rockets.Update(scene, 0.1f);
	Require(std::abs(rockets.GetRockets().front().position.x - 1) < 0.01f &&
			std::abs(rockets.GetRockets().front().position.z + 130) < 0.01f,
		"Rocket did not inherit aircraft velocity");
	const auto axis = XMVector3TransformNormal(
		XMVectorSet(1, 0, 0, 0), XMLoadFloat4x4(&scene.GetObjects().front().worldTransform));
	Require(XMVectorGetZ(axis) < -0.999f, "Rocket model forward was not aligned with velocity");
	const auto before = rockets.GetRockets().front().position;
	rockets.Update(scene, std::numeric_limits<float>::infinity());
	rockets.Update(scene, -1);
	Require(rockets.GetRockets().front().position.z == before.z, "Invalid rocket delta moved rocket");
	const auto steered = RocketSystem::Steer({0, 0, -1200}, {100, 0, 0}, XMConvertToRadians(7.5f));
	Require(std::abs(XMVectorGetX(XMVector3Length(XMLoadFloat3(&steered))) - 1200) < 0.01f && steered.x > 0 &&
			std::abs(std::acos(-steered.z / 1200) - XMConvertToRadians(7.5f)) < 0.0001f,
		"Rocket turn exceeded bound or changed speed");
	Scene guidedScene;
	RocketSystem guided;
	guided.Configure(settings, model, {1, 0, 0});
	guided.Fire(guidedScene, {}, world, {0, 0, -1}, {}, 5);
	std::vector<CombatTarget> targets{
		{5, {500, 100, -1000}, {}, "LOCKED", 100}, {6, {-500, 100, -1000}, {}, "OTHER", 100}};
	guided.Update(guidedScene, 0.25f, {}, targets);
	Require(guided.GetRockets().front().targetId == 5 && guided.GetRockets().front().velocity.x > 0,
		"Locked rocket did not guide toward stored target");
	const auto velocity = guided.GetRockets().front().velocity;
	targets.erase(targets.begin());
	guided.Update(guidedScene, 0.25f, {}, targets);
	Require(guided.GetRockets().front().targetId == InvalidObjectId &&
			guided.GetRockets().front().velocity.x == velocity.x,
		"Lost target was replaced or unguided rocket turned");
	const auto trailCount = [](int frames) {
		RocketSettings config;
		RocketSystem system;
		Scene render;
		XMFLOAT4X4 pose;
		XMStoreFloat4x4(&pose, XMMatrixTranslation(0, 100, 0));
		system.Configure(config, std::make_shared<Model>(), {1, 0, 0});
		system.Fire(render, {}, pose, {0, 0, -1}, {}, InvalidObjectId);
		size_t count = 0;
		for (int i = 0; i < frames; ++i) {
			system.Update(render, 0.25f / frames);
			count += system.TakeTrail().size();
		}
		return count;
	};
	Require(trailCount(1) == trailCount(15), "Rocket smoke density depended on frame rate");
	CollisionTarget box{7, {{-2, -2, -2}, {2, 2, 2}}, {0, 100, -100}, {}};
	XMStoreFloat4x4(&box.world, XMMatrixTranslation(0, 100, -100));
	Scene collisionScene;
	RocketSystem collision;
	collision.Configure(settings, model, {1, 0, 0});
	collision.Fire(collisionScene, {}, world, {0, 0, -1}, {}, InvalidObjectId);
	collision.Update(collisionScene, 0.25f, std::span(&box, 1));
	auto hits = collision.TakeHits();
	Require(hits.size() == 1 && hits.front().target == 7 && hits.front().kind == ProjectileKind::Rocket &&
			std::abs(hits.front().position.z + 98) < 0.01f && collision.GetCount() == 0,
		"Rocket tunneled through target or generated incorrect contact");
	Require(collision.TakeHits().empty(), "Rocket hit repeated after consumption");
	CollisionTarget moving = box;
	moving.previousPosition = {-50, 100, -100};
	XMStoreFloat4x4(&moving.world, XMMatrixTranslation(100, 100, -100));
	Scene movingScene;
	RocketSystem movingRocket;
	movingRocket.Configure(settings, model, {1, 0, 0});
	movingRocket.Fire(movingScene, {}, world, {0, 0, -1}, {}, InvalidObjectId);
	movingRocket.Update(movingScene, 0.25f, std::span(&moving, 1));
	Require(movingRocket.TakeHits().size() == 1, "Rocket missed target crossing its swept path");
	Scene fineScene;
	RocketSystem fine;
	fine.Configure(settings, model, {1, 0, 0});
	fine.Fire(fineScene, {}, world, {0, 0, -1}, {}, 5);
	const std::array<CombatTarget, 1> staticTarget{{{5, {500, 100, -1000}, {}, "LOCKED", 100}}};
	for (int frame = 0; frame < 15; ++frame)
		fine.Update(fineScene, 1.0f / 60, {}, staticTarget);
	// 同じ経過時間の追尾を細かいフレームと低FPSで比較する
	Scene coarseScene;
	RocketSystem coarseRocket;
	coarseRocket.Configure(settings, model, {1, 0, 0});
	coarseRocket.Fire(coarseScene, {}, world, {0, 0, -1}, {}, 5);
	coarseRocket.Update(coarseScene, 0.25f, {}, staticTarget);
	Require(
		std::abs(fine.GetRockets().front().position.x - coarseRocket.GetRockets().front().position.x) < 0.02f,
		"Guided trajectory depended on frame rate");
	Scene seaScene;
	RocketSystem sea;
	sea.Configure(settings, model, {1, 0, 0});
	sea.Fire(seaScene, {}, world, {0, -1, 0}, {}, InvalidObjectId);
	sea.Update(seaScene, 0.25f);
	hits = sea.TakeHits();
	Require(hits.size() == 1 && hits.front().target == InvalidObjectId &&
			std::abs(hits.front().position.y) < 0.01f,
		"Rocket sea contact failed");
	settings.lifetimeSeconds = 0.02f;
	settings.maxRockets = 1;
	Scene limitedScene;
	RocketSystem limited;
	limited.Configure(settings, model, {1, 0, 0});
	Require(limited.Fire(limitedScene, {}, world, {0, 0, -1}, {}, InvalidObjectId) &&
			!limited.Fire(limitedScene, {}, world, {0, 0, -1}, {}, InvalidObjectId),
		"Rocket capacity not enforced");
	limited.Update(limitedScene, 0.25f);
	Require(limited.GetCount() == 0 && limited.TakeHits().empty(),
		"Rocket lifetime caused detonation or retained model");
	std::cout << "PASS: rocket F edges, double tap, guidance, target loss, inherited speed, swept collision, "
				 "smoke cadence, lifetime, sea and cap\n";
}

void TestRocketLauncherAndAssets(const fs::path& repository, const fs::path& fixture) {
	using namespace DirectX;
	const auto assets = repository / L"Assets";
	const auto definition = FlightSceneLoader{}.Load({.environment = assets / L"Environment/environment.json",
		.playerAircraft = assets / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = assets / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = assets / L"Environment/retroFilm.json"});
	const JsonAssetReader reader;
	const auto rocketConfig = assets / L"Aircraft/N1K2JShidenKai/rocketWeapon.json";
	const float expectedExtent = reader.ResolveFloat(
		reader.Resolve(reader.Resolve(rocketConfig, "ammunition"), "modelImport"), "normalizedExtent");
	Require(definition.playerRocket.settings.cooldownSeconds ==
				reader.ResolveFloat(rocketConfig, "cooldownSeconds") &&
			definition.playerRocket.ammunition.options.normalizedExtent == expectedExtent &&
			definition.playerRocket.modelForward.x == 1,
		"Rocket configuration did not load");
	const HWND window = CreateWindowW(L"STATIC", L"Rocket asset test", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
		nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Rocket asset window failed");
	{
		RenderDevice device;
		device.Initialize(window, 64, 64);
		CommandContext context;
		context.Initialize(&device);
		context.BeginFrame();
		auto aircraft = std::make_shared<Model>();
		auto rocket = std::make_shared<Model>();
		Require(aircraft->Load(device.GetDevice(), context.GetCommandList(), &device,
					(fixture / L"muzzles.dae").wstring()),
			"Rocket muzzle fixture failed");
		Require(rocket->Load(device.GetDevice(), context.GetCommandList(), &device,
					definition.playerRocket.ammunition.file.wstring(),
					definition.playerRocket.ammunition.options),
			"Actual rocket model failed to load");
		const auto bounds = rocket->GetLocalBounds();
		Require(bounds && std::abs(bounds->maximum.x - bounds->minimum.x - expectedExtent) < 0.01f &&
				std::abs(bounds->Center().x) < 0.01f,
			"Rocket bounds center or display length incorrect");
		// 挙動の検証には固定条件を使い、実アセットの表示サイズ調整とは切り離す
		RocketDefinition fixtureRocket = definition.playerRocket;
		fixtureRocket.settings = RocketSettings{};
		const CombatSettings fixtureCombat;
		const AircraftFlightParameters fixtureFlight;
		const AircraftAIParameters fixtureAI;
		RocketLauncher launcher;
		launcher.Configure(fixtureRocket.settings, *aircraft);
		Require(launcher.GetNextMuzzle().has_value() && launcher.GetState().nextMuzzle == 0,
			"Launcher initial port was not L1");
		launcher.CommitShot();
		launcher.Update(0.1f);
		Require(launcher.GetNextMuzzle().has_value() && launcher.GetState().nextMuzzle == 1,
			"R1 shared L1 cooldown");
		launcher.CommitShot();
		Require(
			!launcher.GetNextMuzzle() && launcher.GetState().nextMuzzle == 0, "Launcher fired cooling L1");
		launcher.CommitShot();
		Require(launcher.GetState().nextMuzzle == 0, "Failed shot advanced next port");
		for (int i = 0; i < 11; ++i)
			launcher.Update(0.25f);
		launcher.Update(0.15f);
		Require(launcher.GetNextMuzzle().has_value() && launcher.GetState().cooldowns[1] > 0,
			"Independent three second cooldown boundary failed");
		const auto state = launcher.GetState();
		launcher.Update(std::numeric_limits<float>::quiet_NaN());
		Require(launcher.GetState().cooldowns == state.cooldowns, "Invalid delta changed launcher cooldown");
		auto bad = fixtureRocket.settings;
		bad.muzzleNames[0] = "missing";
		bool rejected = false;
		try {
			launcher.Configure(bad, *aircraft);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Missing rocket muzzle accepted");
		// 複数ロケットが同じ敵へ当たっても、撃破通知は一度だけ発生する
		FlightScene scene;
		scene.Initialize(1, 1, 10000);
		scene.SetPlayerAircraftFlightParameters(fixtureFlight);
		scene.ConfigureCombat(fixtureCombat);
		scene.AddPlayerObject(aircraft);
		scene.ConfigurePlayerRocket(fixtureRocket, rocket);
		scene.AddEnemyAircraft(rocket, {0, 15, -160}, 5, fixtureFlight, fixtureAI, 100, "TARGET");
		scene.Update({}, 0.25f, {}, {{}, false, 2});
		Require(scene.GetCombatSnapshot().enemies.empty() && scene.GetCombatSnapshot().hitCount == 1,
			"Rocket direct hit did not destroy standard enemy");
		Require(
			scene.GetEffectSnapshot().explosions.size() <= 2 && !scene.GetEffectSnapshot().explosions.empty(),
			"Rocket impact explosion missing or duplicated per hit");
		scene.RemoveObject(scene.GetObjects().front().id);
		for (int i = 0; i < 33; ++i)
			scene.Update({}, 0.25f, {}, {{}, false, 2});
		Require(scene.GetRocketCount() == 0 && scene.GetEffectSnapshot().rocketExhausts.empty() &&
				scene.GetEffectSnapshot().rocketSmoke.empty(),
			"Removed player kept firing or rocket smoke never expired");
		FlightScene mixed;
		mixed.Initialize(1, 1, 10000);
		mixed.SetPlayerAircraftFlightParameters(fixtureFlight);
		mixed.ConfigureCombat(fixtureCombat);
		mixed.AddPlayerObject(aircraft);
		mixed.ConfigurePlayerWeapon(definition.playerWeapon, aircraft);
		mixed.ConfigurePlayerRocket(fixtureRocket, rocket);
		mixed.AddEnemyAircraft(rocket, {0, 15, -160}, 5, fixtureFlight, fixtureAI, 200, "MIXED");
		mixed.Update({}, 0.25f, {}, {{true}, false, 1});
		Require(mixed.GetCombatSnapshot().enemies.empty() && mixed.GetCombatSnapshot().hitCount > 1 &&
				mixed.GetEffectSnapshot().explosions.size() == 2,
			"Mixed gun/rocket damage or one-time defeat failed");
		FlightScene full;
		full.Initialize(1, 1, 10000);
		full.AddPlayerObject(aircraft);
		auto single = fixtureRocket;
		single.settings.maxRockets = 1;
		full.ConfigurePlayerRocket(single, rocket);
		full.Update({}, 0.01f, {}, {{}, false, 2});
		Require(full.GetRocketCount() == 1 && full.GetCombatSnapshot().rocket.nextMuzzle == 1 &&
				full.GetCombatSnapshot().rocket.cooldowns[1] == 0,
			"Capacity failure consumed R1 cooldown or alternation");
		FlightScene boundary;
		boundary.Initialize(1, 1, 10000);
		boundary.AddPlayerObject(aircraft);
		boundary.ConfigurePlayerRocket(fixtureRocket, rocket);
		boundary.Update({}, 0.01f, {}, {{}, false, 2});
		for (int frame = 0; frame < 20 && boundary.GetCombatSnapshot().rocket.cooldowns[0] > 0.10001f;
			++frame)
			boundary.Update({}, std::min(0.25f, boundary.GetCombatSnapshot().rocket.cooldowns[0] - 0.1f));
		const auto beforeBoundary = boundary.GetRocketCount();
		boundary.Update({}, 0.2f, {}, {{}, false, 1});
		Require(boundary.GetRocketCount() == beforeBoundary + 1 &&
				boundary.GetCombatSnapshot().rocket.nextMuzzle == 1 &&
				boundary.GetCombatSnapshot().rocket.cooldowns[0] == fixtureRocket.settings.cooldownSeconds,
			"F press crossing cooldown end was dropped or new shot lost cooldown time");
		context.EndFrame();
		context.WaitForGpu();
	}
	DestroyWindow(window);
	std::cout << "PASS: rocket asset, model axis/size, alternating launcher, independent cooldown and scene "
				 "damage\n";
}
