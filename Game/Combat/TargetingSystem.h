#pragma once
#include "CombatTypes.h"

namespace Game {
	class TargetingSystem {
	public:
		void Configure(const TargetingSettings& settings);
		void Update(std::span<const CombatTarget> enemies, const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& forward, bool next, float dt);
		Engine::ObjectId GetSelected() const { return m_selected; }
		LockState GetState() const { return m_state; }
		float GetProgress() const;
	private:
		TargetingSettings m_settings;
		Engine::ObjectId m_selected = Engine::InvalidObjectId;
		LockState m_state = LockState::None;
		float m_acquired = 0, m_lost = 0;
	};
}
