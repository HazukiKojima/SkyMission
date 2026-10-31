#pragma once
#include "WeaponDefinition.h"
#include "WeaponController.h"
#include "Engine/Resources/Mesh/Model.h"

namespace Game {
	class AircraftGun {
	  public:
		void Configure(const GunSettings& settings, const Engine::Model& model);
		// 各斉射がフレーム末までに経過した秒数を返す
		std::vector<float> Update(WeaponInput input, float dt);
		const std::array<DirectX::XMFLOAT3, 4>& GetMuzzles() const {
			return m_muzzles;
		}

	  private:
		GunSettings m_settings;
		std::array<DirectX::XMFLOAT3, 4> m_muzzles{};
		double m_cooldown = 0;
	};
} // namespace Game
