#pragma once
#include "BombFlight.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {

	// 投下爆弾の生成・自由落下・命中を管理する。飛行中の実体はSceneにも登録する
	class BombSystem {
	  public:
		// reloadSecondsは1～120秒。飛行中の弾がある状態での再設定は許可しない
		void Configure(
			float reloadSeconds, std::shared_ptr<Engine::Model> model, const DirectX::XMFLOAT3& modelForward);
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
			return m_bombs.size();
		}
		// 命中と外れ着弾の通知はいずれも取得時に消費する
		std::vector<HitEvent> TakeHits();
		std::vector<DirectX::XMFLOAT3> TakeMissImpacts();

	  private:
		struct Bomb {
			Engine::ObjectId id;
			BombFlight flight;
			float remainingSeconds = BombTuning::lifetimeSeconds;
		};
		std::shared_ptr<Engine::Model> m_model;
		DirectX::XMFLOAT3 m_modelForward{1, 0, 0};
		float m_reloadSeconds = 10;
		float m_cooldownSeconds = 0;
		std::vector<Bomb> m_bombs;
		std::vector<HitEvent> m_hits;
		std::vector<DirectX::XMFLOAT3> m_missImpacts;
	};
} // namespace Game
