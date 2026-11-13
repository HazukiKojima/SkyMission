#pragma once
#include "CombatEffectSettings.h"
#include "CombatEffectTypes.h"
#include "Game/Combat/CombatTypes.h"
#include <array>
#include <span>

namespace Game {
	// 戦闘イベントから演出を生成し、寿命と個数上限を管理する。ダメージ判定は行わない
	class CombatEffectSystem {
	  public:
		void Configure(const EffectSettings& settings) {
			m_settings = settings;
		}
		const EffectSettings& GetSettings() const {
			return m_settings;
		}
		// 経過秒数を進め、寿命を終えた演出を除去する
		void Update(float deltaSeconds);
		void SpawnGunShot(const GunShotEvent& shot);
		void SetGunMuzzlePose(
			const std::array<DirectX::XMFLOAT3, 4>& positions, const DirectX::XMFLOAT3& forward);
		void ClearGunFlashes();
		void SetGunTracers(std::vector<GunTracerSegment> tracers);
		void SpawnRocketSmoke(const DirectX::XMFLOAT3& position, float age);
		void SpawnCarrierExplosion(const DirectX::XMFLOAT3& position);
		void SpawnDeckSmoke(const DirectX::XMFLOAT3& position);
		void SetRocketExhausts(std::vector<RocketExhaust> exhausts);
		void SpawnImpact(const HitEvent& hit, const DirectX::XMFLOAT3& targetVelocity);
		void SpawnExplosion(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity);
		void SpawnExplosion(
			const Engine::SceneObject& aircraft, const DirectX::XMFLOAT3& velocity, float visualScale);
		// 内部の演出状態への参照。更新・生成操作で内容が変わるため描画するフレーム内で使う
		const CombatEffectSnapshot& GetSnapshot() const {
			return m_snapshot;
		}

	  private:
		void AddExplosion(ExplosionEffect explosion);
		EffectSettings m_settings;
		CombatEffectSnapshot m_snapshot;
	};
} // namespace Game
