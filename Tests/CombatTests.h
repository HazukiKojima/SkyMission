#pragma once
#include "Game/Combat/CombatController.h"
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/TargetingSystem.h"
#include "Game/Combat/ProjectileCollision.h"
#include "Game/UI/CombatHudPresenter.h"
#include "Game/Effects/CombatEffectPresenter.h"
#include <iomanip>

void TestEnemyCombatAI() {
	using namespace DirectX;
	AircraftFlightModel aircraft;
	aircraft.SetPosition({0, 300, 0});
	AircraftAIParameters p;
	const XMFLOAT3 forward{0, 0, -1};
	AircraftAITarget target{{0, 300, -600}, {0, 0, -60}, forward};
	AircraftAIController controller;
	bool fired = false, aimedBeforeShot = false;
	for (int i = 0; i < 400; ++i) {
		const auto decision = controller.ReadCombatInput(aircraft, target, forward, 0.01f, p, 0, true);
		if (i < 299)
			Require(!decision.fire, "Enemy launched during opening grace");
		aimedBeforeShot |= decision.aiming && !decision.fire;
		fired |= decision.fire;
	}
	Require(fired && aimedBeforeShot, "Enemy did not hold its aim before firing");
	const auto interrupted = controller.ReadCombatInput(aircraft, target, forward, 0.01f, p, 0, true, false);
	Require(!interrupted.fire && !interrupted.aiming && interrupted.aimProgress == 0,
		"Cooldown retained gun aim");
	const auto reacquire = controller.ReadCombatInput(aircraft, target, forward, 0.01f, p, 0, true);
	Require(!reacquire.fire && reacquire.aimProgress < 1, "Gun fired without reacquiring aim");
	Require(!controller.ReadCombatInput(aircraft, target, forward, 0.01f, p, 0, false).fire,
		"Reserved enemy kept launching");
	AircraftAIController pursue, disengage, evade, recover;
	const auto settle = [&](AircraftAIController& ai, AircraftAITarget t) {
		AircraftAIDecision result;
		for (int i = 0; i < 10; ++i)
			result = ai.ReadCombatInput(aircraft, t, forward, 0.1f, p, 0, true);
		return result;
	};
	settle(pursue, {{0, 300, -4000}, {}, forward});
	Require(pursue.GetMode() == AircraftCombatMode::Pursuit, "Distant target was not pursued");
	settle(disengage, {{0, 300, -100}, {}, forward});
	Require(
		disengage.GetMode() == AircraftCombatMode::Disengage, "Close target did not trigger disengagement");
	const auto evasive = settle(evade, {{0, 300, 400}, {}, forward});
	Require(evade.GetMode() == AircraftCombatMode::Evade && evasive.flight.highGTurn,
		"Pursuer on tail did not trigger evasive turn");
	// 攻撃状態を長く維持しても、発射成功の通知がなければ離脱しない
	AircraftAIController attack;
	for (int i = 0; i < 120; ++i)
		attack.ReadCombatInput(aircraft, target, forward, 0.1f, p, 0, true, false);
	Require(attack.GetMode() == AircraftCombatMode::Attack, "Attack duration alone caused escape");
	attack.NotifyWeaponFired();
	const auto postShot = attack.ReadCombatInput(aircraft, target, forward, 0.1f, p, 0, true);
	Require(attack.GetMode() == AircraftCombatMode::Disengage && !postShot.aiming && !postShot.fire,
		"Successful launch did not create a counterattack window");
	bool returned = false;
	for (int i = 0; i < 31; ++i) {
		attack.ReadCombatInput(aircraft, target, forward, 0.1f, p, 0, true, false);
		returned |= attack.GetMode() != AircraftCombatMode::Disengage;
	}
	Require(returned, "Post-shot break waited indefinitely for separation distance");
	AircraftAIController cappedBreak;
	auto longManeuver = p;
	longManeuver.maneuverSeconds = 10;
	cappedBreak.NotifyWeaponFired();
	float breakDuration = 0;
	for (int i = 0; i < 320; ++i) {
		cappedBreak.ReadCombatInput(aircraft, target, forward, 0.01f, longManeuver, 0, true, false);
		if (cappedBreak.GetMode() != AircraftCombatMode::Disengage)
			break;
		breakDuration += 0.01f;
	}
	Require(breakDuration >= 2.9f && breakDuration <= p.maximumDisengageSeconds + 0.02f,
		"Maximum disengagement time did not cap a longer maneuver");
	returned = false;
	for (int i = 0; i < 31; ++i) {
		disengage.ReadCombatInput(aircraft, {{0, 300, -100}, {}, forward}, forward, 0.1f, p, 0, true);
		returned |= disengage.GetMode() != AircraftCombatMode::Disengage;
	}
	Require(returned, "Persistent close range kept resetting separation duration");
	AircraftAIController stalled;
	auto reposition = p;
	reposition.threatDistance = 100;
	const AircraftAITarget noseOn{{0, 300, 2000}, {}, forward};
	const auto initialSide = settle(stalled, noseOn).flight.turn;
	AircraftAIDecision changedSide;
	for (int i = 0; i < 125; ++i)
		changedSide = stalled.ReadCombatInput(aircraft, noseOn, forward, 0.1f, reposition, 0, true);
	Require(initialSide * changedSide.flight.turn < 0, "Stalled pursuit did not switch flanking side");
	for (size_t slot = 0; slot < 2; ++slot) {
		AircraftFlightModel rotated;
		rotated.SetPosition({0, 300, 0});
		rotated.SetHeading({-1, 0, 0});
		AircraftAIController baseAI, rotatedAI;
		AircraftAIDecision baseDecision, rotatedDecision;
		for (int i = 0; i < 10; ++i) {
			baseDecision = baseAI.ReadCombatInput(aircraft, noseOn, forward, 0.1f, p, slot, false);
			rotatedDecision = rotatedAI.ReadCombatInput(
				rotated, {{2000, 300, 0}, {}, {-1, 0, 0}}, {-1, 0, 0}, 0.1f, p, slot, false);
		}
		Require(std::abs(baseDecision.flight.turn - rotatedDecision.flight.turn) < 0.001f &&
				std::abs(baseDecision.flight.yaw - rotatedDecision.flight.yaw) < 0.001f,
			"Flanking used world axes instead of the player's heading");
	}
	AircraftAIController tooClose;
	for (int i = 0; i < 100; ++i) {
		const auto decision =
			tooClose.ReadCombatInput(aircraft, {{0, 300, -100}, {}, {0, 0, 1}}, forward, 0.1f, p, 0, true);
		Require(!decision.aiming && !decision.fire, "Enemy attacked inside its minimum separation");
	}
	AircraftFlightParameters fast;
	fast.initialSpeed = 650;
	fast.minimumSpeed = 300;
	fast.maximumSpeed = 1600;
	AircraftFlightModel closing;
	closing.SetFlightParameters(fast);
	closing.SetPosition({0, 300, 0});
	AircraftAIController matching;
	const auto slowing =
		matching.ReadCombatInput(closing, {{0, 300, -800}, {0, 0, -500}, forward}, forward, 0.1f, p, 0, true);
	Require(slowing.flight.throttle < 0 && matching.GetMode() != AircraftCombatMode::Disengage,
		"Tail pursuit did not brake without escaping");
	aircraft.SetPosition({0, 50, 0});
	const auto recovery = settle(recover, target);
	Require(recover.GetMode() == AircraftCombatMode::Recover && recovery.flight.pitch > 0,
		"Low altitude did not trigger climb");
	std::cout << "PASS: enemy pursuit, evasion, separation, recovery, grace and gun aim acquisition\n";
}

