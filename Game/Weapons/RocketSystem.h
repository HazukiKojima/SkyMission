#pragma once
#include "RocketDefinition.h"
#include "Game/Combat/CombatTypes.h"
#include "Game/Effects/CombatEffectTypes.h"

namespace Game {
	struct RocketState {
		Engine::ObjectId objectId = Engine::InvalidObjectId, targetId = Engine::InvalidObjectId;
		DirectX::XMFLOAT3 position{}, velocity{};
		float remainingLifetime = 0;
		float trailCountdown = 0;
	};
	class RocketSystem {
	  public:
		void Configure(const RocketSettings& settings, std::shared_ptr<Engine::Model> model,
			const DirectX::XMFLOAT3& modelForward);
		bool Fire(Engine::Scene& scene, const DirectX::XMFLOAT3& muzzle,
			const DirectX::XMFLOAT4X4& aircraftWorld, const DirectX::XMFLOAT3& forward,
			const DirectX::XMFLOAT3& aircraftVelocity, Engine::ObjectId targetId);
		void Update(Engine::Scene& scene, float deltaSeconds,
			std::span<const CollisionTarget> collisions = {}, std::span<const CombatTarget> targets = {});
		std::vector<HitEvent> TakeHits() {
			auto result = std::move(m_hits);
			m_hits.clear();
			return result;
		}
		std::vector<RocketTrailSample> TakeTrail() {
			auto result = std::move(m_trail);
			m_trail.clear();
			return result;
		}
		std::vector<RocketExhaust> GetExhausts() const;
		const std::vector<RocketState>& GetRockets() const {
			return m_rockets;
		}
		size_t GetCount() const {
			return m_rockets.size();
		}
		static DirectX::XMFLOAT3 Steer(const DirectX::XMFLOAT3& velocity,
			const DirectX::XMFLOAT3& desiredDirection, float maximumRadians);

	  private:
		bool Advance(RocketState& rocket, float deltaSeconds, std::span<const CollisionTarget> collisions,
			std::span<const CombatTarget> targets);
		DirectX::XMFLOAT4X4 MakeWorld(const RocketState& rocket) const;
		DirectX::XMFLOAT3 Tail(const RocketState& rocket) const;
		RocketSettings m_settings;
		std::shared_ptr<Engine::Model> m_model;
		DirectX::XMFLOAT3 m_modelForward{1, 0, 0};
		float m_tailDistance = 6;
		std::vector<RocketState> m_rockets;
		std::vector<HitEvent> m_hits;
		std::vector<RocketTrailSample> m_trail;
	};
} // namespace Game
