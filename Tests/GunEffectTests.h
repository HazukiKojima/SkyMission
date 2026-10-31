#pragma once
#include "Game/Effects/CombatEffectSystem.h"
#include "Game/Weapons/ProjectileSystem.h"
#include "Game/UI/HudProjection.h"

void TestGunFireEffects() {
	using namespace DirectX;
	GunSettings settings;
	settings.muzzleNames = {"L1", "L2", "R1", "R2"};
	settings.muzzleSpeed = 1000;
	settings.lifetime = 0.2f;
	settings.maxProjectiles = 4;
	auto model = std::make_shared<Model>();
	Scene scene;
	ProjectileSystem bullets;
	bullets.Configure(settings, model, {0, 0, -1});
	const std::array<XMFLOAT3, 4> muzzles{{{-3, 0, 0}, {-1, 0, 0}, {1, 0, 0}, {3, 0, 0}}};
	XMFLOAT4X4 world;
	XMStoreFloat4x4(&world, XMMatrixTranslation(0, 20, 0));
	const auto fire = [&](float age) {
		return bullets.FireVolley(scene, muzzles, world, world, {0, 0, -1}, {20, 0, 0}, age);
	};
	Require(fire(0.01f), "Gun effect volley failed");
	auto shots = bullets.TakeShotEvents();
	Require(shots.size() == 4 && bullets.TakeShotEvents().empty(), "Gun notifications were lost or replayed");
	for (size_t index = 0; index < shots.size(); ++index) {
		Require(shots[index].muzzleIndex == index && shots[index].position.x == muzzles[index].x &&
				shots[index].position.y == 20 && shots[index].age == 0.01f &&
				shots[index].aircraftVelocity.x == 20,
			"Gun notification lost muzzle transform, velocity or frame age");
	}
	auto tracers = bullets.TakeTracers();
	Require(tracers.size() == 2 && std::abs(tracers[0].tail.x + 3) < 0.001f &&
			std::abs(tracers[1].tail.x - 3) < 0.001f,
		"First volley did not trace L1 and R2");
	Require(std::abs(tracers[0].head.x + 2.8f) < 0.001f && std::abs(tracers[0].head.z + 10) < 0.001f &&
			std::abs(tracers[0].tail.z) < 0.001f,
		"Tracer did not use actual inherited velocity or exceeded birth position");
	Require(!fire(0.01f) && bullets.TakeShotEvents().empty() && bullets.TakeTracers().empty(),
		"Capacity failure produced gun effects");
	bullets.Update(scene, 0.25f);
	tracers = bullets.TakeTracers();
	Require(bullets.GetCount() == 0 && tracers.size() == 2 && std::abs(tracers[0].head.z + 200) < 0.01f,
		"Lifetime did not retain the final tracer interval");
	bullets.Update(scene, 0.01f);
	Require(bullets.TakeTracers().empty(), "Expired tracer survived another frame");
	Require(fire(0.01f), "Second accepted volley failed");
	shots = bullets.TakeShotEvents();
	tracers = bullets.TakeTracers();
	Require(shots.front().sequence == 1 && tracers.size() == 2 && std::abs(tracers[0].tail.x + 1) < 0.001f &&
			std::abs(tracers[1].tail.x - 1) < 0.001f,
		"Failed volley consumed tracer phase");
	bullets.Update(scene, 0.25f);
	bullets.Configure(settings, model, {0, 0, -1});
	fire(0.01f);
	Require(
		bullets.TakeShotEvents().front().sequence == 0, "Weapon configuration did not reset tracer phase");
	bullets.Update(scene, 0.25f);

	// 即時命中でも発射通知を残し、光の筋は最初の接触点で止める
	CollisionTarget target{7, {{-10, -5, -1}, {10, 5, 1}}, {0, 20, -10}, {}};
	XMStoreFloat4x4(&target.world, XMMatrixTranslation(0, 20, -10));
	bullets.Configure(settings, model, {0, 0, -1});
	bullets.FireVolley(scene, muzzles, world, world, {0, 0, -1}, {}, 0.02f, {}, std::span{&target, 1}, 0.02f);
	const auto hits = bullets.TakeHits();
	tracers = bullets.TakeTracers();
	Require(hits.size() == 4 && bullets.TakeShotEvents().size() == 4 && bullets.GetCount() == 0 &&
			tracers.size() == 2 && std::abs(tracers[0].head.z + 9) < 0.001f,
		"Immediate contact lost flash events or drew a tracer beyond the target");
	Require(std::all_of(hits.begin(), hits.end(),
				[&](const auto& hit) {
					return hit.target == 7 && std::abs(hit.position.z - tracers.front().head.z) < 0.001f;
				}),
		"Tracer and ordinary bullet collisions differ");
	bullets.Update(scene, 0.01f);
	Require(bullets.TakeTracers().empty(), "Contact tracer persisted");
	bullets.FireVolley(scene, muzzles, world, world, {0, -1, 0}, {}, 0.05f);
	tracers = bullets.TakeTracers();
	Require(bullets.GetCount() == 0 && tracers.size() == 2 && std::abs(tracers.front().head.y) < 0.001f,
		"Sea contact did not terminate the tracer");
	settings.muzzleSpeed = 10000;
	bullets.Configure(settings, model, {0, 0, -1});
	fire(0.1f);
	tracers = bullets.TakeTracers();
	Require(std::abs(XMVectorGetX(XMVector3Length(
						 XMLoadFloat3(&tracers.front().head) - XMLoadFloat3(&tracers.front().tail))) -
				120) < 0.01f,
		"Fast tracer exceeded the maximum world length");
	bullets.Update(scene, 0.25f);
	settings.muzzleSpeed = 1000;
	bullets.Configure(settings, model, {0, 0, -1});
	const AimAssistance assistance{CombatTarget{9, {10, 20, -100}, {}, "TEST", 100}, 30};
	bullets.FireVolley(scene, muzzles, world, world, {0, 0, -1}, {20, 0, 0}, 0.05f, assistance);
	tracers = bullets.TakeTracers();
	const auto& assistedWorld = scene.GetObjects().front().worldTransform;
	Require(std::abs(tracers.front().head.x - assistedWorld._41) < 0.001f &&
			std::abs(tracers.front().head.z - assistedWorld._43) < 0.001f,
		"Assisted tracer diverged from the actual projectile");

	// FPSを変えても、同じ秒数に同じ数の発射と煙が生成される
	for (float dt : {1.0f / 30, 1.0f / 60, 1.0f / 144, 0.25f}) {
		// 演出側の密度を独立して調べるため、同じ0.1秒周期の発射時刻を渡す
		CombatEffectSystem effects;
		unsigned emitted = 0;
		const int frames = int(std::round(1 / dt));
		for (int frame = 0; frame < frames; ++frame) {
			effects.Update(dt);
			const float end = (frame + 1) * dt;
			while (emitted < 10 && emitted * 0.1f < end - 1e-6f) {
				for (size_t muzzle = 0; muzzle < 4; ++muzzle)
					effects.SpawnGunShot({muzzle, {float(emitted), 20, 0}, {40, 0, 0}, {0, 0, -1},
						end - emitted * 0.1f, emitted});
				++emitted;
			}
		}
		Require(emitted == 10 && effects.GetSnapshot().gunSmoke.size() == 12,
			"Gun smoke density changed with FPS");
	}
	CombatEffectSystem effects;
	for (size_t index = 0; index < 70; ++index)
		effects.SpawnGunShot({index % 4, {float(index), 0, 0}, {40, 0, 0}, {0, 0, -1}, 0.25f, index});
	const auto& snapshot = effects.GetSnapshot();
	Require(snapshot.gunFlashes.size() == 4 && snapshot.gunSmoke.size() == 64 &&
			snapshot.gunSmoke.front().position.x == 6,
		"Gun effect caps did not replace old entries");
	Require(snapshot.gunFlashes.front().age < GunEffectSettings::flashLifetimeSeconds,
		"Long frame hid every muzzle flash");
	std::array<XMFLOAT3, 4> movedMuzzles{{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}, {10, 11, 12}}};
	effects.SetGunMuzzlePose(movedMuzzles, {1, 0, 0});
	Require(snapshot.gunFlashes.front().position.x == 1 && snapshot.gunFlashes.front().direction.x == 1 &&
			snapshot.gunSmoke.front().position.x == 6,
		"Muzzle motion dragged emitted smoke");
	effects.ClearGunFlashes();
	Require(snapshot.gunFlashes.empty() && !snapshot.gunSmoke.empty(),
		"Player removal cleared independent smoke");
	effects.Update(0.25f);
	Require(snapshot.gunSmoke.empty(), "Gun smoke did not expire");
	effects.SetGunTracers(std::vector<GunTracerSegment>(300));
	Require(
		snapshot.gunTracers.size() == GunEffectSettings::maxTracerCount, "Tracer snapshot exceeded its cap");
	effects.Update(0.01f);
	Require(snapshot.gunTracers.empty(), "Frame-local tracer snapshot persisted");
	std::cout << "PASS: gun shot events, alternating tracers, capacity, contact, sea, lifetime, FPS and "
				 "effect ownership\n";
}

