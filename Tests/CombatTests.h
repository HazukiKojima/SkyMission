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
	bool fired = false, warnedBeforeLaunch = false;
	for (int i = 0; i < 400; ++i) {
		const auto decision = controller.ReadCombatInput(aircraft, target, forward, 0.01f, p, 0, true);
		if (i < 299)
			Require(!decision.fire, "Enemy launched during opening grace");
		warnedBeforeLaunch |= decision.locking && !decision.fire;
		fired |= decision.fire;
	}
	Require(fired && warnedBeforeLaunch, "Enemy missile did not warn before lock completion");
	const auto interrupted = controller.ReadCombatInput(aircraft, target, forward, 0.01f, p, 0, true, false);
	Require(!interrupted.fire && !interrupted.locking && interrupted.lockProgress == 0,
		"Cooldown retained missile lock");
	const auto reacquire = controller.ReadCombatInput(aircraft, target, forward, 0.01f, p, 0, true);
	Require(!reacquire.fire && reacquire.lockProgress < 1, "Missile reacquired instantly after cooldown");
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
	Require(attack.GetMode() == AircraftCombatMode::Disengage && !postShot.locking && !postShot.fire,
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
		Require(!decision.locking && !decision.fire, "Enemy attacked inside its minimum separation");
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
	std::cout << "PASS: enemy pursuit, evasion, separation, recovery, grace and missile lock acquisition\n";
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
			 << ",\"rocketWeapon\":" << std::quoted((enemy / L"rocketWeapon.json").generic_string())
			 << ",\"aiParameters\":" << std::quoted(aiPath.generic_string()) << '}';
	aircraft.close();
	const auto load = [&] {
		return FlightSceneLoader{}.Load({.environment = assets / L"Environment/environment.json",
			.playerAircraft = assets / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
			.enemyAircraft = aircraftPath,
			.retroFilm = assets / L"Environment/retroFilm.json"});
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

void TestMissileEdgeIndicators() {
	using namespace DirectX;
	for (const XMFLOAT2 size : {XMFLOAT2{800, 600}, XMFLOAT2{1920, 1080}})
		for (float yaw : {0.0f, XM_PIDIV2}) {
			Engine::RenderView view;
			view.width = static_cast<unsigned>(size.x);
			view.height = static_cast<unsigned>(size.y);
			XMStoreFloat4x4(&view.view, XMMatrixRotationY(yaw));
			XMStoreFloat4x4(&view.projection, XMMatrixPerspectiveFovLH(XM_PIDIV4, size.x / size.y, 1, 10000));
			const auto cameraWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view.view));
			const auto world = [&](XMFLOAT3 local) {
				XMFLOAT3 result;
				XMStoreFloat3(&result, XMVector3TransformCoord(XMLoadFloat3(&local), cameraWorld));
				return result;
			};
			CombatSnapshot missile;
			missile.threatWarning = ThreatWarning::Missile;
			Engine::HudDrawList empty;
			empty.Reset(view.width, view.height);
			CombatHudPresenter{}.Build(missile, view, empty);
			const size_t emptyCount = empty.GetVertices().size();
			missile.incomingMissiles = {{77, world({0, 0, 100}), 100}};
			Engine::HudDrawList list;
			list.Reset(view.width, view.height);
			CombatHudPresenter{}.Build(missile, view, list);
			const size_t visibleCount = list.GetVertices().size();
			Require(visibleCount > emptyCount + 12, "Visible missile was not circled");
			for (const XMFLOAT3 local : {XMFLOAT3{5000, 0, 100}, XMFLOAT3{-5000, 0, 100},
					 XMFLOAT3{0, 5000, 100}, XMFLOAT3{0, -5000, 100}, XMFLOAT3{0, 0, -100}}) {
				const auto position = world(local);
				missile.incomingMissiles.front().position = position;
				list.Reset(view.width, view.height);
				CombatHudPresenter{}.Build(missile, view, list);
				Require(list.GetVertices().size() == emptyCount + 12 + 24,
					"Offscreen missile did not add one edge arrow and one radar marker");
				const auto& vertices = list.GetVertices();
				CombatSnapshot enemy;
				enemy.selected = 1;
				enemy.enemies = {{1, position, {}, "ENEMY", 100}};
				Engine::HudDrawList enemyList;
				enemyList.Reset(view.width, view.height);
				CombatHudPresenter{}.Build(enemy, view, enemyList);
				bool same = false;
				for (size_t first = 0; first + 12 <= enemyList.GetVertices().size(); ++first) {
					bool matches = true;
					for (size_t n = 0; n < 12; ++n) {
						const auto a = vertices[vertices.size() - 12 + n].position,
								   b = enemyList.GetVertices()[first + n].position;
						matches &= std::abs(a.x - b.x) < 0.01f && std::abs(a.y - b.y) < 0.01f;
					}
					same |= matches;
				}
				Require(same, "Enemy and missile edge indicators used different placement or shape");
				CombatSnapshot lock;
				lock.threatWarning = ThreatWarning::LockOn;
				lock.lockingEnemyCount = 1;
				lock.enemyLockProgress = 0.5f;
				lock.lockingEnemyPositions = {world({0, 0, 100})};
				Engine::HudDrawList yellow;
				yellow.Reset(view.width, view.height);
				CombatHudPresenter{}.Build(lock, view, yellow);
				const auto yellowVisibleCount = yellow.GetVertices().size();
				lock.lockingEnemyPositions = {position};
				yellow.Reset(view.width, view.height);
				CombatHudPresenter{}.Build(lock, view, yellow);
				Require(yellow.GetVertices().size() == yellowVisibleCount + 12,
					"Yellow lock arrow was not restricted to screen edges");
				for (size_t n = 0; n < 12; ++n) {
					const auto redVertex = vertices[vertices.size() - 12 + n];
					const auto yellowVertex = yellow.GetVertices()[yellow.GetVertices().size() - 12 + n];
					Require(std::abs(redVertex.position.x - yellowVertex.position.x) < 0.01f &&
							std::abs(redVertex.position.y - yellowVertex.position.y) < 0.01f,
						"Yellow/red warning arrows used different placement");
				}
			}
			missile.incomingMissiles.front().position = world({0, 0, 100});
			list.Reset(view.width, view.height);
			CombatHudPresenter{}.Build(missile, view, list);
			Require(list.GetVertices().size() == visibleCount,
				"Visible missile did not switch from edge arrow to circle");
		}
	std::cout << "PASS: identical enemy/lock/missile edge arrows, front/rear/sides, camera rotation and "
				 "visible circle "
				 "suppression\n";
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
		.retroFilm = assets / L"Environment/retroFilm.json"});
	// 実機の飛行モデルを使い、長距離離脱ではなく背後の位置取りと攻撃復帰を測定する
	const auto simulatePursuit = [&](DirectX::XMFLOAT3 start, size_t slot, bool turning,
									 bool physicalPlayer) {
		const float battleSpeed = definition.playerAircraft.flightParameters.initialSpeed;
		AircraftFlightModel fighter;
		fighter.SetFlightParameters(definition.enemyAircraft.flightParameters);
		fighter.SetPosition(start);
		fighter.SetHeading({-start.x, 1500 - start.y, -start.z});
		AircraftAIController controller;
		AircraftFlightModel player;
		player.SetFlightParameters(definition.playerAircraft.flightParameters);
		player.SetPosition({0, 1500, 0});
		float firstRear = -1, firstLock = -1, rearSeconds = 0, distantSeconds = 0;
		float bandSeconds = 0, closeSeconds = 0;
		float breakSeconds = 0, maximumBreak = 0, cooldown = 0, closest = 100000;
		unsigned shots = 0, rearEntries = 0;
		bool wasRear = false;
		for (int frame = 0; frame < 7200; ++frame) {
			const float seconds = frame / 60.0f;
			const float angle = turning ? seconds * 0.22f : 0;
			AircraftAITarget target;
			target.position = turning ? XMFLOAT3{battleSpeed / 0.22f * (1 - std::cos(angle)), 1500,
											-battleSpeed / 0.22f * std::sin(angle)}
									  : XMFLOAT3{0, 1500, -battleSpeed * seconds};
			target.velocity = {battleSpeed * std::sin(angle), 0, -battleSpeed * std::cos(angle)};
			target.forward = {std::sin(angle), 0, -std::cos(angle)};
			if (physicalPlayer)
				target = {player.GetPosition(), player.GetVelocity(), player.GetForward()};
			const auto position = fighter.GetPosition();
			const auto delta = XMLoadFloat3(&target.position) - XMLoadFloat3(&position);
			const float distance = XMVectorGetX(XMVector3Length(delta));
			XMFLOAT3 direction;
			XMStoreFloat3(&direction, XMVector3Normalize(delta));
			cooldown = std::max(0.0f, cooldown - 1 / 60.0f);
			const bool allowed = slot % 3 != static_cast<size_t>(seconds / 6) % 3;
			const auto decision = controller.ReadCombatInput(fighter, target, direction, 1 / 60.0f,
				definition.enemyAIParameters, slot, allowed, cooldown <= 0);
			const bool rear = distance <= definition.enemyAIParameters.attackDistance &&
				XMVectorGetX(XMVector3Dot(XMLoadFloat3(&direction), XMLoadFloat3(&target.forward))) >= 0.5f;
			if (rear) {
				if (firstRear < 0)
					firstRear = seconds;
				rearSeconds += 1 / 60.0f;
				if (!wasRear)
					++rearEntries;
			}
			wasRear = rear;
			if (decision.locking) {
				Require(distance >= definition.enemyAIParameters.disengageDistance &&
						distance <= definition.enemyAIParameters.attackDistance && allowed && cooldown <= 0,
					"Enemy locked outside its medium-range attack window");
				if (firstLock < 0)
					firstLock = seconds;
			}
			if (decision.fire) {
				++shots;
				cooldown = definition.enemyMissile.settings.cooldownSeconds;
				controller.NotifyWeaponFired();
			}
			if (controller.GetMode() == AircraftCombatMode::Disengage) {
				breakSeconds += 1 / 60.0f;
				maximumBreak = std::max(maximumBreak, breakSeconds);
			} else
				breakSeconds = 0;
			if (seconds > 30 && distance >= definition.enemyAIParameters.disengageDistance &&
				distance <= definition.enemyAIParameters.reengageDistance)
				bandSeconds += 1 / 60.0f;
			if (seconds > 30 && distance < definition.enemyAIParameters.disengageDistance * 0.85f)
				closeSeconds += 1 / 60.0f;
			if (seconds > 45 && distance > definition.enemyAIParameters.attackDistance * 1.2f)
				distantSeconds += 1 / 60.0f;
			closest = std::min(closest, distance);
			fighter.Update(decision.flight, 1 / 60.0f);
			if (physicalPlayer) {
				const auto f = player.GetForward(), playerPosition = player.GetPosition();
				AircraftInput input;
				input.autoLevelRoll = true;
				input.turn = turning ? 0.35f : 0;
				input.throttle = std::clamp((battleSpeed - player.GetSpeed()) * 0.015f, -1.0f, 1.0f);
				input.pitch = std::clamp(
					(std::atan2(1500 - playerPosition.y, 1000.0f) - std::atan2(f.y, std::hypot(f.x, f.z))) *
						1.5f,
					-1.0f, 1.0f);
				player.Update(input, 1 / 60.0f);
			}
			const auto updated = fighter.GetPosition();
			Require(std::isfinite(updated.x) && std::isfinite(updated.y) && std::isfinite(updated.z) &&
					updated.y > 0,
				"Combat AI produced a non-finite position or entered the sea");
		}
		Require(firstLock >= 0 && firstLock <= 45,
			"Actual flight AI did not acquire a medium-range lock within 45 seconds");
		Require(bandSeconds > 45 && closeSeconds < 5 && shots >= 2 && distantSeconds < 10,
			"Actual flight AI did not maintain medium-range pursuit and repeated attacks");
		Require(maximumBreak <= definition.enemyAIParameters.maximumDisengageSeconds + 1 / 60.0f,
			"Actual flight AI exceeded its disengagement time limit");
		Require(closest > definition.enemyAIParameters.disengageDistance * 0.6f,
			"Actual flight AI entered close combat");
		std::cout << "PASS: tail pursuit physical=" << physicalPlayer << " turn=" << turning
				  << " slot=" << slot << " first rear=" << firstRear << " lock=" << firstLock
				  << " rear seconds=" << rearSeconds << " entries=" << rearEntries << " shots=" << shots
				  << " band seconds=" << bandSeconds << " close seconds=" << closeSeconds
				  << " far seconds=" << distantSeconds << " break=" << maximumBreak << " closest=" << closest
				  << '\n';
	};
	for (bool physicalPlayer : {false, true})
		for (bool turning : {false, true}) {
			for (size_t slot = 0; slot < definition.combat.enemyPositions.size(); ++slot)
				simulatePursuit(definition.combat.enemyPositions[slot], slot, turning, physicalPlayer);
			// 初期配置だけでなく、前方・側方・後方の接近からも確認する
			simulatePursuit({0, 1500, -5000}, 0, turning, physicalPlayer);
			simulatePursuit({5000, 1500, 0}, 1, turning, physicalPlayer);
			simulatePursuit({0, 1500, 5000}, 2, turning, physicalPlayer);
		}
	// 同じミサイルでも直進なら被弾し、接近を見てハイG旋回すると失探させられる。
	const auto evadeMissile = [&](bool evade) {
		AircraftFlightModel playerFlight;
		playerFlight.SetFlightParameters(definition.playerAircraft.flightParameters);
		playerFlight.SetPosition({0, 1500, -10000});
		playerFlight.SetHeading({0, 0, 1});
		RocketSystem missiles;
		Scene missileScene;
		missiles.Configure(definition.enemyMissile.settings, std::make_shared<Model>(), {1, 0, 0});
		XMFLOAT4X4 pose;
		XMStoreFloat4x4(&pose, XMMatrixTranslation(0, 1500, 0));
		missiles.Fire(missileScene, {}, pose, {0, 0, -1}, {0, 0, -650}, 77);
		bool hit = false, lost = false, turning = false;
		for (int frame = 0; frame < 1000 && missiles.GetCount(); ++frame) {
			const auto previous = playerFlight.GetPosition();
			const auto rocket = missiles.GetRockets().front().position;
			const float distance =
				XMVectorGetX(XMVector3Length(XMLoadFloat3(&rocket) - XMLoadFloat3(&previous)));
			AircraftInput input;
			input.autoLevelRoll = true;
			turning |= evade && distance < 6000;
			if (turning) {
				input.turn = 1;
				input.pitch = 0.8f;
				input.highGTurn = true;
			}
			playerFlight.Update(input, 1 / 60.0f);
			const auto position = playerFlight.GetPosition();
			CollisionTarget collision{
				77, {{-80, -30, -80}, {80, 30, 80}}, previous, playerFlight.GetWorldTransform()};
			CombatTarget target{77, position, playerFlight.GetVelocity(), "PLAYER", 100};
			missiles.Update(missileScene, 1 / 60.0f, std::span(&collision, 1), std::span(&target, 1));
			hit |= !missiles.TakeHits().empty();
			if (missiles.GetCount())
				lost |= missiles.GetRockets().front().targetId == InvalidObjectId;
		}
		return std::pair{hit, lost};
	};
	Require(evadeMissile(false).first, "Straight flight was never hit by missile");
	const auto avoided = evadeMissile(true);
	Require(!avoided.first && avoided.second, "High-G turn could not evade/break missile seeker");
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
			scene.AddPlayerObject(player);
			scene.ConfigurePlayerWeapon(definition.playerWeapon, player);
			scene.AddEnemyAircraft(enemy, {0, 15, -160}, 5, definition.enemyAircraft.flightParameters,
				definition.enemyAIParameters, 100, "ENEMY 01");
			ShipDefinition staticShip;
			staticShip.position = {2200, 0, -9000};
			staticShip.size = 5400;
			staticShip.waterlineOffset = -195;
			const auto shipTransform = staticShip.BuildTransform();
			const auto shipId = scene.AddObject(enemy, shipTransform);
			scene.Update({}, 0.25f, {}, {{true}});
			const auto& state = scene.GetCombatSnapshot();
			const auto& playerWorld = scene.GetObjects().front().worldTransform;
			Require(std::abs(state.playerPosition.x - playerWorld._41) < 0.001f &&
					std::abs(state.playerPosition.y - playerWorld._42) < 0.001f &&
					std::abs(state.playerPosition.z - playerWorld._43) < 0.001f &&
					std::abs(state.playerHeading.y) < 0.001f &&
					std::abs(std::hypot(state.playerHeading.x, state.playerHeading.z) - 1) < 0.001f,
				"Radar player snapshot does not match the aircraft position and horizontal heading");
			const auto& effectState = scene.GetEffectSnapshot();
			Require(state.enemies.empty() && state.destroyedSeconds > 0 && state.allDownSeconds > 0,
				"Projectile damage did not destroy enemy and notify all-down");
			Require(state.firedVolleys > 0 && state.hitCount == 1 && !effectState.impacts.empty(),
				"Combat feedback events missing");
			Require(state.audio.aircraftExplosions == 1 && !state.audio.flying && !state.audio.gunFiring,
				"Final kill audio missing, duplicated or loops retained");
			Require(effectState.explosions.size() == 1 && effectState.explosions.front().age == 0,
				"Lethal multi-hit did not create exactly one explosion");
			const auto deathPosition = effectState.explosions.front().position;
			scene.Update({}, 0.01f);
			Require(effectState.explosions.size() == 1 &&
					effectState.explosions.front().position.z == deathPosition.z,
				"Explosion did not survive enemy removal");
			Require(scene.GetCombatSnapshot().firedVolleys == 0 && scene.GetCombatSnapshot().hitCount == 0,
				"Combat sound events persisted across frames");
			Require(state.audio.aircraftExplosions == 0, "Kill audio repeated after victory");
			Require(state.selected == Engine::InvalidObjectId && state.lockState == LockState::None,
				"Destroyed enemy retained target reference");
			scene.Update({}, 0.25f, {}, {{true}});
			Require(scene.GetCombatSnapshot().allDownSeconds < 3, "All-down timer was repeatedly restarted");
			scene.Update({}, 0.25f);
			Require(scene.GetEffectSnapshot().impacts.empty(), "Impact effect did not expire");
			for (int frame = 0; frame < 14; ++frame)
				scene.Update({}, 0.25f);
			Require(effectState.explosions.empty(), "Explosion did not expire");
			for (int retry = 0; retry < 3; ++retry) {
				Require(state.result == CombatResult::Victory, "Last enemy kill did not produce victory");
				CombatInput input;
				input.retry = true;
				scene.Update({}, 0.01f, {}, input);
				const auto shipObject = std::find_if(
					scene.GetObjects().begin(), scene.GetObjects().end(), [&](const auto& object) {
						return object.id == shipId;
					});
				Require(shipObject != scene.GetObjects().end() &&
						std::memcmp(&shipObject->worldTransform, &shipTransform, sizeof(shipTransform)) == 0,
					"Retry did not preserve static ship transform");
				Require(std::abs(state.playerSpeedMetersPerSecond -
							definition.playerAircraft.flightParameters.initialSpeed) < 0.001f &&
						std::abs(state.playerAltitudeMeters) < 0.001f,
					"Retry did not restore flight HUD values");
				Require(std::hypot(state.playerPosition.x, state.playerPosition.z) < 0.001f &&
						std::abs(state.playerHeading.x) < 0.001f && state.playerHeading.z < -0.99f,
					"Retry did not restore the radar player position and heading");
				Require(state.result == CombatResult::Active && state.enemies.size() == 1 &&
						scene.GetProjectileCount() == 0 && scene.GetRocketCount() == 0 &&
						state.lockState == LockState::None,
					"Victory retry retained weapons, targets or result");
				Require(state.audio.restarted && state.audio.flying && !state.audio.aircraftExplosions,
					"Retry did not reset audio state");
				scene.Update({}, 0.25f, {}, {{true}});
			}
		}
		// 実際の遠距離戦で、捕捉警告→発射→追尾警告→命中/失探と再挑戦を検証する。
		{
			FlightScene battle;
			battle.Initialize(1, 1, 60000);
			auto combat = definition.combat;
			combat.playerHealth = 35;
			battle.ConfigureCombat(combat);
			auto targetFlight = definition.playerAircraft.flightParameters;
			targetFlight.initialSpeed = 500;
			targetFlight.gravity = targetFlight.drag = targetFlight.acceleration = 0;
			battle.SetPlayerAircraftFlightParameters(targetFlight);
			battle.SetPlayerAircraftPosition({0, 1500, 0});
			battle.AddPlayerObject(enemy);
			auto enemyFlight = targetFlight;
			enemyFlight.initialSpeed = 650;
			for (size_t i = 0; i < 3; ++i)
				battle.AddEnemyAircraft(enemy, {static_cast<float>(i) * 1000, 1500, 11000}, 1, enemyFlight,
					definition.enemyAIParameters, 120, "MISSILE", true);
			battle.ConfigureEnemyMissile(definition.enemyMissile, player);
			for (int retry = 0; retry < 3; ++retry) {
				bool locking = false, incoming = false;
				unsigned launches = 0;
				for (int frame = 0;
					frame < 14400 && battle.GetCombatSnapshot().result == CombatResult::Active; ++frame) {
					battle.Update({}, 1 / 60.0f);
					const auto& state = battle.GetCombatSnapshot();
					locking |= state.threatWarning == ThreatWarning::LockOn;
					if (!incoming && state.threatWarning == ThreatWarning::Missile) {
						FlightScene brokenLock = battle;
						brokenLock.SetPlayerAircraftPosition({100000, 1500, 0});
						brokenLock.Update({}, 1 / 60.0f);
						Require(brokenLock.GetEnemyMissileCount() > 0 &&
								brokenLock.GetCombatSnapshot().incomingMissiles.empty() &&
								brokenLock.GetCombatSnapshot().threatWarning == ThreatWarning::None,
							"Lost seeker retained incoming warning");
					}
					incoming |= state.threatWarning == ThreatWarning::Missile;
					launches += state.enemyMissilesLaunched;
					Require(state.lockingEnemyCount == state.lockingEnemyPositions.size(),
						"Lock warning positions were stale or missing");
					Require(state.lockingEnemyCount <= 2 && state.enemyMissilesLaunched <= 2,
						"More than two enemy missile locks/launches");
				}
				Require(locking && incoming && launches > 0, "Missile lock/launch warnings were missing");
				Require(battle.GetCombatSnapshot().result == CombatResult::Defeat,
					"Straight flight escaped every enemy missile");
				Require(battle.GetCombatSnapshot().audio.missileHits > 0 &&
						!battle.GetCombatSnapshot().audio.gunHits && !battle.GetCombatSnapshot().audio.flying,
					"Lethal missile audio was missing or misclassified");
				Require(battle.GetCombatSnapshot().incomingMissiles.empty() &&
						battle.GetCombatSnapshot().threatWarning == ThreatWarning::None,
					"Defeat retained missile warning");
				Require(std::all_of(battle.GetCombatSnapshot().enemies.begin(),
							battle.GetCombatSnapshot().enemies.end(),
							[](const CombatTarget& target) {
								return target.health == 120;
							}),
					"Enemy missiles damaged friendly aircraft");
				CombatInput input;
				input.retry = true;
				battle.Update({}, 0.01f, {}, input);
				Require(battle.GetEnemyMissileCount() == 0 && battle.GetCombatSnapshot().playerHealth == 35 &&
						battle.GetCombatSnapshot().threatWarning == ThreatWarning::None &&
						battle.GetObjects().size() == 4,
					"Retry retained enemy missiles, warning, cooldown or damage");
			}
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
			snapshot.rocket = {true, {0, 2.4f}, 0};
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
			effects.rocketSmoke.assign(RocketEffectSettings::maxSmokeCount, {{0, 0, 250}, 0.1f});
			effects.rocketExhausts.assign(RocketEffectSettings::maxExhaustCount, {{0, 0, 250}, {0, 0, -1}});
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
			effects.rocketSmoke.clear();
			effects.rocketExhausts.clear();
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
			effects.rocketExhausts = {{{60, 0, 200}, {0, 0, -1}}};
			for (int smoke = 0; smoke < 20; ++smoke)
				effects.rocketSmoke.push_back({{60, 0, 200 + smoke * 5.0f}, smoke * 0.025f});
			snapshot.selected = 1;
			// 両解像度の読み戻し画像に発射演出を含め、照準との重なりを確認する
			for (size_t muzzle = 0; muzzle < 4; ++muzzle) {
				const float x = -18 + float(muzzle) * 12;
				effects.gunFlashes.push_back({muzzle, {x, -10, 90}, {0, 0, 1}, 0.01f, muzzle});
				effects.gunSmoke.push_back({{x, -10, 90}, {20, 0, 0}, 0.15f});
				effects.gunTracers.push_back({{x + 3, -5, 170}, {x, -10, 100}});
			}
			buildFrame();
			const auto withoutWarning = list.GetVertices().size();
			snapshot.threatWarning = ThreatWarning::LockOn;
			snapshot.lockingEnemyCount = 1;
			snapshot.enemyLockProgress = 0.5f;
			snapshot.threatDirection = {1, 0, 1};
			buildFrame();
			Require(list.GetVertices().size() > withoutWarning, "Lock warning was invisible");
			snapshot.threatWarning = ThreatWarning::Missile;
			snapshot.incomingMissiles = {{88, {0, 0, 250}, 250}, {89, {5000, 0, 100}, 5001}};
			snapshot.lockingEnemyPositions = {{-5000, 0, 100}};
			buildFrame();
			Require(list.GetVertices().size() > withoutWarning, "Incoming missile warning was invisible");
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
			snapshot.threatWarning = ThreatWarning::LockOn;
			snapshot.incomingMissiles.clear();
			buildFrame();
			saveFrame("-lock");
			snapshot.threatWarning = ThreatWarning::None;
			snapshot.lockingEnemyPositions.clear();
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
			snapshot.rocket = {};
			snapshot.missionEnabled = true;
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
