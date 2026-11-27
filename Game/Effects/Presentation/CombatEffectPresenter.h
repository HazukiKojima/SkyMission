#pragma once
#include "Game/Effects/CombatEffectSettings.h"
#include "Game/Effects/CombatEffectTypes.h"
#include "Engine/Renderer/Hud/HudDrawList.h"

namespace Game {
	class EffectDrawContext;
	class CombatEffectPresenter {
	  public:
		void Configure(const EffectSettings& settings) {
			m_settings = settings;
		}

		// 呼び出し側で初期化した描画リストに追記し、照準より先に描く
		void Build(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
			const DirectX::XMFLOAT3& playerPosition, Engine::HudDrawList& list) const;

	  private:
		static constexpr unsigned aircraftFireCloudCount = 6;
		static constexpr size_t aircraftFireVertexBudget =
			2 * Engine::HudDrawList::SoftDiscVertexCount +
			aircraftFireCloudCount * Engine::HudDrawList::QuadVertexCount;
		EffectSettings m_settings;
		void DrawAircraftSmoke(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
		void DrawAircraftFire(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
		void DrawAircraftVapor(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
			EffectDrawContext& context) const;
		void DrawExplosionScreenFlash(const CombatEffectSnapshot& snapshot, const Engine::RenderView& view,
			const DirectX::XMFLOAT3& playerPosition, EffectDrawContext& context, Engine::HudDrawList& list) const;
		void DrawRocketEffects(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
		void DrawExplosion(
			const ExplosionEffect& effect, EffectDrawContext& context, float fragmentScale) const;
		void DrawDeckSmoke(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
		void DrawImpact(const ImpactEffect& effect, EffectDrawContext& context) const;
		void DrawGunSmoke(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
		void DrawGunFire(const CombatEffectSnapshot& snapshot, EffectDrawContext& context) const;
	};
} // namespace Game
