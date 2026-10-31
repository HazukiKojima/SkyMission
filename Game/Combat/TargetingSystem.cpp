#include "TargetingSystem.h"
#include "GunAimSolver.h"
#include <algorithm>
#include <cmath>
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
		m_acquired = 0;
		m_lost = 0;
	}
	float TargetingSystem::GetProgress() const {
		return std::clamp(m_acquired / m_settings.acquireSeconds, 0.0f, 1.0f);
	}

	void TargetingSystem::Update(std::span<const CombatTarget> enemies, const XMFLOAT3& position,
		const XMFLOAT3& forward, bool next, float dt) {
		if (!std::isfinite(dt) || dt < 0)
			return;
		dt = std::min(dt, 0.25f);
		std::vector<const CombatTarget*> candidates;
		const auto distance = [&](const CombatTarget& target) {
			return XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.center) - XMLoadFloat3(&position)));
		};
		for (const auto& enemy : enemies)
			if (enemy.health > 0 && distance(enemy) <= m_settings.selectionDistance)
				candidates.push_back(&enemy);
		std::sort(candidates.begin(), candidates.end(), [](auto* a, auto* b) {
			return a->id < b->id;
		});
		const auto old = m_selected;
		auto found = std::find_if(candidates.begin(), candidates.end(), [&](auto* p) {
			return p->id == m_selected;
		});
		if (next && !candidates.empty()) {
			if (found == candidates.end() || ++found == candidates.end())
				found = candidates.begin();
			m_selected = (*found)->id;
		} else if (found == candidates.end()) {
			m_selected = Engine::InvalidObjectId;
			std::tuple<float, float, Engine::ObjectId> best{2.0f, 0.0f, Engine::InvalidObjectId};
			for (auto* candidate : candidates) {
				const auto direction =
					XMVector3Normalize(XMLoadFloat3(&candidate->center) - XMLoadFloat3(&position));
				const float dot = XMVectorGetX(XMVector3Dot(direction, XMLoadFloat3(&forward)));
				if (dot <= 0)
					continue;
				const auto score = std::tuple{1 - dot, distance(*candidate), candidate->id};
				if (score < best) {
					best = score;
					m_selected = candidate->id;
				}
			}
		}
		if (old != m_selected) {
			m_acquired = 0;
			m_lost = 0;
			m_state = LockState::Selected;
		}
		if (m_selected == Engine::InvalidObjectId) {
			m_state = LockState::None;
			m_acquired = 0;
			m_lost = 0;
			return;
		}
		const auto target = std::find_if(candidates.begin(), candidates.end(), [&](auto* p) {
			return p->id == m_selected;
		});
		XMFLOAT3 direction;
		XMStoreFloat3(&direction, XMLoadFloat3(&(*target)->center) - XMLoadFloat3(&position));
		if (m_state == LockState::Locked) {
			if (distance(**target) <= m_settings.maintainDistance &&
				IsWithinAngle(forward, direction, m_settings.maintainAngleDegrees))
				m_lost = 0;
			else
				m_lost += dt;
			if (m_lost + 1e-6f < m_settings.lossGraceSeconds)
				return;
			m_state = LockState::Selected;
			m_acquired = 0;
			m_lost = 0;
		}
		if (distance(**target) <= m_settings.acquireDistance &&
			IsWithinAngle(forward, direction, m_settings.acquireAngleDegrees)) {
			m_acquired = std::min(m_acquired + dt, m_settings.acquireSeconds);
			m_state =
				m_acquired + 1e-6f >= m_settings.acquireSeconds ? LockState::Locked : LockState::Acquiring;
		} else {
			m_acquired = 0;
			m_state = LockState::Selected;
		}
	}
} // namespace Game
