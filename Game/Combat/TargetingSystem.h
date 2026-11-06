#pragma once
#include "CombatTypes.h"

namespace Game {
	// 選択対象とロック捕捉・維持・喪失猶予を管理する。描画や発砲は行わない
	class TargetingSystem {
	  public:
		void Configure(const TargetingSettings& settings);
		// 位置・前方はワールド座標、deltaSecondsは秒。cycleRequestedは対象切替の押下イベントとして渡す
		// enemiesへの参照は保持せず、継続する対象はObjectIdで識別する
		void Update(std::span<const CombatTarget> enemies, const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& forward, bool cycleRequested, float deltaSeconds);
		Engine::ObjectId GetSelected() const {
			return m_selected;
		}
		LockState GetState() const {
			return m_state;
		}
		// 捕捉の進行率を0～1で返す。ロック完了時は1
		float GetProgress() const;

	  private:
		std::vector<const CombatTarget*> CollectCandidates(
			std::span<const CombatTarget> enemies, const DirectX::XMFLOAT3& position) const;
		const CombatTarget* SelectTarget(std::span<const CombatTarget* const> candidates,
			const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& forward, bool cycleRequested);
		void UpdateLock(const CombatTarget* target, const DirectX::XMFLOAT3& position,
			const DirectX::XMFLOAT3& forward, float deltaSeconds);

		TargetingSettings m_settings;
		Engine::ObjectId m_selected = Engine::InvalidObjectId;
		LockState m_state = LockState::None;
		float m_acquiredSeconds = 0;
		float m_lostSeconds = 0;
	};
} // namespace Game
