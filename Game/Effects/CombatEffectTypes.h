#pragma once
#include <DirectXMath.h>
#include "CombatEffectSettings.h"
#include <vector>
#include "Game/Weapons/Gun/GunShotEvent.h"

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

	struct AircraftSmokePuff {
		static constexpr float lifetimeSeconds = 4.0f;
		static constexpr size_t maximumCount = 512;
		DirectX::XMFLOAT3 position{};
		float age = 0;
	};

	struct AircraftFire {
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 direction{};
		float seconds = 0;
		float size = 1.0f;
	};

	struct WingtipVaporSegment {
		static constexpr float lifetimeSeconds = 1.0f;
		static constexpr size_t maximumCount = 1024;
		DirectX::XMFLOAT3 head{}, tail{};
		float strength = 0;
		float age = 0;
	};

	struct RocketExhaust {
		DirectX::XMFLOAT3 position{}, direction{0, 0, -1};
	};

	struct ContrailPuff {
		static constexpr float lifetimeSeconds = 3.6f;
		static constexpr size_t maximumCount = 4096;
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 direction{};
		float age = 0;
	};

	struct ImpactEffect {
		DirectX::XMFLOAT3 position{};
		float age = 0;
		DirectX::XMFLOAT3 velocity{};

		// メッシュの法線ではなく、入射方向の逆を金属片の散乱基準に使う
		DirectX::XMFLOAT3 direction{0, 0, -1};
	};

	enum class ExplosionStyle { Standard, BacklitSmoke, FallingDebris };

	struct ExplosionEffect {
		DirectX::XMFLOAT3 position{}, velocity{};
		float age = 0;
		float radius = ExplosionEffectSettings::minRadius;
		float flashDurationMultiplier = 1.0f;
		ExplosionStyle style = ExplosionStyle::Standard;
	};

	// 敵機が削除された後も描画できるよう、位置と速度を値として保持する
	struct CombatEffectSnapshot {
		std::vector<GunMuzzleFlash> gunFlashes;
		std::vector<GunSmoke> gunSmoke;
		std::vector<GunTracerSegment> gunTracers;
		std::vector<RocketTrailSample> deckSmoke;
		std::vector<ImpactEffect> impacts;
		std::vector<ExplosionEffect> explosions;
		std::vector<RocketTrailSample> rocketSmoke;
		std::vector<AircraftSmokePuff> aircraftSmoke;
		std::vector<AircraftFire> aircraftFires;
		std::vector<RocketExhaust> rocketExhausts;
		std::vector<WingtipVaporSegment> wingtipVapor;
		std::vector<ContrailPuff> contrails;
	};

	struct ExplosionFeedback {
		float impact = 0;
		float flash = 0;
	};

	ExplosionFeedback EvaluateExplosionFeedback(const CombatEffectSnapshot& snapshot,
		const DirectX::XMFLOAT3& playerPosition, const EffectSettings::ExplosionFeedbackTuning& settings);
} // namespace Game
