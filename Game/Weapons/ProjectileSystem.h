#pragma once
#include "Engine/Scene/Scene.h"
#include "WeaponDefinition.h"
#include "Game/Combat/CombatTypes.h"
#include "GunShotEvent.h"

namespace Game {
	// 弾の寿命・等速移動・連続衝突判定を管理し、命中と描画用イベントを蓄積する
	class ProjectileSystem {
	  public:
		void Configure(const GunSettings& settings, std::shared_ptr<Engine::Model> model,
			const DirectX::XMFLOAT3& modelForward);
		// dtは秒。命中・寿命切れ・海面到達した弾をSceneからも取り除く
		void Update(Engine::Scene& scene, float dt, std::span<const CollisionTarget> targets = {});
		bool FireSingle(Engine::Scene& scene, const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& velocity, AttackSource source, float damage);
		// ageはフレーム末までに弾が経過した秒数。新規弾もその時間だけ進めて衝突判定する
		// muzzlesはモデル座標、forwardとaircraftVelocityはワールド座標系
		bool FireVolley(Engine::Scene& scene, const std::array<DirectX::XMFLOAT3, 4>& muzzles,
			const DirectX::XMFLOAT4X4& aircraftWorld, const DirectX::XMFLOAT4X4& aircraftPose,
			const DirectX::XMFLOAT3& forward, const DirectX::XMFLOAT3& aircraftVelocity, float age,
			const AimAssistance& assistance = {}, std::span<const CollisionTarget> targets = {},
			float frameSeconds = 0, float damage = 10);
		// 未取得の命中通知を返して内部キューを空にする。同じ通知は二度取得できない
		std::vector<HitEvent> TakeHits() {
			auto hits = std::move(m_hits);
			m_hits.clear();
			return hits;
		}
		size_t GetCount() const {
			return m_projectiles.size();
		}
		// 発射通知と曳光線も取得時に消費する。演出側への引き渡しは1フレームに一度行う
		std::vector<GunShotEvent> TakeShotEvents();
		std::vector<GunTracerSegment> TakeTracers();

	  private:
		struct Projectile {
			Engine::ObjectId objectId;
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT3 velocity;
			float remainingLifetime;
			DirectX::XMFLOAT3 birthPosition{};
			bool isTracer = false;
			AttackSource source = AttackSource::Player;
			float damage = 0;
		};
		static void Advance(Projectile& projectile, float dt);
		void AddTracer(const Projectile& projectile, const DirectX::XMFLOAT3& head);
		bool MoveAndCollide(Projectile& projectile, float duration, float frameOffset, float frameSeconds,
			std::span<const CollisionTarget> targets);
		GunSettings m_settings;
		std::shared_ptr<Engine::Model> m_model;
		DirectX::XMFLOAT4X4 m_modelRotation;
		std::vector<Projectile> m_projectiles;
		std::vector<HitEvent> m_hits;
		std::vector<GunShotEvent> m_shotEvents;
		std::vector<GunTracerSegment> m_tracers;
		std::uint64_t m_volleySequence = 0;
	};
} // namespace Game
