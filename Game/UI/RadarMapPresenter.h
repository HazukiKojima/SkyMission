#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {
	struct RadarLayout {
		DirectX::XMFLOAT2 origin{}, center{}, statusOrigin{};
		float scale = 0, size = 0, radius = 0;
	};
	struct RadarPoint {
		DirectX::XMFLOAT2 pixel{}, direction{};
		bool valid = false, outside = false;
	};
	class RadarMapPresenter {
	  public:
		static constexpr float RangeMeters = 10000;
		static RadarLayout Layout(const Engine::RenderView& view);
		// 北は-Z、画面右は-X。高度とカメラ姿勢には依存しない
		static RadarPoint Project(
			DirectX::XMFLOAT3 world, DirectX::XMFLOAT3 player, const RadarLayout& layout);
		void Build(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
	};
} // namespace Game
