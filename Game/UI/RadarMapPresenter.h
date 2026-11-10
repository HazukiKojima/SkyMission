#pragma once
#include "Game/Settings/GameplaySettings.h"
#include "Game/Settings/UiTextSettings.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "Game/Combat/CombatTypes.h"

namespace Game {
	// レーダーと隣接ステータス欄の配置。位置・大きさは実画面のピクセル単位
	struct RadarLayout {
		DirectX::XMFLOAT2 origin{}, center{}, statusOrigin{};
		float scale = 0, size = 0, radius = 0;
		float rangeMeters = 10000;
	};
	struct RadarPoint {
		// 範囲外の対象は円周上に置き、directionには中心からの単位方向を保持する
		DirectX::XMFLOAT2 pixel{}, direction{};
		// valid=falseは投影不可、outside=trueは探知距離の外側を表す
		bool valid = false, outside = false;
	};
	// 自機中心・北固定の俯瞰レーダー。戦闘HUDと同じリストへ描画を追記する
	class RadarMapPresenter {
	  public:
		static constexpr float RangeMeters = 10000;
		static RadarLayout Layout(const Engine::RenderView& view, const UiTuning& tuning = {});
		// 北は-Z、画面右は-X。高度とカメラ姿勢には依存しない
		static RadarPoint Project(
			DirectX::XMFLOAT3 world, DirectX::XMFLOAT3 player, const RadarLayout& layout);
		void Build(const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list,
			const UiTuning& tuning = {}, const UiTextSettings& text = UiTextSettings::Defaults()) const;
	};
} // namespace Game
