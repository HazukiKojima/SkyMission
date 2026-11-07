#pragma once
#include "GunAimSolver.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Game {
	inline constexpr float EnemyGunLifetimeSeconds = 3.0f;

	inline float EnemyVariation(size_t slot, uint32_t event, uint32_t salt) {
		uint32_t value = uint32_t(slot) * 747796405u + event * 2891336453u + salt * 277803737u;
		value ^= value >> 16;
		value *= 2246822519u;
		value ^= value >> 13;
		return float(value & 0x00ffffffu) / 16777216.0f;
	}

	class EnemyGunPattern {
	  public:
		bool Update(float dt, size_t slot) {
			if (!std::isfinite(dt) || dt <= 0) return false;
			if (!m_initialized) {
				m_remaining = 0.35f + EnemyVariation(slot, 0, 11) * 2.4f;
				m_initialized = true;
			}
			m_remaining -= std::min(dt, 0.25f);
			if (m_remaining <= 0) {
				m_burst = !m_burst;
				const float variation = EnemyVariation(slot, ++m_sequence, 23);
				m_remaining += m_burst ? 0.4f + variation * 0.45f : 1.1f + variation * 1.7f;
			}
			return m_burst;
		}
		uint32_t GetShotIndex() const {
			return m_shots;
		}
		void NotifyShot() {
			++m_shots;
		}
		float Interval(float base, size_t slot) const {
			return base * (0.85f + EnemyVariation(slot, m_shots, 37) * 0.4f);
		}

	  private:
		float m_remaining = 0;
		uint32_t m_sequence = 0, m_shots = 0;
		bool m_initialized = false, m_burst = false;
	};

	inline DirectX::XMFLOAT3 EnemyGunDirection(const DirectX::XMFLOAT3& position,
		const DirectX::XMFLOAT3& velocity, const CombatTarget& target, float speed, size_t slot,
		uint32_t shot) {
		using namespace DirectX;
		auto direction = XMLoadFloat3(&target.center) - XMLoadFloat3(&position);
		if (XMVectorGetX(XMVector3LengthSq(direction)) < 0.0001f) return {0, 0, -1};
		direction = XMVector3Normalize(direction);
		if (const auto aim = SolveGunAim(position, velocity, target, speed, EnemyGunLifetimeSeconds)) {
			// 発射時だけ予測位置へ狙いを寄せる。発射後の弾は誘導しない。
			constexpr float kMinimumLeadFraction = 0.35f;
			constexpr float kLeadVariation = 0.2f;
			const float lead = kMinimumLeadFraction + EnemyVariation(slot, shot, 41) * kLeadVariation;
			direction = XMVector3Normalize(XMVectorLerp(direction, XMLoadFloat3(&aim->direction), lead));
		}
		constexpr float kYawErrorDegrees = 2.5f;
		constexpr float kPitchErrorDegrees = 1.8f;
		const float yaw = XMConvertToRadians((EnemyVariation(slot, shot, 53) * 2 - 1) * kYawErrorDegrees);
		const float pitch = XMConvertToRadians((EnemyVariation(slot, shot, 67) * 2 - 1) * kPitchErrorDegrees);
		auto right = XMVector3Cross(direction, XMVectorSet(0, 1, 0, 0));
		if (XMVectorGetX(XMVector3LengthSq(right)) < 0.0001f) right = XMVectorSet(1, 0, 0, 0);
		direction =
			XMVector3TransformNormal(direction, XMMatrixRotationAxis(XMVector3Normalize(right), pitch));
		direction = XMVector3TransformNormal(direction, XMMatrixRotationY(yaw));
		XMFLOAT3 result;
		XMStoreFloat3(&result, XMVector3Normalize(direction));
		return result;
	}
	inline std::array<DirectX::XMFLOAT3, 2> SpreadEnemyGunDirections(
		const DirectX::XMFLOAT3& center, size_t slot, uint32_t shot) {
		using namespace DirectX;
		constexpr float kMinimumSpreadDegrees = 3.0f;
		constexpr float kSpreadRangeDegrees = 3.0f;
		const auto forward = XMVector3Normalize(XMLoadFloat3(&center));
		auto right = XMVector3Cross(forward, XMVectorSet(0, 1, 0, 0));
		if (XMVectorGetX(XMVector3LengthSq(right)) < 0.0001f) right = XMVectorSet(1, 0, 0, 0);
		right = XMVector3Normalize(right);
		const auto up = XMVector3Normalize(XMVector3Cross(right, forward));
		const float angle = EnemyVariation(slot, shot, 83) * XM_2PI;
		const float spread =
			XMConvertToRadians(kMinimumSpreadDegrees + EnemyVariation(slot, shot, 97) * kSpreadRangeDegrees);
		const auto offset = (right * std::cos(angle) + up * std::sin(angle)) * std::tan(spread);
		std::array<XMFLOAT3, 2> directions;
		// 中心を挟んだ2方向へ散らす。弾の進行方向は発射時に確定する。
		XMStoreFloat3(&directions[0], XMVector3Normalize(forward + offset));
		XMStoreFloat3(&directions[1], XMVector3Normalize(forward - offset));
		return directions;
	}
} // namespace Game
