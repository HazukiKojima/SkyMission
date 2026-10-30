#include "RocketLauncher.h"
#include <algorithm>

namespace Game {
	void RocketLauncher::Configure(const RocketSettings& settings, const Engine::Model& aircraft) {
		ValidateRocketSettings(settings);
		std::array<DirectX::XMFLOAT3, 2> muzzles{};
		for (size_t i = 0; i < muzzles.size(); ++i) {
			if (aircraft.CountNodes(settings.muzzleNames[i]) != 1)
				throw std::invalid_argument(
					"muzzleNames: expected exactly one node named '" + settings.muzzleNames[i] + "'");
			muzzles[i] = *aircraft.FindNodePosition(settings.muzzleNames[i]);
			if (!std::isfinite(muzzles[i].x) || !std::isfinite(muzzles[i].y) || !std::isfinite(muzzles[i].z))
				throw std::invalid_argument("muzzleNames: nonfinite muzzle position");
		}
		m_settings = settings;
		m_muzzles = muzzles;
		m_cooldowns = {};
		m_nextMuzzle = 0;
		m_configured = true;
	}
	void RocketLauncher::Update(float deltaSeconds) {
		if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0)
			return;
		for (auto& cooldown : m_cooldowns)
			cooldown = std::max(0.0, cooldown - std::min(double(deltaSeconds), 0.25));
	}
	std::optional<DirectX::XMFLOAT3> RocketLauncher::GetNextMuzzle() const {
		if (!m_configured || m_cooldowns[m_nextMuzzle] > 1e-6)
			return std::nullopt;
		return m_muzzles[m_nextMuzzle];
	}
	void RocketLauncher::CommitShot() {
		// モデル生成に成功した場合だけ、順番と銃口別のクールタイムを進める
		if (!GetNextMuzzle())
			return;
		m_cooldowns[m_nextMuzzle] = m_settings.cooldownSeconds;
		m_nextMuzzle = 1 - m_nextMuzzle;
	}
	RocketHudState RocketLauncher::GetState() const {
		return {m_configured, {float(m_cooldowns[0]), float(m_cooldowns[1])}, m_nextMuzzle};
	}
} // namespace Game
