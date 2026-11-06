#include "TargetingSystem.h"
#include "GunAimSolver.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace Game {
	using namespace DirectX;
	void TargetingSystem::Configure(const TargetingSettings& settings) {
		for (float value : {settings.selectionDistance, settings.acquireDistance,
				 settings.acquireAngleDegrees, settings.acquireSeconds, settings.maintainDistance,
				 settings.maintainAngleDegrees, settings.lossGraceSeconds, settings.assistAngleDegrees})
			if (!std::isfinite(value) || value <= 0)
				throw std::invalid_argument("targeting values must be positive and finite");
		if (settings.acquireDistance > settings.maintainDistance ||
			settings.maintainDistance > settings.selectionDistance ||
			settings.acquireAngleDegrees > settings.maintainAngleDegrees ||
			settings.maintainAngleDegrees >= 90 || settings.assistAngleDegrees > settings.acquireAngleDegrees)
			throw std::invalid_argument("targeting distance/angle ranges are inconsistent");
		m_settings = settings;
		m_selected = Engine::InvalidObjectId;
		m_state = LockState::None;
		m_acquiredSeconds = 0;
		m_lostSeconds = 0;
	}
	float TargetingSystem::GetProgress() const {
		return std::clamp(m_acquiredSeconds / m_settings.acquireSeconds, 0.0f, 1.0f);
	}

	void TargetingSystem::Update(std::span<const CombatTarget> enemies, const XMFLOAT3& position,
		const XMFLOAT3& forward, bool cycleRequested, float deltaSeconds) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds < 0)
			return;
		deltaSeconds = std::min(deltaSeconds, 0.25f);
		const auto candidates = CollectCandidates(enemies, position);
		const auto* target = SelectTarget(candidates, position, forward, cycleRequested);
		UpdateLock(target, position, forward, deltaSeconds);
	}
	std::vector<const CombatTarget*> TargetingSystem::CollectCandidates(
		std::span<const CombatTarget> enemies, const XMFLOAT3& position) const {
		std::vector<const CombatTarget*> candidates;
		const auto distance = [&](const CombatTarget& target) {
			return XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.center) - XMLoadFloat3(&position)));
		};
		for (const auto& enemy : enemies)
			if (enemy.health > 0 && distance(enemy) <= m_settings.selectionDistance)
				candidates.push_back(&enemy);
		std::sort(candidates.begin(), candidates.end(), [](const auto* left, const auto* right) {
			return left->id < right->id;
		});
		return candidates;
	}
	const CombatTarget* TargetingSystem::SelectTarget(std::span<const CombatTarget* const> candidates,
		const XMFLOAT3& position, const XMFLOAT3& forward, bool cycleRequested) {
		const auto distance = [&](const CombatTarget& target) {
			return XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.center) - XMLoadFloat3(&position)));
		};
		const auto previousSelectedId = m_selected;
		auto found = std::find_if(candidates.begin(), candidates.end(), [&](auto* candidate) {
			return candidate->id == m_selected;
		});
		if (cycleRequested && !candidates.empty()) {
			if (found != candidates.end())
				++found;
			if (found == candidates.end())
				found = candidates.begin();
			m_selected = (*found)->id;
		} else if (found == candidates.end()) {
			m_selected = Engine::InvalidObjectId;
			std::tuple<float, float, Engine::ObjectId> best{
				std::numeric_limits<float>::infinity(), 0.0f, Engine::InvalidObjectId};
			for (auto* candidate : candidates) {
				const auto direction =
					XMVector3Normalize(XMLoadFloat3(&candidate->center) - XMLoadFloat3(&position));
				const float dot = XMVectorGetX(XMVector3Dot(direction, XMLoadFloat3(&forward)));
				// 初回は前方を優先するが、選択対象の撃墜・喪失後は後方の敵も引き継ぐ
				if (dot <= 0 && previousSelectedId == Engine::InvalidObjectId)
					continue;
				const auto score = std::tuple{1 - dot, distance(*candidate), candidate->id};
				if (score < best) {
					best = score;
					m_selected = candidate->id;
				}
			}
		}
		if (previousSelectedId != m_selected) {
			m_acquiredSeconds = 0;
			m_lostSeconds = 0;
			m_state = LockState::Selected;
		}
		const auto selected = std::find_if(candidates.begin(), candidates.end(), [&](const auto* candidate) {
			return candidate->id == m_selected;
		});
		return selected == candidates.end() ? nullptr : *selected;
	}
	void TargetingSystem::UpdateLock(
		const CombatTarget* target, const XMFLOAT3& position, const XMFLOAT3& forward, float deltaSeconds) {
		if (!target) {
			m_state = LockState::None;
			m_acquiredSeconds = 0;
			m_lostSeconds = 0;
			return;
		}
		const float targetDistance =
			XMVectorGetX(XMVector3Length(XMLoadFloat3(&target->center) - XMLoadFloat3(&position)));
		XMFLOAT3 direction;
		XMStoreFloat3(&direction, XMLoadFloat3(&target->center) - XMLoadFloat3(&position));
		if (m_state == LockState::Locked) {
			if (targetDistance <= m_settings.maintainDistance &&
				IsWithinAngle(forward, direction, m_settings.maintainAngleDegrees))
				m_lostSeconds = 0;
			else
				m_lostSeconds += deltaSeconds;
			if (m_lostSeconds + 1e-6f < m_settings.lossGraceSeconds)
				return;
			m_state = LockState::Selected;
			m_acquiredSeconds = 0;
			m_lostSeconds = 0;
		}
		if (targetDistance <= m_settings.acquireDistance &&
			IsWithinAngle(forward, direction, m_settings.acquireAngleDegrees)) {
			m_acquiredSeconds = std::min(m_acquiredSeconds + deltaSeconds, m_settings.acquireSeconds);
			m_state = m_acquiredSeconds + 1e-6f >= m_settings.acquireSeconds ? LockState::Locked
																			 : LockState::Acquiring;
		} else {
			m_acquiredSeconds = 0;
			m_state = LockState::Selected;
		}
	}
} // namespace Game
