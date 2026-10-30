#pragma once
#include "RocketDefinition.h"
#include "Game/Combat/CombatTypes.h"
#include <optional>

namespace Game {
	class RocketLauncher {
	  public:
		void Configure(const RocketSettings& settings, const Engine::Model& aircraft);
		void Update(float deltaSeconds);
		std::optional<DirectX::XMFLOAT3> GetNextMuzzle() const;
		void CommitShot();
		RocketHudState GetState() const;

	  private:
		RocketSettings m_settings;
		std::array<DirectX::XMFLOAT3, 2> m_muzzles{};
		std::array<double, 2> m_cooldowns{};
		size_t m_nextMuzzle = 0;
		bool m_configured = false;
	};
} // namespace Game
