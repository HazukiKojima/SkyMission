#pragma once
#include "WeaponDefinition.h"
#include "WeaponController.h"
#include "Engine/Resources/Mesh/Model.h"

namespace Game {
	// 4門の銃口位置と斉射間隔を管理する。実際の弾の生成・移動はProjectileSystemが担当する
	class AircraftGun {
	  public:
		// モデル内で各銃口名が一意に解決できることを検証し、発射待ち時間を初期化する
		void Configure(const GunSettings& settings, const Engine::Model& model);
		// 各斉射がフレーム末までに経過した秒数を返す
		std::vector<float> Update(WeaponInput input, float dt);
		// モデル座標系の銃口位置。発射時に機体のワールド変換を適用する
		const std::array<DirectX::XMFLOAT3, 4>& GetMuzzles() const {
			return m_muzzles;
		}

	  private:
		GunSettings m_settings;
		std::array<DirectX::XMFLOAT3, 4> m_muzzles{};
		double m_cooldown = 0;
	};
} // namespace Game
