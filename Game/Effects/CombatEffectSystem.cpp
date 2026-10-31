#include "CombatEffectSystem.h"
#include <algorithm>
#include <cmath>

namespace Game {
	namespace {
		template <class Effect>
		void AdvanceEffects(std::vector<Effect>& effects, float deltaSeconds, float lifetimeSeconds) {
			for (auto& effect : effects)
				effect.age += deltaSeconds;
			std::erase_if(effects, [lifetimeSeconds](const Effect& effect) {
				return effect.age >= lifetimeSeconds;
			});
		}

		template <class Effect>
		void AddBoundedEffect(std::vector<Effect>& effects, Effect effect, size_t maximumCount) {
			// 上限では最も古い演出を置き換え、新しい命中と撃破を優先する
			if (effects.size() >= maximumCount)
				effects.erase(effects.begin());
			effects.push_back(std::move(effect));
		}
	} // namespace

	void CombatEffectSystem::Update(float deltaSeconds) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0)
			return;
		const float step = std::min(deltaSeconds, CombatEffectSettings::maximumFrameSeconds);
		AdvanceEffects(m_snapshot.gunFlashes, step, GunEffectSettings::flashLifetimeSeconds);
		AdvanceEffects(m_snapshot.gunSmoke, step, GunEffectSettings::smokeLifetimeSeconds);
		m_snapshot.gunTracers.clear();
		AdvanceEffects(m_snapshot.impacts, step, ImpactEffectSettings::lifetimeSeconds);
		AdvanceEffects(m_snapshot.rocketSmoke, step, RocketEffectSettings::smokeLifetimeSeconds);
		AdvanceEffects(m_snapshot.explosions, step, ExplosionEffectSettings::lifetimeSeconds);
	}

	void CombatEffectSystem::SpawnGunShot(const GunShotEvent& shot) {
		if (shot.muzzleIndex >= GunEffectSettings::maxFlashCount || !std::isfinite(shot.age) || shot.age < 0)
			return;
		auto found =
			std::find_if(m_snapshot.gunFlashes.begin(), m_snapshot.gunFlashes.end(), [&](const auto& flash) {
				return flash.muzzleIndex == shot.muzzleIndex;
			});
		// 低FPSで閃光の寿命をまたいでも、最新の発射を一度は画面に出す
		const GunMuzzleFlash flash{shot.muzzleIndex, shot.position, shot.direction,
			std::min(shot.age, GunEffectSettings::flashLifetimeSeconds * 0.5f), shot.sequence};
		if (found == m_snapshot.gunFlashes.end())
			m_snapshot.gunFlashes.push_back(flash);
		else
			*found = flash;

		if (shot.age < GunEffectSettings::smokeLifetimeSeconds) {
			AddBoundedEffect(m_snapshot.gunSmoke, GunSmoke{shot.position, shot.aircraftVelocity, shot.age},
				GunEffectSettings::maxSmokeCount);
		}
	}

	void CombatEffectSystem::SetGunMuzzlePose(
		const std::array<DirectX::XMFLOAT3, 4>& positions, const DirectX::XMFLOAT3& forward) {
		for (auto& flash : m_snapshot.gunFlashes) {
			flash.position = positions[flash.muzzleIndex];
			flash.direction = forward;
		}
	}

	void CombatEffectSystem::ClearGunFlashes() {
		m_snapshot.gunFlashes.clear();
	}

	void CombatEffectSystem::SetGunTracers(std::vector<GunTracerSegment> tracers) {
		if (tracers.size() > GunEffectSettings::maxTracerCount)
			tracers.resize(GunEffectSettings::maxTracerCount);
		m_snapshot.gunTracers = std::move(tracers);
	}

	void CombatEffectSystem::SpawnRocketSmoke(const DirectX::XMFLOAT3& position, float age) {
		if (!std::isfinite(age) || age < 0 || age >= RocketEffectSettings::smokeLifetimeSeconds)
			return;
		AddBoundedEffect(
			m_snapshot.rocketSmoke, RocketTrailSample{position, age}, RocketEffectSettings::maxSmokeCount);
	}
	void CombatEffectSystem::SetRocketExhausts(std::vector<RocketExhaust> exhausts) {
		if (exhausts.size() > RocketEffectSettings::maxExhaustCount)
			exhausts.resize(RocketEffectSettings::maxExhaustCount);
		m_snapshot.rocketExhausts = std::move(exhausts);
	}

	void CombatEffectSystem::SpawnImpact(const HitEvent& hit, const DirectX::XMFLOAT3& targetVelocity) {
		AddBoundedEffect(m_snapshot.impacts, ImpactEffect{hit.position, 0, targetVelocity, hit.direction},
			ImpactEffectSettings::maxCount);
	}

	void CombatEffectSystem::SpawnExplosion(
		const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity) {
		AddExplosion({position, velocity, 0, ExplosionEffectSettings::minRadius});
	}

	void CombatEffectSystem::SpawnExplosion(
		const Engine::SceneObject& aircraft, const DirectX::XMFLOAT3& velocity, float visualScale) {
		using namespace DirectX;
		ExplosionEffect explosion{
			{aircraft.worldTransform._41, aircraft.worldTransform._42, aircraft.worldTransform._43},
			velocity};
		if (aircraft.model) {
			if (const auto bounds = aircraft.model->GetLocalBounds()) {
				// 描画と同じ変換で中心を求め、機体の大きさに爆発半径を合わせる
				const auto center = bounds->Center();
				XMStoreFloat3(&explosion.position,
					XMVector3TransformCoord(XMLoadFloat3(&center), XMLoadFloat4x4(&aircraft.worldTransform)));
				const auto extent = XMLoadFloat3(&bounds->maximum) - XMLoadFloat3(&bounds->minimum);
				explosion.radius = std::clamp(XMVectorGetX(XMVector3Length(extent)) * visualScale *
						ExplosionEffectSettings::boundsRadiusScale,
					ExplosionEffectSettings::minRadius, ExplosionEffectSettings::maxRadius);
			}
		}
		AddExplosion(explosion);
	}

	void CombatEffectSystem::AddExplosion(ExplosionEffect explosion) {
		AddBoundedEffect(m_snapshot.explosions, std::move(explosion), ExplosionEffectSettings::maxCount);
	}
} // namespace Game