void TestAIParameters(const fs::path& repository, const fs::path& fixture) {
	const auto assets = repository / L"Assets";
	const auto enemy = assets / L"Aircraft/F4U4Corsair";
	const auto aiPath = fixture / L"tail-ai.json";
	const auto aircraftPath = fixture / L"tail-aircraft.json";
	const auto text = [](const fs::path& path) {
		std::ifstream stream(path);
		return std::string(std::istreambuf_iterator<char>(stream), {});
	};
	const auto original = text(enemy / L"aiParameters.json");
	std::ofstream aircraft(aircraftPath);
	aircraft << "{\"visualScale\":20,\"model\":" << std::quoted((enemy / L"F4U4Corsair.fbx").generic_string())
			 << ",\"flightParameters\":" << std::quoted((enemy / L"flightParameters.json").generic_string())
			 << ",\"modelImport\":" << std::quoted((enemy / L"modelImport.json").generic_string())
			 << ",\"aiParameters\":" << std::quoted(aiPath.generic_string()) << '}';
	aircraft.close();
	const auto load = [&] {
		return FlightSceneLoader{}.Load({.environment = assets / L"Environment/environment.json",
			.playerAircraft = assets / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
			.enemyAircraft = aircraftPath,
			.retroFilm = assets / L"Environment/retroFilm.json",
			.bomb = assets / L"Ammunition/Bomb/ammunition.json"});
	};
	std::ofstream(aiPath) << original;
	const auto valid = load().enemyAIParameters;
	Require(valid.tailDistance == 4500 && valid.disengageDistance == 3000 && valid.flankDistance == 2000 &&
			valid.pursuitPredictionSeconds == 1.5f && valid.maximumDisengageSeconds == 3 &&
			valid.repositionSeconds == 12,
		"Generated C#/C++ layout did not load the tail-pursuit fields");
	const auto reject = [&](const std::string& field, const std::string& value) {
		auto invalid = original;
		const auto key = invalid.find('"' + field + '"');
		Require(key != std::string::npos, "AI fixture field is missing");
		const auto begin = invalid.find(':', key) + 1;
		const auto end = invalid.find_first_of(",}", begin);
		invalid.replace(begin, end - begin, value);
		std::ofstream(aiPath) << invalid;
		bool rejected = false;
		try {
			load();
		} catch (const std::runtime_error&) {
			rejected = true;
		}
		Require(rejected, "Invalid tail-pursuit setting was accepted");
	};
	for (const std::string field : {"tailDistance", "flankDistance", "pursuitPredictionSeconds",
			 "maximumDisengageSeconds", "repositionSeconds"})
		for (const std::string value : {"0", "-1", "1e39"})
			reject(field, value);
	reject("disengageDistance", "4500");
	reject("tailDistance", "6000");
	reject("reengageDistance", "9000");
	std::cout << "PASS: tail-pursuit JSON/layout, positive finite fields and distance ordering\n";
}

