#pragma once
#include "Game/Weapons/BombSystem.h"

void TestBombSystem() {
	using namespace DirectX;
	const auto model = std::make_shared<Model>();
	Scene scene;
	BombSystem bombs;
	bombs.Configure(10, model, {1, 0, 0});
	Require(bombs.TryDrop(scene, {10, 1000, 20}, {200, 20, -300}), "Ready bomb did not drop");
	Require(!bombs.TryDrop(scene, {}, {}), "Second click bypassed bomb reload");
	Require(scene.GetObjects().front().worldTransform._42 == 1000 - BombTuning::releaseOffsetMeters,
		"Actual bomb release disagrees with HUD release position");
	const auto expected = BombSystem::ReleaseFlight({10, 1000, 20}, {200, 20, -300});
	bombs.Update(scene, 0.25f, std::nullopt);
	auto advanced = expected;
	advanced.Advance(0.25f);
	const auto& world = scene.GetObjects().front().worldTransform;
	Require(std::abs(world._41 - advanced.position.x) < 0.001f &&
			std::abs(world._42 - advanced.position.y) < 0.001f &&
			std::abs(world._43 - advanced.position.z) < 0.001f,
		"Bomb system lost inherited velocity or gravity");
	const float cooldown = bombs.GetCooldownSeconds();
	for (float invalid : {0.0f, -1.0f, std::numeric_limits<float>::quiet_NaN()})
		bombs.Update(scene, invalid, std::nullopt);
	Require(bombs.GetCooldownSeconds() == cooldown, "Invalid delta advanced bomb reload");
	for (int i = 0; i < 39; ++i)
		bombs.Update(scene, 0.25f, std::nullopt);
	Require(bombs.GetCooldownSeconds() == 0 && bombs.TryDrop(scene, {0, 1000, 0}, {}),
		"Bomb did not reload at the ten-second boundary");
	Require(bombs.GetCount() <= BombSystem::RequiredModelCapacity(10), "Bomb capacity reservation too small");

	Scene deckScene;
	BombSystem deckBombs;
	deckBombs.Configure(10, model, {1, 0, 0});
	CollisionTarget deck{77, {{-10, -1, -10}, {10, 1, 10}}, {0, 50, 0}, {}};
	XMStoreFloat4x4(&deck.world, XMMatrixRotationY(0.7f) * XMMatrixTranslation(0, 50, 0));
	deckBombs.TryDrop(deckScene, {0, 100, 0}, {});
	for (int i = 0; i < 20; ++i)
		deckBombs.Update(deckScene, 0.25f, deck);
	const auto hits = deckBombs.TakeHits();
	Require(hits.size() == 1 && hits[0].target == 77 && hits[0].kind == ProjectileKind::Bomb &&
			hits[0].source == AttackSource::Player && deckBombs.GetCount() == 0 &&
			deckScene.GetObjects().empty(),
		"Deck hit did not remove bomb or identify its attack source");
	Require(deckBombs.TakeHits().empty() && deckBombs.TakeMissImpacts().empty(),
		"Deck hit was emitted twice or treated as a sea miss");

	Scene seaScene;
	BombSystem seaBombs;
	seaBombs.Configure(1, model, {1, 0, 0});
	seaBombs.TryDrop(seaScene, {0, 20, 0}, {});
	for (int i = 0; i < 10; ++i)
		seaBombs.Update(seaScene, 0.25f, std::nullopt);
	Require(seaBombs.GetCount() == 0 && seaScene.GetObjects().empty() &&
			seaBombs.TakeMissImpacts().size() == 1 && seaBombs.TakeMissImpacts().empty(),
		"Sea impact did not expire bomb exactly once");
	seaBombs.TryDrop(seaScene, {0, 1000000, 0}, {-200, 0, 0});
	const auto& reverse = seaScene.GetObjects().front().worldTransform;
	Require(reverse._11 < -0.99f && std::isfinite(reverse._22),
		"Antiparallel bomb axis produced an invalid render pose");
	for (int i = 0; i < int(BombTuning::lifetimeSeconds * 4); ++i)
		seaBombs.Update(seaScene, 0.25f, std::nullopt);
	Require(seaBombs.GetCount() == 0 && seaScene.GetObjects().empty(), "Bomb exceeded maximum lifetime");
	std::cout
		<< "PASS: bomb lifecycle, release/prediction agreement, reload, deck/sea hits, pose and lifetime\n";
}
