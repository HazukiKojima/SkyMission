#pragma once
#include "Game/Effects/CombatEffectSettings.h"
#include "Game/UI/HudProjection.h"
#include <optional>

namespace Game {
	// 中心が画面内のものだけ描くか、外周の一部が見えるものも描くかを選ぶ
	enum class EffectClipMode { CenterVisible, BoundsVisible };

	// 発生位置とサンプル番号から再現可能な値を作り、フレーム間の模様のちらつきを防ぐ
	class EffectRandom {
	  public:
		explicit EffectRandom(const DirectX::XMFLOAT3& position);
		float Sample(int index) const; // 戻り値は0以上1未満

	  private:
		float m_seed;
	};

	// 煙、閃光、破片のワールド座標を共有の投影条件で描画する
	class EffectDrawContext {
	  public:
		const EffectSettings& Settings() const {
			return m_settings;
		}

		EffectDrawContext(
			const Engine::RenderView& view, Engine::HudDrawList& list, const EffectSettings& settings = {});

		// HUDなどの予約分を残したまま、指定頂点数を追加できるか判定する
		bool HasVertexBudget(size_t requiredVertices) const;
		void SetAdditionalReservedVertices(size_t vertices);
		float GetScale() const;
		void DrawCloud(DirectX::FXMVECTOR position, float radius, float rotation, unsigned seed,
			DirectX::XMFLOAT4 color, EffectClipMode clipMode = EffectClipMode::BoundsVisible,
			float minimumPixelRadius = 0);
		void DrawGlow(DirectX::FXMVECTOR position, float radius, DirectX::XMFLOAT4 color,
			EffectClipMode clipMode = EffectClipMode::BoundsVisible);
		std::optional<HudSegmentProjection> ProjectSegment(
			DirectX::FXMVECTOR head, DirectX::FXMVECTOR tail) const;
		std::optional<HudSegmentProjection> ProjectClippedSegment(
			const DirectX::XMFLOAT3& head, const DirectX::XMFLOAT3& tail) const;
		void DrawStreak(const HudSegmentProjection& segment, float outerWidth, DirectX::XMFLOAT4 outerColor,
			float innerWidth, DirectX::XMFLOAT4 innerColor);

	  private:
		struct ProjectedBillboard {
			DirectX::XMFLOAT2 center;
			float radius;
			float depth;
		};

		std::optional<ProjectedBillboard> ProjectBillboard(
			DirectX::FXMVECTOR position, float radius, EffectClipMode clipMode, float minimumPixelRadius = 0) const;

		// 借用参照のため、viewとlistはこのコンテキストより長く生存させる
		EffectSettings m_settings;
		const Engine::RenderView& m_view;
		Engine::HudDrawList& m_list;
		HudProjector m_projector;
		DirectX::XMFLOAT3 m_cameraRight;
		size_t m_additionalReservedVertices = 0;
	};
} // namespace Game
