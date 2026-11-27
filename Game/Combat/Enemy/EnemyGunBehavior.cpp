#include "EnemyGunBehavior.h"
#include "Game/Settings/SimulationTiming.h"

namespace Game {
	namespace {
		constexpr float kMinimumAimDirectionLengthSquared = 0.0001f;
		constexpr float kMinimumAimRightLengthSquared = 0.0001f;
		constexpr uint32_t kInitialDelaySalt = 11;
		constexpr uint32_t kBurstTimingSalt = 23;
		constexpr uint32_t kAimLeadSalt = 41;
		constexpr uint32_t kAimYawSalt = 53;
		constexpr uint32_t kAimPitchSalt = 67;
		constexpr uint32_t kSpreadAzimuthSalt = 83;
		constexpr uint32_t kSpreadAngleSalt = 97;
	}
	float EnemyVariation(size_t slot, uint32_t event, uint32_t salt) {
		// 機体と射撃番号から再現可能なばらつきを作る
		constexpr uint32_t kSlotMultiplier = 747796405u;
		constexpr uint32_t kEventMultiplier = 2891336453u;
		constexpr uint32_t kSaltMultiplier = 277803737u;
		constexpr uint32_t kMixMultiplier = 2246822519u;
		constexpr unsigned kFirstMixShift = 16;
		constexpr unsigned kSecondMixShift = 13;
		constexpr uint32_t kSampleRange = 1u << 24;
		uint32_t value = uint32_t(slot) * kSlotMultiplier + event * kEventMultiplier + salt * kSaltMultiplier;
		value ^= value >> kFirstMixShift;
		value *= kMixMultiplier;
		value ^= value >> kSecondMixShift;
		return float(value & (kSampleRange - 1)) / float(kSampleRange);
	}

	bool EnemyGunPattern::Update(float dt, size_t slot, const EnemyGunTuning& tuning) {
		if (!std::isfinite(dt) || dt <= 0) return false;

		// 機体ごとに射撃開始をずらす
		if (!m_initialized) {
			m_remaining =
				tuning.initialDelaySeconds + EnemyVariation(slot, 0, kInitialDelaySalt) * tuning.initialDelayRangeSeconds;
			m_initialized = true;
		}

		// 個体差を付けて連射と休止を切り替える
		m_remaining -= std::min(dt, kMaximumSimulationDeltaSeconds);

		if (m_remaining <= 0) {
			m_burst = !m_burst;
			const float variation = EnemyVariation(slot, ++m_sequence, kBurstTimingSalt);
			m_remaining += m_burst ? tuning.burstSeconds + variation * tuning.burstRangeSeconds
								   : tuning.pauseSeconds + variation * tuning.pauseRangeSeconds;
		}
		return m_burst;
	}

	DirectX::XMFLOAT3 EnemyGunDirection(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity,
		const CombatTarget& target, float speed, size_t slot, uint32_t shot, const EnemyGunTuning& tuning) {
		using namespace DirectX;

		auto direction = XMLoadFloat3(&target.center) - XMLoadFloat3(&position);

		if (XMVectorGetX(XMVector3LengthSq(direction)) < kMinimumAimDirectionLengthSquared) return {0, 0, -1};

		direction = XMVector3Normalize(direction);

		if (const auto aim = SolveGunAim(position, velocity, target, speed, tuning.lifetimeSeconds)) {
			// 発射時だけ予測位置へ狙いを寄せる。発射後の弾は誘導しない
			const float lead =
				tuning.minimumLeadFraction + EnemyVariation(slot, shot, kAimLeadSalt) * tuning.leadVariation;
			direction = XMVector3Normalize(XMVectorLerp(direction, XMLoadFloat3(&aim->direction), lead));
		}

		// 予測した射線に角度誤差を加える
		const float yaw =
			XMConvertToRadians((EnemyVariation(slot, shot, kAimYawSalt) * 2 - 1) * tuning.yawErrorDegrees);
		const float pitch =
			XMConvertToRadians((EnemyVariation(slot, shot, kAimPitchSalt) * 2 - 1) * tuning.pitchErrorDegrees);
		auto right = XMVector3Cross(direction, XMVectorSet(0, 1, 0, 0));

		if (XMVectorGetX(XMVector3LengthSq(right)) < kMinimumAimRightLengthSquared) right = XMVectorSet(1, 0, 0, 0);
		direction =
			XMVector3TransformNormal(direction, XMMatrixRotationAxis(XMVector3Normalize(right), pitch));
		direction = XMVector3TransformNormal(direction, XMMatrixRotationY(yaw));
		XMFLOAT3 result;
		XMStoreFloat3(&result, XMVector3Normalize(direction));
		return result;
	}

	std::array<DirectX::XMFLOAT3, kEnemyGunVolleySize> SpreadEnemyGunDirections(
		const DirectX::XMFLOAT3& center, size_t slot, uint32_t shot, const EnemyGunTuning& tuning) {
		using namespace DirectX;

		const auto forward = XMVector3Normalize(XMLoadFloat3(&center));
		auto right = XMVector3Cross(forward, XMVectorSet(0, 1, 0, 0));

		if (XMVectorGetX(XMVector3LengthSq(right)) < kMinimumAimRightLengthSquared) right = XMVectorSet(1, 0, 0, 0);
		right = XMVector3Normalize(right);
		const auto up = XMVector3Normalize(XMVector3Cross(right, forward));

		// 射線に垂直な平面で散布量を決める
		const float angle = EnemyVariation(slot, shot, kSpreadAzimuthSalt) * XM_2PI;
		const float spread = XMConvertToRadians(
			tuning.minimumSpreadDegrees + EnemyVariation(slot, shot, kSpreadAngleSalt) * tuning.spreadRangeDegrees);
		const auto offset = (right * std::cos(angle) + up * std::sin(angle)) * std::tan(spread);
		std::array<XMFLOAT3, kEnemyGunVolleySize> directions;

		// 中心を挟んだ2方向へ散らす。弾の進行方向は発射時に確定する
		XMStoreFloat3(&directions[0], XMVector3Normalize(forward + offset));
		XMStoreFloat3(&directions[1], XMVector3Normalize(forward - offset));
		return directions;
	}
} // namespace Game