void TestGunSegmentClipping() {
	using namespace DirectX;
	Engine::RenderView view;
	view.width = 800;
	view.height = 600;
	XMStoreFloat4x4(&view.view, XMMatrixIdentity());
	XMStoreFloat4x4(&view.projection, XMMatrixPerspectiveFovLH(XM_PIDIV2, 4.0f / 3, 1, 1000));
	const HudProjector projector(view);
	Require(projector.ProjectClippedSegment({0, 0, 10}, {100, 0, 10}).has_value(),
		"Screen edge discarded entire tracer");
	Require(!projector.ProjectClippedSegment({100, 0, 10}, {200, 0, 10}), "Offscreen segment was accepted");
	Require(!projector.ProjectClippedSegment({0, 0, -10}, {1, 1, -20}), "Rear tracer was projected");
	for (const auto segment : {std::pair{XMFLOAT3{1, 0, 10}, XMFLOAT3{0, 0, -10}},
			 std::pair{XMFLOAT3{0, 0, 0.1f}, XMFLOAT3{1, 1, 10}},
			 std::pair{XMFLOAT3{-100, 0, 10}, XMFLOAT3{100, 0, 10}}}) {
		const auto clipped = projector.ProjectClippedSegment(segment.first, segment.second);
		Require(clipped.has_value(), "Visible clipped segment was lost");
		for (auto point : {clipped->head, clipped->tail})
			Require(std::isfinite(point.x) && std::isfinite(point.y) && point.x >= 0 && point.x <= 800 &&
					point.y >= 0 && point.y <= 600,
				"Clipped segment produced oversized or nonfinite screen coordinates");
	}
	Require(!projector.ProjectClippedSegment({std::numeric_limits<float>::quiet_NaN(), 0, 1}, {}),
		"Nonfinite tracer was projected");
	std::cout << "PASS: tracer screen edges, rear camera, near plane and finite clipping\n";
}
