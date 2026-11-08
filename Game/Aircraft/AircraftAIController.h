#pragma once

#include "AircraftFlightModel.h"
#include <cstddef>
#include <cstdint>
#include <optional>

namespace Game {
	struct AircraftAIParameters {
		float directionChangeSeconds = 5.0f;
		float altitudeChangeSeconds = 12.0f;
		float lowAltitude = 100.0f;
		float highAltitude = 400.0f;
		float pitchLookAheadDistance = 800.0f;
		float turnGain = 1.5f;
		float pitchGain = 1.5f;
		float speedMatchGain = 0.015f;
		float highGManeuverIntervalSeconds = 24.0f;
		float highGManeuverDurationSeconds = 3.0f;
		float reactionSeconds = 0.3f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float maneuverSeconds = 1.8f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float disengageDistance = 350.0f;
		float threatDistance = 700.0f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float attackDistance = 3000.0f;
		float aimAngleDegrees = 3.0f;
		float aimHoldSeconds = 0.4f;
		float openingGraceSeconds = 3.0f;
		float aimErrorDegrees = 0.65f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float cruiseSpeed = 340.0f;
		float attackSpeed = 260.0f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float escapeSpeed = 520.0f;
		float reengageDistance = 2500.0f;
		float attackRunSeconds = 7.0f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float separationLookAheadSeconds = 0.8f;
		float tailDistance = 1000.0f;
		float flankDistance = 650.0f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float pursuitPredictionSeconds = 1.5f;
		float maximumDisengageSeconds = 3.0f; // 旧設定との互換用。現在の戦闘AIでは使用しない
		float repositionSeconds = 12.0f;	  // 旧設定との互換用。現在の戦闘AIでは使用しない
	};
	enum class AircraftCombatMode { Pursuit, Attack, Disengage, Evade, Recover, Reengage, ReturnToCarrier };
	struct AircraftAITarget {
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 velocity{};
		DirectX::XMFLOAT3 forward{0, 0, -1};
	};
	struct AircraftAIDecision {
		AircraftInput flight;
		bool fire = false;
		bool locking = false;
		float lockProgress = 0;
	};

	// 敵機ごとに保持する操縦判断。状態遷移と照準保持時間を持つため機体間で共有しない
	class AircraftAIController {
	  public:
		// 射撃後は照準の保持時間だけをリセットし、攻撃航路は継続する
		void NotifyWeaponFired() {
			m_aimTime = 0;
		}
		// dtは秒。slotで機体ごとの行動差を固定し、defenseCenter指定時は防衛範囲へ帰還させる
		// 操縦・射撃の要求を返すだけで、飛行モデルの更新や弾の生成は呼び出し側が行う
		AircraftAIDecision ReadCombatInput(const AircraftFlightModel& aircraft,
			const AircraftAITarget& target, const DirectX::XMFLOAT3& aimDirection, float dt,
			const AircraftAIParameters& p, size_t slot, bool attackAllowed, bool weaponReady = true,
			std::optional<DirectX::XMFLOAT3> defenseCenter = std::nullopt, bool opportunisticGun = false);
		// 非戦闘時は一定間隔で方位と高度を変える操縦入力を返す
		AircraftInput ReadInput(const AircraftFlightModel& aircraft, float targetSpeed, float deltaSeconds,
			const AircraftAIParameters& parameters);

		AircraftCombatMode GetMode() const {
			return m_mode;
		}

	  private:
		void EnterMode(AircraftCombatMode mode);
		void ChooseTurn(float yaw, DirectX::FXMVECTOR direction, size_t slot);
		void BeginDisengagement(const AircraftFlightModel& aircraft, const AircraftAITarget& target,
			const AircraftAIParameters& parameters, size_t slot,
			std::optional<DirectX::XMFLOAT3> defenseCenter);
		AircraftInput BuildFlightInput(const AircraftFlightModel& aircraft, const AircraftAITarget& target,
			const AircraftAIParameters& parameters, size_t slot, float distance,
			bool imminentCollision) const;

		// 戦闘中の操縦・照準・離脱状態
		AircraftCombatMode m_mode = AircraftCombatMode::Pursuit;
		DirectX::XMFLOAT3 m_steering{0, 0, -1};
		DirectX::XMFLOAT3 m_escapeDirection{0, 0, -1};
		float m_flankSide = 1;
		bool m_returning = false;
		bool m_collisionAvoidance = false;
		bool m_spacingBreak = false;
		float m_combatTime = 0;
		float m_modeSeconds = 0;
		float m_continuousTurnSeconds = 0;
		float m_aimTime = 0;
		float m_passMinimum = 3;
		float m_passMaximum = 9;
		float m_passLeg = 4500;
		float m_turnBurstSeconds = 2.5f;
		float m_reentrySeconds = 1.5f;
		float m_avoidanceRecheck = 0;
		DirectX::XMFLOAT3 m_passStart{};
		DirectX::XMFLOAT3 m_reentryOffset{};
		uint32_t m_passSequence = 0;

		// 非戦闘時の方位・高度の切り替え
		float m_elapsedSeconds = 0.0f;
		float m_nextDirectionChange = 0.0f;
		float m_nextAltitudeChange = 0.0f;
		float m_targetYaw = 0.0f;
		float m_targetAltitude = 0.0f;
		std::size_t m_directionChangeIndex = 0;
		bool m_initialized = false;
	};
} // namespace Game
