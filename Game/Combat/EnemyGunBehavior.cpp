#include "EnemyGunBehavior.h"

namespace Game {
	float EnemyVariation(size_t slot, uint32_t event, uint32_t salt) {
		uint32_t value = uint32_t(slot) * 747796405u + event * 2891336453u + salt * 277803737u;
		value ^= value >> 16;
		value *= 2246822519u;
		value ^= value >> 13;
		return float(value & 0x00ffffffu) / 16777216.0f;
	}

	bool EnemyGunPattern::Update(float dt, size_t slot, const EnemyGunTuning& tuning) {
		if (!std::isfinite(dt) || dt <= 0) return false;

		// 機体ごとに射撃開始をずらす
		if (!m_initialized) {
			m_remaining =
				tuning.initialDelaySeconds + EnemyVariation(slot, 0, 11) * tuning.initialDelayRangeSeconds;
			m_initialized = true;
		}

		// 個体差を付けて連射と休止を切り替える
		m_remaining -= std::min(dt, 0.25f);

		if (m_remaining <= 0) {
			m_burst = !m_burst;
			const float variation = EnemyVariation(slot, ++m_sequence, 23);
			m_remaining += m_burst ? tuning.burstSeconds + variation * tuning.burstRangeSeconds
								   : tuning.pauseSeconds + variation * tuning.pauseRangeSeconds;
		}
		return m_burst;
	}

	DirectX::XMFLOAT3 EnemyGunDirection(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& velocity,
		const CombatTarget& target, float speed, size_t slot, uint32_t shot, const EnemyGunTuning& tuning) {
		using namespace DirectX;

		auto direction = XMLoadFloat3(&target.center) - XMLoadFloat3(&position);

		if (XMVectorGetX(XMVector3LengthSq(direction)) < 0.0001f) return {0, 0, -1};

		direction = XMVector3Normalize(direction);

		if (const auto aim = SolveGunAim(position, velocity, target, speed, tuning.lifetimeSeconds)) {
			// 発射時だけ予測位置へ狙いを寄せる。発射後の弾は誘導しない
			const float lead =
				tuning.minimumLeadFraction + EnemyVariation(slot, shot, 41) * tuning.leadVariation;
			direction = XMVector3Normalize(XMVectorLerp(direction, XMLoadFloat3(&aim->direction), lead));
		}

		// 予測した射線に角度誤差を加える
		const float yaw =
			XMConvertToRadians((EnemyVariation(slot, shot, 53) * 2 - 1) * tuning.yawErrorDegrees);
		const float pitch =
			XMConvertToRadians((EnemyVariation(slot, shot, 67) * 2 - 1) * tuning.pitchErrorDegrees);
		auto right = XMVector3Cross(direction, XMVectorSet(0, 1, 0, 0));

		if (XMVectorGetX(XMVector3LengthSq(right)) < 0.0001f) right = XMVectorSet(1, 0, 0, 0);
		direction =
			XMVector3TransformNormal(direction, XMMatrixRotationAxis(XMVector3Normalize(right), pitch));
		direction = XMVector3TransformNormal(direction, XMMatrixRotationY(yaw));
		XMFLOAT3 result;
		XMStoreFloat3(&result, XMVector3Normalize(direction));
		return result;
	}

	std::array<DirectX::XMFLOAT3, 2> SpreadEnemyGunDirections(
		const DirectX::XMFLOAT3& center, size_t slot, uint32_t shot, const EnemyGunTuning& tuning) {
		using namespace DirectX;

		const auto forward = XMVector3Normalize(XMLoadFloat3(&center));
		auto right = XMVector3Cross(forward, XMVectorSet(0, 1, 0, 0));

		if (XMVectorGetX(XMVector3LengthSq(right)) < 0.0001f) right = XMVectorSet(1, 0, 0, 0);
		right = XMVector3Normalize(right);
		const auto up = XMVector3Normalize(XMVector3Cross(right, forward));

		// 射線に垂直な平面で散布量を決める
		const float angle = EnemyVariation(slot, shot, 83) * XM_2PI;
		const float spread = XMConvertToRadians(
			tuning.minimumSpreadDegrees + EnemyVariation(slot, shot, 97) * tuning.spreadRangeDegrees);
		const auto offset = (right * std::cos(angle) + up * std::sin(angle)) * std::tan(spread);
		std::array<XMFLOAT3, 2> directions;

		// 中心を挟んだ2方向へ散らす。弾の進行方向は発射時に確定する
		XMStoreFloat3(&directions[0], XMVector3Normalize(forward + offset));
		XMStoreFloat3(&directions[1], XMVector3Normalize(forward - offset));
		return directions;
	}
} // namespace Game
