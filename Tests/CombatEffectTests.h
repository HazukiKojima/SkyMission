#pragma once
#include "Game/Effects/CombatEffectSystem.h"

void TestCombatEffectLifetime() {
	CombatEffectSystem effects;
	HitEvent hit{1, 0, {1, 2, 3}, {0, 0, -1}};
	const DirectX::XMFLOAT3 velocity{10, 20, 30};
	effects.SpawnImpact(hit, velocity);
	effects.SpawnExplosion(hit.position, velocity);
	const auto& snapshot = effects.GetSnapshot();
	Require(snapshot.impacts.front().velocity.z == 30 && snapshot.impacts.front().direction.z == -1,
		"Impact lost inherited velocity or incoming direction");

	for (float invalid :
		{0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
		effects.Update(invalid);
	Require(snapshot.impacts.front().age == 0 && snapshot.explosions.front().age == 0,
		"Invalid delta advanced effects");
	effects.Update(10);
	Require(snapshot.impacts.front().age == 0.25f, "Effect update did not clamp long frame");
	effects.Update(0.20f);
	Require(snapshot.impacts.empty() && snapshot.explosions.size() == 1,
		"Impact lifetime was not independent from explosion");
	for (int frame = 0; frame < 13; ++frame)
		effects.Update(0.25f);
	Require(snapshot.explosions.empty(), "Expired explosion was retained");

	for (size_t i = 0; i < ImpactEffectSettings::maxCount + 1; ++i) {
		hit.position.x = static_cast<float>(i);
		effects.SpawnImpact(hit, velocity);
	}
	Require(
		snapshot.impacts.size() == ImpactEffectSettings::maxCount && snapshot.impacts.front().position.x == 1,
		"Impact limit did not replace oldest entry");
	for (size_t i = 0; i < ExplosionEffectSettings::maxCount + 1; ++i)
		effects.SpawnExplosion({static_cast<float>(i), 0, 0}, velocity);
	Require(snapshot.explosions.size() == ExplosionEffectSettings::maxCount &&
			snapshot.explosions.front().position.x == 1,
		"Explosion limit did not replace oldest entry");

	std::cout
		<< "PASS: effect lifetime, invalid delta, long frames, inherited velocity and oldest replacement\n";
}
