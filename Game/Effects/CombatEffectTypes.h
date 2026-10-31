#pragma once
#include <DirectXMath.h>
#include "CombatEffectSettings.h"
#include <vector>
#include "Game/Weapons/GunShotEvent.h"

namespace Game {
	struct GunMuzzleFlash {
		std::size_t muzzleIndex = 0;
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 direction{0, 0, -1};
		float age = 0;
		std::uint64_t sequence = 0;
	};
	struct GunSmoke {
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 velocity{};
		float age = 0;
	};
	struct RocketTrailSample {
		DirectX::XMFLOAT3 position{};
		float age = 0;
	};
	struct RocketExhaust {
		DirectX::XMFLOAT3 position{}, direction{0, 0, -1};
	};
	struct ImpactEffect {
		DirectX::XMFLOAT3 position{};
		float age = 0;
		DirectX::XMFLOAT3 velocity{};
		// メッシュの法線ではなく、入射方向の逆を金属片の散乱基準に使う
		DirectX::XMFLOAT3 direction{0, 0, -1};
	};

	struct ExplosionEffect {
		DirectX::XMFLOAT3 position{}, velocity{};
		float age = 0;
		float radius = ExplosionEffectSettings::minRadius;
	};

	// 敵機が削除された後も描画できるよう、位置と速度を値として保持する
	struct CombatEffectSnapshot {
		std::vector<GunMuzzleFlash> gunFlashes;
		std::vector<GunSmoke> gunSmoke;
		std::vector<GunTracerSegment> gunTracers;
		std::vector<ImpactEffect> impacts;
		std::vector<ExplosionEffect> explosions;
		std::vector<RocketTrailSample> rocketSmoke;
		std::vector<RocketExhaust> rocketExhausts;
	};
} // namespace Game