void TestCombatMath() {
	using namespace DirectX;
	RawInputState raw;
	raw.SetActive(true);
	RAWKEYBOARD key{};
	key.VKey = VK_TAB;
	key.MakeCode = 0x0f;
	const auto keyboard = reinterpret_cast<HANDLE>(std::uintptr_t(17));
	raw.ProcessKeyboard(keyboard, key);
	key.Flags = RI_KEY_BREAK;
	raw.ProcessKeyboard(keyboard, key);
	Require(CombatController{}.ReadInput(raw.ConsumeFrame()).nextTarget, "Short Tab press was lost");
	Require(!CombatController{}.ReadInput(raw.ConsumeFrame()).nextTarget, "Tab edge was consumed twice");
	key.Flags = RI_KEY_MAKE;
	raw.ProcessKeyboard(keyboard, key);
	raw.ConsumeFrame();
	raw.ProcessKeyboard(keyboard, key);
	Require(!raw.ConsumeFrame().WasKeyPressed(VK_TAB), "Tab autorepeat produced another edge");
	key.Flags = RI_KEY_BREAK;
	raw.ProcessKeyboard(keyboard, key);
	key.Flags = RI_KEY_MAKE;
	raw.ProcessKeyboard(keyboard, key);
	raw.SetActive(false);
	raw.SetActive(true);
	Require(!raw.ConsumeFrame().WasKeyPressed(VK_TAB), "Focus regain restored a Tab edge");

	CombatTarget target{1, {0, 0, -900}, {}, "ENEMY 01", 100};
	const auto aim = SolveGunAim({}, {}, target, 900, 3);
	Require(aim && std::abs(aim->seconds - 1) < 0.001f && aim->direction.z < -0.999f,
		"Stationary intercept failed");
	target.velocity = {100, 0, 0};
	const auto lateral = SolveGunAim({}, {}, target, 900, 3);
	Require(lateral && lateral->direction.x > 0.1f, "Lateral target did not require lead");
	const auto inherited = SolveGunAim({}, {100, 0, 0}, target, 900, 3);
	Require(inherited && std::abs(inherited->direction.x) < 0.001f, "Shooter velocity was not subtracted");
	target.velocity = {0, 0, 900};
	const auto linear = SolveGunAim({}, {}, target, 900, 3);
	Require(linear && std::abs(linear->seconds - 0.5f) < 0.001f, "Linear intercept case failed");
	target.velocity = {0, 0, -1800};
	Require(!SolveGunAim({}, {}, target, 900, 3), "Unreachable target had an intercept");
	target.velocity = {};
	Require(!SolveGunAim({}, {}, target, 900, 0.5f), "Intercept beyond lifetime was accepted");
	Require(IsWithinAngle({0, 0, -1}, {0.02f, 0, -1}, 3) && !IsWithinAngle({0, 0, -1}, {0.1f, 0, -1}, 3),
		"Assist angle boundary failed");

	std::vector<CombatTarget> targets{
		{3, {200, 0, -900}, {}, "3", 100}, {1, {0, 0, -900}, {}, "1", 100}, {2, {0, 0, 900}, {}, "2", 100}};
	TargetingSystem targeting;
	targeting.Configure({});
	targeting.Update(targets, {}, {0, 0, -1}, false, 0.25f);
	Require(targeting.GetSelected() == 1 && targeting.GetState() == LockState::Acquiring,
		"Initial automatic selection failed");
	targeting.Update(targets, {}, {0, 0, -1}, false, 0.25f);
	Require(targeting.GetState() == LockState::Locked, "Acquire timer did not lock");
	targeting.Update(targets, {}, {0, 0, -1}, true, 0);
	Require(targeting.GetSelected() == 2 && targeting.GetState() == LockState::Selected,
		"Tab did not select rear target");
	targeting.Update(targets, {}, {0, 0, -1}, true, 0);
	Require(targeting.GetSelected() == 3, "Tab did not follow stable IDs");
	targeting.Update(targets, {}, {0, 0, -1}, true, 0);
	Require(targeting.GetSelected() == 1, "Tab did not wrap");
	targeting.Update(targets, {}, {0, 0, -1}, false, 0.25f);
	targeting.Update(targets, {}, {0, 0, -1}, false, 0.25f);
	targets[1].center = {0, 0, 900};
	targeting.Update(targets, {}, {0, 0, -1}, false, 0.1f);
	Require(targeting.GetState() == LockState::Locked, "Lock grace was skipped");
	targeting.Update(targets, {}, {0, 0, -1}, false, 0.15f);
	Require(targeting.GetState() == LockState::Selected, "Lock grace did not expire");
	targets.erase(targets.begin() + 1);
	targeting.Update(targets, {}, {0, 0, -1}, false, 0);
	Require(targeting.GetSelected() == 3, "Destroyed selected target was not replaced");
	targeting.Update({}, {}, {0, 0, -1}, false, 0);
	Require(targeting.GetState() == LockState::None, "Empty target list retained selection");

	CollisionTarget box{4, {{-1, -1, -1}, {1, 1, 1}}, {0, 10, -50}, {}};
	XMStoreFloat4x4(&box.world, XMMatrixRotationY(0.7f) * XMMatrixTranslation(0, 10, -50));
	Require(IntersectMovingTarget({0, 10, 0}, {0, 10, -100}, box, 0, 1).has_value(),
		"Fast projectile missed rotated OBB");
	Require(!IntersectMovingTarget({20, 10, 0}, {20, 10, -100}, box, 0, 1), "OBB accepted a distant miss");
	box.previousPosition = {-10, 10, -50};
	XMStoreFloat4x4(&box.world, XMMatrixTranslation(10, 10, -50));
	Require(IntersectMovingTarget({0, 10, 0}, {0, 10, -100}, box, 0, 1).has_value(),
		"Moving target crossing was missed");
	std::cout << "PASS: Tab edges, targeting/lock/grace, intercepts, assist cone and moving OBB collision\n";
}

