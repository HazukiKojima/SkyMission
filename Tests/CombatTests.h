#pragma once
#include "Game/Combat/CombatController.h"
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/TargetingSystem.h"
#include "Game/Combat/EnemyEngagementProfile.h"
#include "Game/Combat/ProjectileCollision.h"
#include "Game/UI/CombatHudPresenter.h"
#include "Game/Effects/CombatEffectPresenter.h"
#include <iomanip>

void TestEnemyGunBehavior() {
	using namespace DirectX;
	std::array<EnemyGunPattern, 6> patterns;
	std::array<float, 6> cooldown{}, firstShot;
	std::array<unsigned, 6> shots{};
	firstShot.fill(-1);
	for (int frame = 0; frame < 1800; ++frame)
		for (size_t slot = 0; slot < patterns.size(); ++slot) {
			cooldown[slot] = std::max(0.0f, cooldown[slot] - 1 / 60.0f);
			if (patterns[slot].Update(1 / 60.0f, slot) && cooldown[slot] <= 0) {
				if (firstShot[slot] < 0) firstShot[slot] = frame / 60.0f;
				++shots[slot];
				cooldown[slot] = patterns[slot].Interval(0.22f, slot);
				patterns[slot].NotifyShot();
			}
		}
	for (auto count : shots)
		Require(count >= 5 && count < 45, "Enemy gun lacked burst pauses or stopped attacking");
	const auto times = std::minmax_element(firstShot.begin(), firstShot.end());
	Require(*times.second - *times.first > 0.5f, "Enemy guns started in a synchronized volley");
	const CombatTarget target{1, {1000, 1500, -5000}, {500, 0, 0}};
	const XMFLOAT3 position{0, 1500, 0}, inherited{0, 0, -700};
	const auto exact = SolveGunAim(position, inherited, target, 1800, 5);
	const auto direction = EnemyGunDirection(position, inherited, target, 1800, 2, 0);
	const auto repeat = EnemyGunDirection(position, inherited, target, 1800, 2, 0);
	const auto next = EnemyGunDirection(position, inherited, target, 1800, 2, 1);
	Require(exact &&
			XMVectorGetX(XMVector3Dot(XMLoadFloat3(&direction), XMLoadFloat3(&exact->direction))) <
				0.999999f &&
			!IsWithinAngle(exact->direction, direction, 6.0f),
		"Enemy suppression fire retained overly accurate future-position aiming");
	Require(direction.x == repeat.x && direction.y == repeat.y && direction.z == repeat.z &&
			(direction.x != next.x || direction.y != next.y),
		"Shot errors were unstable or identical for all shots");
	GunSettings gun;
	gun.muzzleNames = {"a", "b", "c", "d"};
	ProjectileSystem projectiles;
	projectiles.Configure(gun, std::make_shared<Model>(), {0, 1, 0});
	Scene scene;
	XMFLOAT3 velocity;
	XMStoreFloat3(&velocity, XMLoadFloat3(&inherited) + XMLoadFloat3(&direction) * 1800);
	Require(projectiles.FireSingle(scene, position, velocity, AttackSource::EnemyAircraft, 5),
		"Enemy shot failed");
	projectiles.Update(scene, 0.1f);
	const auto middle = scene.GetObjects().front().worldTransform;
	projectiles.Update(scene, 0.1f);
	const auto end = scene.GetObjects().front().worldTransform;
	Require(std::abs((end._41 - middle._41) - (middle._41 - position.x)) < 0.001f &&
			std::abs((end._42 - middle._42) - (middle._42 - position.y)) < 0.001f &&
			std::abs((end._43 - middle._43) - (middle._43 - position.z)) < 0.001f,
		"Enemy bullet curved after being fired");
	std::cout << "PASS: staggered enemy bursts, reduced cadence, imperfect aim and straight bullets\n";
}

