#pragma once
#include "Game/Settings/GameplaySettings.h"
#include "Game/Settings/UiTextSettings.h"
#include "Engine/Renderer/Hud/HudDrawList.h"
#include "Game/Combat/CombatTypes.h"
#include "HudProjection.h"

namespace Game {
	// 戦闘状態をHUDへ変換する。対象選択や命中判定などのゲーム状態は変更しない
	class CombatHudPresenter {
	  public:
		void ConfigureTuning(const UiTuning& settings) {
			m_tuning = settings;
		}
		void ConfigureText(const UiTextSettings& text) {
			m_text = text;
		}
		// 線幅・文字サイズは1920x1080基準。Buildで実際の解像度に合わせて縮尺を適用する
		void Configure(const Engine::HudSettings& settings) {
			m_settings = settings;
		}
		// 初期化済みリストへレーダー・敵方向矢印・照準・文字を追記する
		// エフェクト描画後に呼び出し、HUDの視認性を保つ。幅か高さが0なら描画しない
		void Build(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;

	  private:
		void DrawCombatGuide(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawDebugCollisions(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawControls(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawFlightStatus(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawGunSight(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawEnemyMarkers(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawMissionStatus(
			const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawEnemyDirection(const DirectX::XMFLOAT3& target, const CombatSnapshot& snapshot,
			const Engine::RenderView& view, Engine::HudDrawList& list) const;
		void DrawOffscreenIndicator(const HudProjection& projection, DirectX::XMFLOAT4 tint,
			const Engine::RenderView& view, Engine::HudDrawList& list) const;

		UiTuning m_tuning;
		UiTextSettings m_text;
		Engine::HudSettings m_settings;
		// 描画上の補間状態のみ保持する。対象変更・リトライ時は新しい方向から開始する
		mutable Engine::ObjectId m_arrowTarget = Engine::InvalidObjectId;
		mutable float m_arrowSeconds = -1;
		mutable float m_arrowAngle = 0;
		mutable float m_rearTurnSide = 1;
	};
} // namespace Game
