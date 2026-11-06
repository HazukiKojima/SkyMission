#pragma once
#include "BombFlight.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {

	// 投下爆弾と母艦攻撃用ミサイルを管理する。飛行中の実体はSceneにも登録する
	class BombSystem {
	  public:
		// reloadSecondsは1～120秒。飛行中の弾がある状態での再設定は許可しない
		void Configure(
			float reloadSeconds, std::shared_ptr<Engine::Model> model, const DirectX::XMFLOAT3& modelForward);
		void ConfigureMissiles(float speed, float damage);
		// モデル設定済み・ミサイルモード・残弾あり・再装填待ちなしの場合に発射してtrue
		// ロック状態の確認は呼び出し側で行い、targetには誘導先のIDを渡す
		bool TryLaunch(Engine::Scene& scene, const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& velocity, const DirectX::XMFLOAT3& forward, Engine::ObjectId target);
		unsigned GetAmmo() const {
			return m_ammo;
		}
		// 航跡サンプルをまとめて取り出し、内部の未取得分を消去する
		std::vector<DirectX::XMFLOAT3> TakeTrail() {
			auto result = std::move(m_trail);
			m_trail.clear();
			return result;
		}
		void Update(Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& deck);
		bool TryDrop(Engine::Scene& scene, const DirectX::XMFLOAT3& aircraftPosition,
			const DirectX::XMFLOAT3& aircraftVelocity);
		// 機体の下方へ投下位置をずらし、機体速度を引き継いだ初期状態を作る
		static BombFlight ReleaseFlight(DirectX::XMFLOAT3 position, const DirectX::XMFLOAT3& velocity);
		static size_t RequiredModelCapacity(float reloadSeconds);
		float GetCooldownSeconds() const {
			return m_cooldownSeconds;
		}
		size_t GetCount() const {
			return m_bombs.size() + m_missiles.size();
		}
		// 命中と外れ着弾の通知はいずれも取得時に消費する
		std::vector<HitEvent> TakeHits();
		std::vector<DirectX::XMFLOAT3> TakeMissImpacts();

	  private:
		struct Missile {
			Engine::ObjectId id, target;
			CarrierMissileFlight flight;
			float remaining = 8, smokeSeconds = 0;
		};
		void UpdateMissiles(Engine::Scene& scene, float dt, const std::optional<CollisionTarget>& hull);
		bool m_missileMode = false;
		float m_missileSpeed = 6000, m_missileDamage = 150;
		unsigned m_ammo = 2;
		std::vector<Missile> m_missiles;
		std::vector<DirectX::XMFLOAT3> m_trail;
		struct Bomb {
			Engine::ObjectId id;
			BombFlight flight;
			float remainingSeconds = BombTuning::lifetimeSeconds;
		};
		DirectX::XMFLOAT4X4 BuildTransform(const BombFlight& flight) const;
		std::shared_ptr<Engine::Model> m_model;
		DirectX::XMFLOAT3 m_modelForward{1, 0, 0};
		float m_reloadSeconds = 10, m_cooldownSeconds = 0;
		std::vector<Bomb> m_bombs;
		std::vector<HitEvent> m_hits;
		std::vector<DirectX::XMFLOAT3> m_missImpacts;
	};
} // namespace Game