void TestCombatDamage(const fs::path& repository, const fs::path& fixture) {
	using namespace DirectX;
	const auto assets = repository / L"Assets";
	const auto definition = FlightSceneLoader{}.Load({.environment = assets / L"Environment/environment.json",
		.playerAircraft = assets / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = assets / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = assets / L"Environment/retroFilm.json",
		.bomb = assets / L"Ammunition/Bomb/ammunition.json"});
	const auto cubePath = fixture / L"combat-box.obj";
	std::ofstream(cubePath)
		<< "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
		   "f 1 2 3\nf 1 3 4\nf 5 7 6\nf 5 8 7\nf 1 5 6\nf 1 6 2\nf 4 3 7\nf 4 7 8\n";
	const auto window = CreateWindowW(L"STATIC", L"Combat test (hidden)", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
		nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Combat test window failed");
	{
		RenderDevice device;
		device.Initialize(window, 64, 64);
		CommandContext context;
		context.Initialize(&device);
		context.BeginFrame();
		auto player = std::make_shared<Model>();
		auto enemy = std::make_shared<Model>();
		Require(player->Load(device.GetDevice(), context.GetCommandList(), &device,
					(fixture / L"muzzles.dae").wstring()),
			"Combat player fixture failed");
		ModelLoadOptions centered;
		centered.origin = ModelOrigin::BoundsCenter;
		Require(
			enemy->Load(device.GetDevice(), context.GetCommandList(), &device, cubePath.wstring(), centered),
			"Combat enemy fixture failed");
		Require(enemy->GetLocalBounds() && std::abs(enemy->GetLocalBounds()->maximum.z - 5) < 0.001f,
			"Normalized model bounds were incorrect");
		{
			FlightScene scene;
			scene.Initialize(1, 1, 10000);
			auto combat = definition.combat;
			combat.bulletDamage = 100;
			scene.ConfigureCombat(combat);
			scene.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
			const auto playerId = scene.AddPlayerObject(player);
			scene.ConfigurePlayerWeapon(definition.playerWeapon, player);
			scene.AddEnemyAircraft(enemy, {0, 15, -160}, 5, definition.enemyAircraft.flightParameters,
				definition.enemyAIParameters, 100, "PATROL");
			scene.Update({}, 0.25f, {}, {{true}});
			const auto& state = scene.GetCombatSnapshot();
			Require(state.enemies.empty() && state.hitCount == 1 && state.destroyedSeconds > 0,
				"Lethal cannon volley did not produce one kill");
			Require(state.result == CombatResult::Active && state.missionPhase == MissionPhase::CarrierAttack,
				"Patrol all-down incorrectly produced victory");
			Require(scene.GetEffectSnapshot().explosions.size() == 1,
				"Multiple simultaneous bullets created duplicate death effects");
			scene.RemoveObject(playerId);
			scene.Update({}, 0.01f);
			Require(state.result == CombatResult::Defeat, "Player removal did not produce defeat");
			CombatInput retry;
			retry.retry = true;
			scene.Update({}, 0.01f, {}, retry);
			Require(state.result == CombatResult::Active && state.enemies.size() == 1 &&
					scene.GetProjectileCount() == 0 && scene.GetBombCount() == 0 &&
					state.playerHealth == combat.playerHealth,
				"Retry retained damage or weapons");
		}

		GunSettings gun = definition.playerWeapon.gun;
		gun.muzzleSpeed = 900;
		std::array<XMFLOAT3, 4> muzzles{};
		XMFLOAT4X4 world, pose;
		XMStoreFloat4x4(&world, XMMatrixTranslation(0, 20, 0));
		pose = world;
		CollisionTarget nearBox{1, *enemy->GetLocalBounds(), {0, 20, -100}, {}},
			farBox{2, *enemy->GetLocalBounds(), {0, 20, -160}, {}};
		XMStoreFloat4x4(&nearBox.world, XMMatrixTranslation(0, 20, -100));
		XMStoreFloat4x4(&farBox.world, XMMatrixTranslation(0, 20, -160));
		const std::array boxes{farBox, nearBox};
		ProjectileSystem bullets;
		bullets.Configure(gun, player, {0, 1, 0});
		Scene scene;
		bullets.FireVolley(scene, muzzles, world, pose, {0, 0, -1}, {}, 0.25f, {}, boxes, 0.25f);
		const auto hits = bullets.TakeHits();
		Require(hits.size() == 4 &&
				std::all_of(hits.begin(), hits.end(),
					[](const HitEvent& hit) {
						return hit.target == 1;
					}),
			"Projectile did not hit nearest enemy once");
		Require(std::all_of(hits.begin(), hits.end(),
					[](const HitEvent& hit) {
						return std::abs(hit.position.z + 95) < 0.01f &&
							std::abs(hit.position.y - 20) < 0.01f && hit.direction.z < -0.999f;
					}),
			"Impact position did not match first contact");
		Require(bullets.GetCount() == 0 && bullets.TakeHits().empty(),
			"Consumed projectile generated duplicate hit");
		gun.lifetime = 0.05f;
		bullets.Configure(gun, player, {0, 1, 0});
		bullets.FireVolley(scene, muzzles, world, pose, {0, 0, -1}, {}, 0.25f, {}, boxes, 0.25f);
		Require(bullets.TakeHits().empty(), "Expired projectile hit beyond its lifetime");
		gun.lifetime = 3;
		bullets.Configure(gun, player, {0, 1, 0});
		AimAssistance assistance{CombatTarget{3, {30, 20, -1000}, {}, "TEST", 100}, 3};
		bullets.FireVolley(scene, muzzles, world, pose, {0, 0, -1}, {}, 0, assistance);
		bullets.Update(scene, 0.1f);
		Require(scene.GetObjects().front().worldTransform._41 > 1,
			"Locked in-cone target did not adjust projectile direction");
		const auto fixedDirection = scene.GetObjects().front().worldTransform._41;
		bullets.Update(scene, 0.1f);
		Require(std::abs(scene.GetObjects().front().worldTransform._41 - 2 * fixedDirection) < 0.001f,
			"Projectile homed after firing");
		context.EndFrame();
		context.WaitForGpu();
	}
	DestroyWindow(window);
	std::cout << "PASS: model bounds, damage/defeat/target invalidation, nearest collision, lifetime and "
				 "straight assisted fire\n";
}

void TestHudRendering(const fs::path& fixture) {
	using namespace DirectX;
	const HWND window = CreateWindowW(L"STATIC", L"HUD regression (hidden)", WS_OVERLAPPEDWINDOW, 0, 0, 64,
		64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "HUD test window failed");
	{
		RenderDevice device;
		device.Initialize(window, 800, 600);
		CommandContext context;
		context.Initialize(&device);
		HudRenderer renderer;
		context.BeginFrame();
		renderer.Initialize(device, context.GetCommandList());
		context.EndFrame();
		context.WaitForGpu();
		renderer.ReleaseUploadResources();
		for (const auto size : {XMFLOAT2{800, 600}, XMFLOAT2{1920, 1080}, XMFLOAT2{2560, 1080}}) {
			const UINT width = UINT(size.x);
			const UINT height = UINT(size.y);
			context.WaitForGpu();
			device.Resize(width, height);
			context.SynchronizeFrameIndex(device);
			Engine::RenderView view;
			view.width = width;
			view.height = height;
			XMStoreFloat4x4(&view.view, XMMatrixIdentity());
			XMStoreFloat4x4(
				&view.projection, XMMatrixPerspectiveFovLH(XM_PIDIV4, float(width) / height, 1, 10000));
			CombatSnapshot snapshot;
			snapshot.playerSpeedMetersPerSecond = 100;
			snapshot.playerAltitudeMeters = 1542.6f;
			snapshot.enemies = {{1, {0, 30, 1000}, {}, "ENEMY 01", 100, {0, 0, 1}},
				{2, {-2500, 50, 4000}, {}, "ENEMY 02", 100, {0.7f, 0, -0.7f}},
				{3, {15000, 0, 1000}, {}, "ENEMY 03", 100, {-1, 0, 0}},
				{4, {4500, 400, -3000}, {}, "ENEMY 04", 100, {-0.7f, 0, 0.7f}}};
			snapshot.selected = 1;
			snapshot.lockState = LockState::Locked;
			snapshot.forward = {0, 0, 1};
			snapshot.playerHeading = {0, 0, 1};
			snapshot.gunImpactPoint = XMFLOAT3{20, 10, 1000};
			snapshot.aim = AimSolution{{0.05f, 0, 1}, {60, 50, 1100}, 1};
			snapshot.assistReady = true;
			snapshot.hitSeconds = 0.15f;
			Engine::HudDrawList list;
			CombatEffectSnapshot effects;
			const auto buildFrame = [&]() {
				list.Reset(view.width, view.height);
				CombatEffectPresenter{}.Build(effects, view, list);
				CombatHudPresenter{}.Build(snapshot, view, list);
			};
			buildFrame();
			const auto requireFlightText = [&](float right, std::string_view text) {
				const float scale = std::min(float(width) / 1920, float(height) / 1080);
				const float offsetX = (width - 1920 * scale) * 0.5f;
				const float offsetY = (height - 1080 * scale) * 0.5f;
				const float textWidth = Engine::HudDrawList::MeasureTextWidth(text, 40);
				Engine::HudDrawList expected;
				expected.Reset(width, height);
				expected.Text({offsetX + (right - textWidth) * scale, offsetY + 432 * scale}, text,
					40 * scale, Engine::HudSettings{}.color);
				const auto sameVertex = [](const Engine::HudVertex& a, const Engine::HudVertex& b) {
					return std::abs(a.position.x - b.position.x) < 0.00001f &&
						std::abs(a.position.y - b.position.y) < 0.00001f && a.uv.x == b.uv.x &&
						a.uv.y == b.uv.y;
				};
				const auto& vertices = list.GetVertices();
				Require(std::search(vertices.begin(), vertices.end(), expected.GetVertices().begin(),
							expected.GetVertices().end(), sameVertex) != vertices.end(),
					"Flight HUD conversion, rounding or right alignment failed");
			};
			requireFlightText(312, "360");
			requireFlightText(1808, "1543");
			snapshot.playerSpeedMetersPerSecond = 0;
			buildFrame();
			requireFlightText(312, "0");
			snapshot.playerSpeedMetersPerSecond = 100;
			buildFrame();
			Require(
				!list.GetVertices().empty() && list.GetVertices().size() < Engine::HudDrawList::MaxVertices,
				"HUD mesh was empty or overflowed");
			Require(ProjectHudPoint({0, 0, 100}, view).visible && ProjectHudPoint({0, 0, -100}, view).behind,
				"HUD projection front/rear failed");
			const auto baseVertices = list.GetVertices().size();
			for (float age : {0.0f, 0.2f, 0.9f, 2.0f, 3.4f}) {
				effects.explosions = {{{0, 0, 250}, {}, age, 22}};
				buildFrame();
				Require(list.GetVertices().size() > baseVertices, "Explosion phase was invisible");
			}
			effects.explosions = {{{0, 0, -250}, {}, 0.2f, 22}};
			buildFrame();
			Require(list.GetVertices().size() == baseVertices, "Rear explosion was drawn");
			effects.explosions.assign(4, {{0, 0, 250}, {}, 0.08f, 22});
			effects.impacts.assign(64, {{0, 0, 250}, 0});
			effects.gunSmoke.assign(GunEffectSettings::maxSmokeCount, {{0, 0, 100}, {20, 0, 0}, 0.1f});
			effects.gunTracers.assign(GunEffectSettings::maxTracerCount, {{30, 10, 250}, {10, 0, 200}});
			for (size_t muzzle = 0; muzzle < 4; ++muzzle)
				effects.gunFlashes.push_back({muzzle, {float(muzzle) * 5, 0, 100}, {0, 0, 1}, 0.02f, muzzle});
			list.Reset(view.width, view.height);
			CombatEffectPresenter{}.Build(effects, view, list);
			Require(list.GetVertices().size() <=
					Engine::HudDrawList::MaxVertices - EffectDrawSettings::hudReservedVertices,
				"Combined effects consumed reserved HUD vertices");
			const auto crowdedVertices = list.GetVertices().size();
			auto withoutSmoke = effects;
			withoutSmoke.gunSmoke.clear();
			list.Reset(view.width, view.height);
			CombatEffectPresenter{}.Build(withoutSmoke, view, list);
			Require(list.GetVertices().size() == crowdedVertices,
				"Crowded effects did not omit optional gun smoke first");
			buildFrame();
			Require(list.GetVertices().size() < Engine::HudDrawList::MaxVertices,
				"Combined effects overflowed HUD");
			effects.explosions.clear();
			effects.impacts.clear();
			effects.gunSmoke.clear();
			effects.gunTracers.clear();
			effects.gunFlashes.clear();
			for (float age : {0.0f, 0.04f, 0.2f, 0.42f}) {
				effects.impacts = {{{0, 0, 100}, age}};
				buildFrame();
				Require(list.GetVertices().size() > baseVertices, "Impact phase was invisible");
			}
			effects.impacts = {{{0, 0, 100}, 0.08f}};
			buildFrame();
			Require(list.GetVertices().size() > baseVertices, "Visible impact did not produce sparks");
			effects.impacts.front().position.z = -100;
			buildFrame();
			Require(list.GetVertices().size() == baseVertices, "Rear impact produced sparks");
			effects.impacts.front().position.z = 100;
			snapshot.selected = 3;
			buildFrame();
			for (const auto& vertex : list.GetVertices())
				Require(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y),
					"HUD projection generated NaN");
			effects.explosions = {{{-25, 0, 250}, {}, 0.2f, 22}};
			snapshot.selected = 1;
			// 両解像度の読み戻し画像に発射演出を含め、照準との重なりを確認する
			for (size_t muzzle = 0; muzzle < 4; ++muzzle) {
				const float x = -18 + float(muzzle) * 12;
				effects.gunFlashes.push_back({muzzle, {x, -10, 90}, {0, 0, 1}, 0.01f, muzzle});
				effects.gunSmoke.push_back({{x, -10, 90}, {20, 0, 0}, 0.15f});
				effects.gunTracers.push_back({{x + 3, -5, 170}, {x, -10, 100}});
			}
			buildFrame();

			buildFrame();
			const auto saveFrame = [&](std::string_view suffix) {
				context.BeginFrame();
				auto* commands = context.GetCommandList();
				auto* target = device.GetCurrentRenderTarget();
				context.TransitionResource(
					target, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
				const auto rtv = device.GetCurrentRtvHandle();
				const float background[] = {0.015f, 0.025f, 0.035f, 1};
				commands->ClearRenderTargetView(rtv, background, 0, nullptr);
				commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
				const D3D12_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
				const D3D12_RECT scissor{0, 0, LONG(width), LONG(height)};
				commands->RSSetViewports(1, &viewport);
				commands->RSSetScissorRects(1, &scissor);
				renderer.Record(device, commands, list);
				context.TransitionResource(
					target, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
				UINT64 bytes = 0;
				const auto desc = target->GetDesc();
				device.GetDevice()->GetCopyableFootprints(
					&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
				ComPtr<ID3D12Resource> readback;
				const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
				const auto buffer = CD3DX12_RESOURCE_DESC::Buffer(bytes);
				ThrowIfFailed(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
					&buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
				const CD3DX12_TEXTURE_COPY_LOCATION source(target, 0), destination(readback.Get(), footprint);
				commands->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
				context.TransitionResource(
					target, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PRESENT);
				context.EndFrame();
				context.WaitForGpu();
				void* mapped = nullptr;
				const CD3DX12_RANGE range(0, SIZE_T(bytes));
				ThrowIfFailed(readback->Map(0, &range, &mapped));
				ScratchImage image;
				ThrowIfFailed(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1));
				const auto* pixels = image.GetImage(0, 0, 0);
				size_t parchment = 0, englishInk = 0;
				const float scale = std::min(float(width) / 1920, float(height) / 1080);
				const float offsetX = (width - 1920 * scale) * 0.5f;
				const float offsetY = (height - 1080 * scale) * 0.5f;
				for (UINT y = 0; y < height; ++y) {
					const auto* row = static_cast<const uint8_t*>(mapped) + footprint.Offset +
						size_t(y) * footprint.Footprint.RowPitch;
					std::memcpy(pixels->pixels + y * pixels->rowPitch, row, size_t(width) * 4);
					for (UINT x = 0; x < width; ++x)
						if (row[x * 4] > 80 && row[x * 4 + 1] > 60 && row[x * 4 + 2] > 40 &&
							row[x * 4] >= row[x * 4 + 1] && row[x * 4 + 1] >= row[x * 4 + 2]) {
							++parchment;
							if (x > offsetX + 100 * scale && x < offsetX + 215 * scale &&
								y > offsetY + 372 * scale && y < offsetY + 414 * scale)
								++englishInk;
						}
				}
				const CD3DX12_RANGE noWrites(0, 0);
				readback->Unmap(0, &noWrites);
				Require(parchment > 100, "HUD did not produce parchment-colored pixels");
				Require(englishInk > 10, "English SPEED label did not produce visible glyph pixels");
				const auto path = fixture / ("hud-" + std::to_string(width) + std::string(suffix) + ".png");
				ThrowIfFailed(SaveToWICFile(*pixels, WIC_FLAGS_NONE, GUID_ContainerFormatPng, path.c_str()));
			};
			saveFrame("");
			buildFrame();
			saveFrame("-lock");
			snapshot.lockState = LockState::Selected;
			snapshot.hitSeconds = 0;
			buildFrame();
			saveFrame("-normal");
			snapshot.result = CombatResult::Victory;
			snapshot.allDownSeconds = 3;
			buildFrame();
			saveFrame("-victory");
			snapshot.result = CombatResult::Defeat;
			snapshot.allDownSeconds = 0;
			snapshot.playerHealth = 0;
			snapshot.playerHitSeconds = 0.5f;
			buildFrame();
			saveFrame("-defeat");
			snapshot.result = CombatResult::Active;
			snapshot.playerHealth = 100;
			snapshot.playerHitSeconds = 0;
			snapshot.missionMessageSeconds = 6;
			snapshot.missionPhase = MissionPhase::Patrol;
			buildFrame();
			saveFrame("-mission-patrol");
			snapshot.missionPhase = MissionPhase::CarrierAttack;
			snapshot.enemies.clear();
			snapshot.objectivePosition = {180, -100, 1500};
			snapshot.bombImpactPoint = XMFLOAT3{100, -100, 1500};
			snapshot.bombCooldown = 4.2f;
			buildFrame();
			Require(list.GetVertices().size() < Engine::HudDrawList::MaxVertices,
				"Mission objective and bomb HUD overflowed");
			saveFrame("-mission-bombing");
			snapshot.missionPhase = MissionPhase::Commander;
			snapshot.deckDisabled = true;
			snapshot.enemies = {{1, {0, 30, 1000}, {}, "SQUADRON LEADER", 180}};
			buildFrame();
			saveFrame("-mission-commander");
		}
	}
	DestroyWindow(window);
	std::cout << "PASS: native HUD/radar, projection, GPU readback, 800x600/1920x1080/2560x1080 and resize\n";
}