void TestEnemyEngagementProfile() {
	AircraftAIParameters base;
	base.attackDistance = 9000;
	base.disengageDistance = 3000;
	base.cruiseSpeed = 800;
	base.tailDistance = 4500;
	base.aimHoldSeconds = 0.08f;
	const auto normal = GetEnemyEngagementProfile(LockState::Locked, 7, 7);
	const auto aggressive = GetEnemyEngagementProfile(LockState::Locked, 7, 8);
	const auto lockedAI = normal.Apply(base);
	const auto unlockedAI = aggressive.Apply(base);
	Require(lockedAI.cruiseSpeed == base.cruiseSpeed && lockedAI.tailDistance == base.tailDistance &&
			lockedAI.aimHoldSeconds == base.aimHoldSeconds && normal.shotIntervalScale == 1,
		"Locked enemy did not retain its normal engagement profile");
	Require(unlockedAI.cruiseSpeed > lockedAI.cruiseSpeed &&
			unlockedAI.tailDistance < lockedAI.tailDistance &&
			unlockedAI.aimHoldSeconds < lockedAI.aimHoldSeconds &&
			unlockedAI.attackDistance > lockedAI.attackDistance &&
			unlockedAI.disengageDistance == lockedAI.disengageDistance &&
			unlockedAI.aimAngleDegrees == lockedAI.aimAngleDegrees,
		"Unlocked profile did not increase aggression while preserving avoidance and accuracy");
	Require(GetEnemyEngagementProfile(LockState::Acquiring, 7, 7).shotIntervalScale < 1 &&
			GetEnemyEngagementProfile(LockState::None, 0, 7).shotIntervalScale < 1 &&
			GetEnemyEngagementProfile(LockState::Locked, 8, 7).shotIntervalScale < 1,
		"Selection/acquisition or switching targets incorrectly suppressed enemy aggression");
	const auto countShots = [&](const EnemyEngagementProfile& profile) {
		EnemyGunPattern pattern;
		const auto ai = profile.Apply(base);
		AircraftAIController pilot;
		AircraftFlightModel aircraft;
		aircraft.SetPosition({0, 1500, 0});
		const AircraftAITarget target{{0, 1500, -5000}, {}, {0, 0, -1}};
		float cooldown = 0;
		unsigned shots = 0;
		constexpr float dt = 1.0f / 120;
		for (int frame = 0; frame < 60 * 120; ++frame) {
			cooldown = std::max(0.0f, cooldown - dt);
			const bool ready = pattern.Update(dt * profile.burstTempoScale, 0) && cooldown <= 0;
			const auto decision = pilot.ReadCombatInput(
				aircraft, target, {0, 0, -1}, dt, ai, 0, true, ready, std::nullopt, true);
			if (decision.fire) {
				++shots;
				pattern.NotifyShot();
				cooldown = 0.1f * profile.shotIntervalScale;
				pilot.NotifyWeaponFired();
			}
		}
		return shots;
	};
	Require(countShots(aggressive) > countShots(normal) * 1.15f,
		"Unlocked enemy did not produce more attack opportunities over repeated bursts");
	std::cout << "PASS: player-lock engagement profiles, unchanged accuracy and more aggressive bursts\n";
}

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
		if (i < 299) Require(!decision.fire, "Enemy launched during opening grace");
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
	AircraftAIController recover;
	const auto settle = [&](AircraftAIController& ai, AircraftAITarget t) {
		AircraftAIDecision result;
		for (int i = 0; i < 10; ++i)
			result = ai.ReadCombatInput(aircraft, t, forward, 0.1f, p, 0, true);
		return result;
	};
	controller.NotifyWeaponFired();
	controller.ReadCombatInput(aircraft, target, forward, 0.1f, p, 0, true);
	Require(controller.GetMode() == AircraftCombatMode::Attack, "Shot interrupted attack run");
	AircraftAIController pass;
	const AircraftAITarget close{{280, 300, -100}, {}, forward};
	bool finishedPass = false;
	float fixedTurn = 0;
	for (int i = 0; i < 95; ++i) {
		const auto d = pass.ReadCombatInput(aircraft, close, forward, 0.1f, p, 0, true);
		if (i == 0) fixedTurn = d.flight.turn;
		if (i < 60)
			Require(pass.GetMode() == AircraftCombatMode::Disengage && d.flight.turn == fixedTurn &&
					!d.locking && !d.fire,
				"Pass did not hold its selected exit direction");
		finishedPass |= pass.GetMode() == AircraftCombatMode::Reengage;
	}
	Require(finishedPass, "Straight pass exceeded nine seconds");
	AircraftAIController minimumPass;
	minimumPass.ReadCombatInput(aircraft, close, forward, 0.1f, p, 0, true);
	const AircraftAITarget separated{{0, 300, -5000}, {}, forward};
	for (int i = 0; i < 29; ++i) {
		minimumPass.ReadCombatInput(aircraft, separated, forward, 0.1f, p, 0, true);
		Require(minimumPass.GetMode() == AircraftCombatMode::Disengage, "Pass ended before three seconds");
	}
	aircraft.SetPosition({0, 300, -1500});
	for (int i = 0; i < 25; ++i)
		minimumPass.ReadCombatInput(aircraft, separated, forward, 0.1f, p, 0, true);
	Require(minimumPass.GetMode() != AircraftCombatMode::Disengage, "Separated pass did not turn back");
	aircraft.SetPosition({0, 300, 0});
	AircraftAIController reentry;
	reentry.ReadCombatInput(aircraft, close, forward, 0.1f, p, 0, true);
	const AircraftAITarget behind{{0, 300, 5000}, {}, forward};
	AircraftAIDecision burst;
	for (int i = 0; i < 90 && reentry.GetMode() != AircraftCombatMode::Reengage; ++i)
		burst = reentry.ReadCombatInput(aircraft, behind, {0, 0, 1}, 0.1f, p, 0, true);
	Require(burst.flight.highGTurn, "Distant reentry did not use a short high-G burst");
	for (int i = 0; i < 41; ++i)
		burst = reentry.ReadCombatInput(aircraft, behind, {0, 0, 1}, 0.1f, p, 0, true);
	Require(!burst.flight.highGTurn, "Reentry kept high G beyond its initial burst");
	AircraftAIController collision;
	{
		AircraftAIController rearEscape;
		const AircraftAITarget tailing{{0, 300, 2000}, {}, forward};
		rearEscape.ReadCombatInput(aircraft, tailing, {0, 0, 1}, 0.1f, p, 0, true);
		Require(rearEscape.GetMode() == AircraftCombatMode::Disengage,
			"Rear target triggered a circling chase instead of a straight escape");
		AircraftAIController circling;
		const AircraftAITarget lateral{{5000, 300, 0}, {}, forward};
		bool escaped = false;
		for (int frame = 0; frame < 80; ++frame) {
			circling.ReadCombatInput(aircraft, lateral, {1, 0, 0}, 0.1f, p, 0, true);
			escaped |= circling.GetMode() == AircraftCombatMode::Disengage;
		}
		Require(escaped, "Continuous pursuit turn never yielded to a straight leg");
		bool reentryEnded = false;
		for (int frame = 0; frame < 30; ++frame) {
			reentry.ReadCombatInput(aircraft, behind, {0, 0, 1}, 0.1f, p, 0, true);
			reentryEnded |= reentry.GetMode() == AircraftCombatMode::Disengage;
		}
		Require(reentryEnded, "Reengagement circled indefinitely without a new straight leg");
	}
	const auto avoiding = collision.ReadCombatInput(
		aircraft, {{0, 300, -100}, {0, 0, 60}, {0, 0, 1}}, forward, 0.1f, p, 0, true);
	Require(avoiding.flight.highGTurn && std::abs(avoiding.flight.turn) > 0.1f && !avoiding.fire,
		"Imminent collision did not override straight flight");
	// Identical geometry must produce distinct, reproducible pilot choices.
	{
		AircraftFlightModel formationAircraft;
		formationAircraft.SetPosition({0, 1500, 0});
		std::array<AircraftAIController, 6> pilots;
		std::array<AircraftAIDecision, 6> exits;
		const AircraftAITarget nearby{{280, 1500, -100}, {}, forward};
		const AircraftAITarget farBehind{{0, 1500, 5000}, {}, forward};
		for (size_t i = 0; i < pilots.size(); ++i)
			exits[i] = pilots[i].ReadCombatInput(formationAircraft, nearby, forward, 0.1f, p, i, true);
		Require(exits[1].flight.pitch > 0 && exits[4].flight.pitch < 0,
			"Pilot styles did not separate climbing and descending exits");
		Require(std::abs(exits[0].flight.yaw - exits[5].flight.yaw) > 0.1f,
			"Pilot styles used identical exit angles");
		AircraftAIController repeat;
		const auto repeated = repeat.ReadCombatInput(formationAircraft, nearby, forward, 0.1f, p, 4, true);
		Require(repeated.flight.pitch == exits[4].flight.pitch && repeated.flight.yaw == exits[4].flight.yaw,
			"Pilot style changed across identical starts");
		std::array<float, 6> turns{};
		for (int frame = 0; frame < 100; ++frame)
			for (size_t i = 0; i < pilots.size(); ++i) {
				pilots[i].ReadCombatInput(formationAircraft, farBehind, {0, 0, 1}, 0.1f, p, i, true);
				if (turns[i] == 0 && pilots[i].GetMode() == AircraftCombatMode::Reengage)
					turns[i] = frame * 0.1f;
			}
		const auto range = std::minmax_element(turns.begin(), turns.end());
		Require(*range.first > 0 && *range.second - *range.first > 0.5f,
			"Pilot styles turned back at the same time");
	}
	{
		AircraftFlightParameters fastFlight;
		fastFlight.initialSpeed = 700;
		fastFlight.maximumSpeed = 1700;
		AircraftFlightModel approaching;
		approaching.SetFlightParameters(fastFlight);
		approaching.SetPosition({0, 1500, 0});
		auto spacing = p;
		spacing.disengageDistance = 3000;
		spacing.tailDistance = 4500;
		spacing.separationLookAheadSeconds = 3.5f;
		AircraftAIController bypass;
		const AircraftAITarget headOn{{0, 1500, -5000}, {0, 0, 500}, {0, 0, 1}};
		const auto departure = bypass.ReadCombatInput(approaching, headOn, forward, 0.1f, spacing, 0, true);
		Require(bypass.GetMode() == AircraftCombatMode::Disengage && std::abs(departure.flight.turn) > 0.5f &&
				departure.flight.highGTurn,
			"Head-on attack continued straight at the player instead of bypassing early");
		AircraftAIDecision afterBurst;
		for (int i = 0; i < 21; ++i)
			afterBurst = bypass.ReadCombatInput(approaching, headOn, forward, 0.1f, spacing, 0, true);
		Require(!afterBurst.flight.highGTurn, "Spacing break sustained a high-G orbit");
	}
	AircraftAIController defense;
	aircraft.SetPosition({13000, 300, 0});
	const XMFLOAT3 home{0, 0, 0};
	auto d = defense.ReadCombatInput(aircraft, target, forward, 0.1f, p, 0, true, true, home);
	Require(defense.GetMode() == AircraftCombatMode::ReturnToCarrier && !d.locking && d.lockProgress == 0,
		"Defense boundary did not reset lock and start return");
	aircraft.SetPosition({10000, 300, 0});
	defense.ReadCombatInput(aircraft, target, forward, 0.1f, p, 0, true, true, home);
	Require(defense.GetMode() == AircraftCombatMode::ReturnToCarrier, "Return hysteresis released early");
	aircraft.SetPosition({7900, 300, 0});
	defense.ReadCombatInput(aircraft, target, forward, 0.1f, p, 0, true, true, home);
	Require(defense.GetMode() == AircraftCombatMode::Reengage, "Returned fighter did not reengage");
	aircraft.SetPosition({13000, 300, 0});
	aircraft.SetHeading({-1, 0, 0});
	AircraftAIController returningGun, returningMissile;
	bool returnShot = false;
	for (int i = 0; i < 40; ++i) {
		const AircraftAITarget inward{{11000, 300, 0}, {}, {-1, 0, 0}};
		const auto gun =
			returningGun.ReadCombatInput(aircraft, inward, {-1, 0, 0}, 0.1f, p, 0, true, true, home, true);
		returnShot |= gun.fire;
		Require(!gun.locking && gun.lockProgress == 0, "Returning gun acquired a missile lock");
		const auto missile =
			returningMissile.ReadCombatInput(aircraft, inward, {-1, 0, 0}, 0.1f, p, 0, true, true, home);
		Require(!missile.fire, "Returning missile bypassed lock acquisition");
	}
	Require(returnShot, "Returning gun ignored a safe firing opportunity");
	aircraft.SetPosition({0, 50, 0});
	const auto recovery = settle(recover, target);
	Require(recover.GetMode() == AircraftCombatMode::Recover && recovery.flight.pitch > 0,
		"Low altitude did not trigger climb");
	// 高高度の回避では降下を促し、高高度の目標そのものへの追従は許可する
	aircraft.SetPosition({0, 2500, 0});
	aircraft.SetHeading({0, 0, -1});
	AircraftAIController highAvoidance;
	const auto high = highAvoidance.ReadCombatInput(
		aircraft, {{0, 300, -100}, {0, 0, 60}, forward}, forward, 0.1f, p, 3, true);
	Require(high.flight.pitch < 0, "High altitude avoidance kept climbing above the combat band");
	AircraftAIController highPursuit;
	const auto chasing = highPursuit.ReadCombatInput(
		aircraft, {{0, 3500, -5000}, {}, forward}, {0, 0.2f, -1}, 0.1f, p, 0, true);
	Require(chasing.flight.pitch > 0, "Altitude control prevented pursuit of a higher target");
	std::cout << "PASS: straight pass, reengagement, defense hysteresis, recovery and missile acquisition\n";
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
	{
		TargetingSystem replacement;
		std::vector<CombatTarget> remaining{
			{10, {0, 0, -1000}, {}, "FRONT", 100}, {11, {0, 0, 1000}, {}, "REAR", 100}};
		replacement.Update(remaining, {}, {0, 0, -1}, false, 0.25f);
		replacement.Update(remaining, {}, {0, 0, -1}, false, 0.25f);
		Require(replacement.GetSelected() == 10 && replacement.GetState() == LockState::Locked,
			"Replacement fixture did not lock the front enemy");
		remaining[0].health = 0;
		replacement.Update(remaining, {}, {0, 0, -1}, false, 0);
		Require(replacement.GetSelected() == 11 && replacement.GetState() == LockState::Selected &&
				replacement.GetProgress() == 0,
			"Destroyed target did not switch to a rear enemy with a fresh lock state");
		replacement.Update({}, {}, {0, 0, -1}, false, 0);
		Require(
			replacement.GetSelected() == Engine::InvalidObjectId, "All enemies destroyed retained a target");
	}

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
	const std::array<DirectX::XMFLOAT3, 3> initialPositions = {
		{{-4500, 1200, -14500}, {7000, 1800, -16000}, {-13000, 1600, 7000}}};
	// 実機の飛行モデルで通過・再接近・反復攻撃と距離帯を測定する
	const auto simulatePursuit = [&](DirectX::XMFLOAT3 start, size_t slot, bool turning, bool physicalPlayer,
									 bool missionGun = false) {
		const float battleSpeed = definition.playerAircraft.flightParameters.initialSpeed;
		auto ai = definition.enemyAIParameters;
		if (missionGun) {
			ai.aimAngleDegrees = 10;
			ai.aimHoldSeconds = 0.08f;
			ai.openingGraceSeconds += EnemyVariation(slot, 0, 79) * 1.5f;
		}
		AircraftFlightModel fighter;
		fighter.SetFlightParameters(definition.enemyAircraft.flightParameters);
		fighter.SetPosition(start);
		fighter.SetHeading({-start.x, 1500 - start.y, -start.z});
		AircraftAIController controller, conservative;
		EnemyGunPattern gunPattern;
		float conservativeCooldown = 0;
		unsigned conservativeShots = 0;
		AircraftFlightModel player;
		player.SetFlightParameters(definition.playerAircraft.flightParameters);
		player.SetPosition({0, 1500, 0});
		float firstRear = -1, firstLock = -1, rearSeconds = 0, distantSeconds = 0;
		float bandSeconds = 0, closeSeconds = 0, closeRun = 0, maximumCloseRun = 0;
		unsigned passes = 0, acquisitions = 0;
		bool wasLocking = false;
		bool wasPassing = false;
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
			if (physicalPlayer) target = {player.GetPosition(), player.GetVelocity(), player.GetForward()};
			const auto position = fighter.GetPosition();
			const auto delta = XMLoadFloat3(&target.position) - XMLoadFloat3(&position);
			const float distance = XMVectorGetX(XMVector3Length(delta));
			XMFLOAT3 direction;
			XMStoreFloat3(&direction, XMVector3Normalize(delta));
			if (missionGun) {
				const CombatTarget gunTarget{1, target.position, target.velocity};
				if (const auto aim = SolveGunAim(position, fighter.GetVelocity(), gunTarget, 1800, 5))
					direction = aim->direction;
			}
			cooldown = std::max(0.0f, cooldown - 1 / 60.0f);
			const bool allowed = missionGun || slot % 3 != static_cast<size_t>(seconds / 6) % 3;
			const bool gunWindow = !missionGun || gunPattern.Update(1 / 60.0f, slot);
			const auto decision = controller.ReadCombatInput(fighter, target, direction, 1 / 60.0f, ai, slot,
				allowed, cooldown <= 0 && gunWindow, std::nullopt, missionGun);
			if (missionGun) {
				auto previous = ai;
				previous.aimAngleDegrees = 10;
				previous.aimHoldSeconds = 0.025f;
				conservativeCooldown = std::max(0.0f, conservativeCooldown - 1 / 60.0f);
				const auto old = conservative.ReadCombatInput(fighter, target, direction, 1 / 60.0f, previous,
					slot, allowed, conservativeCooldown <= 0, std::nullopt, true);
				Require(old.flight.turn == decision.flight.turn && old.flight.yaw == decision.flight.yaw &&
						old.flight.pitch == decision.flight.pitch &&
						old.flight.throttle == decision.flight.throttle &&
						old.flight.highGTurn == decision.flight.highGTurn,
					"Increased firing changed the aircraft movement");
				if (old.fire) {
					++conservativeShots;
					conservativeCooldown = 0.065f;
					conservative.NotifyWeaponFired();
				}
			}
			const bool rear = distance <= definition.enemyAIParameters.attackDistance &&
				XMVectorGetX(XMVector3Dot(XMLoadFloat3(&direction), XMLoadFloat3(&target.forward))) >= 0.5f;
			if (rear) {
				if (firstRear < 0) firstRear = seconds;
				rearSeconds += 1 / 60.0f;
				if (!wasRear) ++rearEntries;
			}
			wasRear = rear;
			if (decision.locking) {
				if (!wasLocking) ++acquisitions;
				Require(distance >= (missionGun ? 2200.0f : definition.enemyAIParameters.disengageDistance) &&
						distance <= definition.enemyAIParameters.attackDistance && allowed && cooldown <= 0,
					"Enemy locked outside its medium-range attack window");
				if (firstLock < 0) firstLock = seconds;
			}
			wasLocking = decision.locking;
			if (decision.fire) {
				++shots;
				cooldown = missionGun ? gunPattern.Interval(0.22f, slot) : 3.0f;
				if (missionGun) gunPattern.NotifyShot();
				controller.NotifyWeaponFired();
			}
			if (controller.GetMode() == AircraftCombatMode::Disengage) {
				if (!wasPassing) ++passes;
				wasPassing = true;
				breakSeconds += 1 / 60.0f;
				maximumBreak = std::max(maximumBreak, breakSeconds);
			} else {
				breakSeconds = 0;
				wasPassing = false;
			}
			closeRun = distance < 3000 ? closeRun + 1 / 60.0f : 0;
			maximumCloseRun = std::max(maximumCloseRun, closeRun);
			if (seconds > 30 && distance >= definition.enemyAIParameters.disengageDistance &&
				distance <= definition.enemyAIParameters.reengageDistance + (missionGun ? 1000.0f : 0.0f))
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
		std::cout << "PASS: attack runs physical=" << physicalPlayer << " turn=" << turning
				  << " gun=" << missionGun << " acquisitions=" << acquisitions << " slot=" << slot
				  << " first rear=" << firstRear << " lock=" << firstLock
				  << " previous shots=" << conservativeShots << " rear seconds=" << rearSeconds
				  << " entries=" << rearEntries << " shots=" << shots << " band seconds=" << bandSeconds
				  << " close seconds=" << closeSeconds << " passes=" << passes
				  << " close run=" << maximumCloseRun << " far seconds=" << distantSeconds
				  << " break=" << maximumBreak << " closest=" << closest << '\n';
		Require(firstLock >= 0 && firstLock <= 45, "No attack opportunity within 45 seconds");
		Require(shots >= (missionGun ? 2u : 1u) && acquisitions >= 2 && passes >= 2 && bandSeconds > 45 &&
				distantSeconds < 10,
			"Fighter did not repeat medium-range attack runs");
		Require(maximumBreak <= 9.02f, "Straight pass exceeded nine seconds");
		Require(maximumCloseRun < 10, "Fighter remained in close combat for ten seconds");
		if (missionGun)
			Require(
				shots < conservativeShots, "Burst guns did not reduce the previous excessive firing rate");
		if (missionGun)
			Require(closest > 300, "Mission fighter rammed through the player during repeated attacks");
	};
	for (bool physicalPlayer : {false, true})
		for (bool turning : {false, true}) {
			for (size_t slot = 0; slot < initialPositions.size(); ++slot)
				simulatePursuit(initialPositions[slot], slot, turning, physicalPlayer);
			// 初期配置だけでなく、前方・側方・後方の接近からも確認する
			simulatePursuit({0, 1500, -5000}, 0, turning, physicalPlayer);
			simulatePursuit({5000, 1500, 0}, 1, turning, physicalPlayer);
			simulatePursuit({0, 1500, 5000}, 2, turning, physicalPlayer);
		}
	// Mission guns fire across repeated passes while preserving the flight controls.
	for (bool turning : {false, true})
		for (size_t slot = 0; slot < 6; ++slot)
			simulatePursuit({(float(slot) - 2.5f) * 1500,
								1500 + std::array<float, 6>{150, 650, -250, 1000, 350, -100}[slot],
								-5500 - std::array<float, 6>{-600, 1000, -1300, 500, -200, 1500}[slot]},
				slot, turning, true, true);
	// Even from identical starting positions, six aircraft must choose different turn sites.
	for (bool turning : {false, true}) {
		std::array<AircraftFlightModel, 6> planes;
		std::array<AircraftAIController, 6> controllers;
		std::array<XMFLOAT3, 6> turnPoints{};
		std::array<float, 6> turnTimes{};
		std::array<unsigned, 6> passes{};
		for (auto& plane : planes) {
			plane.SetFlightParameters(definition.enemyAircraft.flightParameters);
			plane.SetPosition({0, 1500, -5000});
			plane.SetHeading({0, 0, 1});
		}
		AircraftFlightModel player;
		player.SetFlightParameters(definition.playerAircraft.flightParameters);
		player.SetPosition({0, 1500, 0});
		for (int frame = 0; frame < 3600; ++frame) {
			const AircraftAITarget target{player.GetPosition(), player.GetVelocity(), player.GetForward()};
			for (size_t i = 0; i < planes.size(); ++i) {
				const auto position = planes[i].GetPosition();
				XMFLOAT3 direction;
				XMStoreFloat3(
					&direction, XMVector3Normalize(XMLoadFloat3(&target.position) - XMLoadFloat3(&position)));
				const CombatTarget gunTarget{1, target.position, target.velocity};
				if (const auto aim = SolveGunAim(position, planes[i].GetVelocity(), gunTarget, 1800, 5))
					direction = aim->direction;
				const auto before = controllers[i].GetMode();
				const auto decision = controllers[i].ReadCombatInput(planes[i], target, direction, 1 / 60.0f,
					definition.enemyAIParameters, i, true, true, std::nullopt, true);
				if (before == AircraftCombatMode::Disengage &&
					controllers[i].GetMode() == AircraftCombatMode::Reengage) {
					if (passes[i]++ == 0) {
						turnPoints[i] = position;
						turnTimes[i] = frame / 60.0f;
					}
				}
				planes[i].Update(decision.flight, 1 / 60.0f);
			}
			const auto forward = player.GetForward(), position = player.GetPosition();
			AircraftInput input;
			input.autoLevelRoll = true;
			input.turn = turning ? 0.35f : 0;
			input.throttle = std::clamp(
				(definition.playerAircraft.flightParameters.initialSpeed - player.GetSpeed()) * 0.015f, -1.0f,
				1.0f);
			input.pitch = std::clamp((std::atan2(1500 - position.y, 1000.0f) -
										 std::atan2(forward.y, std::hypot(forward.x, forward.z))) *
					1.5f,
				-1.0f, 1.0f);
			player.Update(input, 1 / 60.0f);
		}
		float closestTurnPoints = 100000;
		for (size_t i = 0; i < planes.size(); ++i) {
			Require(passes[i] >= 2, "Independent aircraft stopped reengaging");
			for (size_t j = i + 1; j < planes.size(); ++j)
				closestTurnPoints = std::min(closestTurnPoints,
					XMVectorGetX(
						XMVector3Length(XMLoadFloat3(&turnPoints[i]) - XMLoadFloat3(&turnPoints[j]))));
		}
		const auto times = std::minmax_element(turnTimes.begin(), turnTimes.end());
		Require(closestTurnPoints > 750 && *times.second - *times.first > 1.5f,
			"Aircraft turn locations or timing clustered despite independent exit routes");
		std::cout << "PASS: six independent turn sites, turning target=" << turning
				  << " closest=" << closestTurnPoints << " time spread=" << *times.second - *times.first
				  << '\n';
	}
	// A fleeing player must not drag a defending fighter away indefinitely.
	{
		AircraftFlightModel fighter;
		fighter.SetFlightParameters(definition.enemyAircraft.flightParameters);
		fighter.SetPosition({13000, 1500, 0});
		fighter.SetHeading({1, 0, 0});
		AircraftAIController ai;
		bool returned = false;
		for (int frame = 0; frame < 7200; ++frame) {
			const auto pos = fighter.GetPosition();
			const AircraftAITarget target{{30000 + frame * 10.0f, 1500, 0}, {600, 0, 0}, {1, 0, 0}};
			XMFLOAT3 aim;
			XMStoreFloat3(&aim, XMVector3Normalize(XMLoadFloat3(&target.position) - XMLoadFloat3(&pos)));
			const auto d = ai.ReadCombatInput(fighter, target, aim, 1 / 60.0f, definition.enemyAIParameters,
				0, true, true, XMFLOAT3{0, 0, 0});
			if (ai.GetMode() == AircraftCombatMode::ReturnToCarrier) {
				Require(!d.locking && d.lockProgress == 0 && !d.fire,
					"Returning fighter retained distant target acquisition");
			} else if (std::hypot(pos.x, pos.z) <= 8000)
				returned = true;
			fighter.Update(d.flight, 1 / 60.0f);
		}
		Require(returned, "Defending fighter did not physically return inside eight kilometers");
	}
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
			combat.feedback.hitSeconds = 1.1f;
			combat.feedback.destroyedSeconds = 2.2f;
			scene.ConfigureCombat(combat);
			scene.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
			scene.SetPlayerAircraftPosition({0, 100, 0});
			const auto playerId = scene.AddPlayerObject(player);
			scene.ConfigurePlayerWeapon(definition.playerWeapon, player);
			scene.AddEnemyAircraft(enemy, {0, 115, -160}, 5, definition.enemyAircraft.flightParameters,
				definition.enemyAIParameters, 100, "PATROL");
			{
				FlightScene encouragement;
				encouragement.Initialize(1, 1, 100000);
				auto safeCombat = combat;
				safeCombat.playerHealth = 100000;
				encouragement.ConfigureCombat(safeCombat);
				encouragement.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
				encouragement.SetPlayerAircraftPosition({0, 1500, 0});
				encouragement.AddPlayerObject(player);
				encouragement.ConfigurePlayerWeapon(definition.playerWeapon, player);
				encouragement.ConfigureCinematics(definition.cinematics, [&](const std::string& id) {
					return FlightScene::CinematicModel{id == PlayerModelId ? player : enemy, 1};
				}, 100000);
				encouragement.AddEnemyAircraft(enemy, {0, 1515, -160}, 5, definition.enemyAircraft.flightParameters,
					definition.enemyAIParameters, 100, "TEST");
				for (float x : {-1000.0f, 1000.0f}) encouragement.AddEnemyAircraft(enemy, {x, 1500, -10000},
					5, definition.enemyAircraft.flightParameters, definition.enemyAIParameters, 100, "SURVIVOR");
				encouragement.Update({}, 0.25f, {}, {{true}});
				Require(encouragement.GetCombatSnapshot().enemies.size() == 2 &&
					encouragement.GetCombatSnapshot().cinematic.cinematicId != "patrolEncouragement",
					"Encouragement started before the second patrol kill");
				auto second = encouragement.GetCombatSnapshot().playerPosition;
				second.y += 15; second.z -= 160;
				encouragement.AddEnemyAircraft(enemy, second, 5, definition.enemyAircraft.flightParameters,
					definition.enemyAIParameters, 100, "SECOND");
				encouragement.Update({}, 0.25f, {}, {{true}});
				Require(encouragement.GetCombatSnapshot().enemies.size() == 2, "Second patrol fixture was not shot down");
				for (int frame = 0; frame < 80 &&
					encouragement.GetCombatSnapshot().cinematic.cinematicId != "patrolEncouragement"; ++frame)
					encouragement.Update({}, 0.25f);
				Require(encouragement.GetCombatSnapshot().cinematic.cinematicId == "patrolEncouragement" &&
					encouragement.GetCombatSnapshot().cinematic.radioLine == 0,
					"Second patrol kill did not lead to encouragement after enemy chatter");
				const auto serial = encouragement.GetCombatSnapshot().cinematic.playbackSerial;
				encouragement.UpdateCinematicBriefing(CinematicVoiceStatus::Finished, 0.01f);
				for (int frame = 0; frame < 20; ++frame) encouragement.Update({}, 0.25f);
				Require(encouragement.GetCombatSnapshot().cinematic.playbackSerial == serial,
					"Patrol encouragement repeated");
			}
			{
				auto transition = scene;
				DirectX::XMFLOAT4X4 shipTransform;
				DirectX::XMStoreFloat4x4(&shipTransform, DirectX::XMMatrixTranslation(0, 0, -8500));
				const auto carrier = transition.AddObject(enemy, shipTransform);
				transition.RegisterMissionShip(carrier, true);
				transition.ConfigureCinematics(definition.cinematics, [&](const std::string& id) {
					return FlightScene::CinematicModel{id == PlayerModelId ? player : enemy, 1};
				}, 100000);
				transition.Update({}, 0.25f, {}, {{true}});
				const auto& command = transition.GetCombatSnapshot();
				Require(command.enemies.empty() && command.audio.aircraftExplosions == 1 &&
					command.missionPhase == MissionPhase::CarrierAttack && !command.cinematic.active &&
					command.enemyRadio.clip >= 0,
					"Last patrol kill did not allow the dying enemy voice before completion praise");
				for (int frame = 0; frame < 60 && command.cinematic.cinematicId != "patrolComplete"; ++frame)
					transition.Update({}, 0.25f);
				Require(command.cinematic.cinematicId == "patrolComplete" && command.cinematic.radioLine == 0 &&
					command.enemyRadio.clip == -1, "Patrol praise did not follow the final death call");
				transition.UpdateCinematicBriefing(CinematicVoiceStatus::Finished, 0.01f);
				transition.Update({}, 0.25f);
				Require(command.cinematic.cinematicId == "patrolComplete" && command.cinematic.radioLine == -1,
					"Carrier order interrupted the half-second pause");
				transition.Update({}, 0.25f);
				Require(command.cinematic.cinematicId == "midMission" && command.cinematic.radioLine == 0,
					"Carrier order did not follow the praise and pause");
			}
			scene.Update({}, 0.25f, {}, {{true}});
			const auto& state = scene.GetCombatSnapshot();
			Require(state.enemies.empty() && state.hitCount == 1 && state.hitSeconds == 1.1f &&
					state.destroyedSeconds == 2.2f,
				"Lethal cannon volley did not produce one kill");
			Require(state.result == CombatResult::Active && state.missionPhase == MissionPhase::CarrierAttack,
				"Patrol all-down incorrectly produced victory");
			Require(state.enemyRadio.clip >= 0 && state.enemyRadio.caption.side == "enemy",
				"Lethal hit did not trigger enemy radio");
			Require(scene.GetEffectSnapshot().explosions.size() == 1,
				"Multiple simultaneous bullets created duplicate death effects");
			scene.RemoveObject(playerId);
			scene.Update({}, 0.01f);
			Require(state.result == CombatResult::Defeat, "Player removal did not produce defeat");
			CombatInput retry;
			retry.retry = true;
			scene.Update({}, 0.01f, {}, retry);
			Require(state.result == CombatResult::Active && state.enemies.size() == 1 &&
					scene.GetProjectileCount() == 0 && scene.GetCarrierMissileCount() == 0 &&
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
		bullets.FireVolley(scene,
			{.muzzles = muzzles,
				.aircraftWorld = world,
				.aircraftPose = pose,
				.forward = {0, 0, -1},
				.aircraftVelocity = {},
				.ageSeconds = 0.25f,
				.assistance = {},
				.targets = boxes,
				.frameSeconds = 0.25f});
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
		bullets.FireVolley(scene,
			{.muzzles = muzzles,
				.aircraftWorld = world,
				.aircraftPose = pose,
				.forward = {0, 0, -1},
				.aircraftVelocity = {},
				.ageSeconds = 0.25f,
				.assistance = {},
				.targets = boxes,
				.frameSeconds = 0.25f});
		Require(bullets.TakeHits().empty(), "Expired projectile hit beyond its lifetime");
		gun.lifetime = 3;
		bullets.Configure(gun, player, {0, 1, 0});
		AimAssistance assistance{CombatTarget{3, {30, 20, -1000}, {}, "TEST", 100}, 3};
		bullets.FireVolley(scene,
			{.muzzles = muzzles,
				.aircraftWorld = world,
				.aircraftPose = pose,
				.forward = {0, 0, -1},
				.aircraftVelocity = {},
				.ageSeconds = 0,
				.assistance = assistance});
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
			snapshot.missileImpactPoint = XMFLOAT3{100, -100, 1500};
			snapshot.missileCooldownSeconds = 4.2f;
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
