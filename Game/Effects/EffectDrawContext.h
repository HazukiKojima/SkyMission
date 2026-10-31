#pragma once
#include "CombatEffectSettings.h"
#include "Game/UI/HudProjection.h"
#include <optional>

namespace Game {
	enum class EffectClipMode { CenterVisible, BoundsVisible };
	struct ProjectedEffectSegment {
		DirectX::XMFLOAT2 head;
		DirectX::XMFLOAT2 tail;
	};

	class EffectRandom {
	  public:
		explicit EffectRandom(const DirectX::XMFLOAT3& position);
		float Sample(int index) const;

	  private:
		float m_seed;
	};

	// 煙、閃光、破片のワールド座標を共有の投影条件で描画する
	class EffectDrawContext {
	  public:
		EffectDrawContext(const Engine::RenderView& view, Engine::HudDrawList& list);
		bool HasVertexBudget(size_t requiredVertices) const;
		float GetScale() const;
		void DrawCloud(DirectX::FXMVECTOR position, float radius, float rotation, unsigned seed,
			DirectX::XMFLOAT4 color, EffectClipMode clipMode = EffectClipMode::BoundsVisible);
		void DrawGlow(DirectX::FXMVECTOR position, float radius, DirectX::XMFLOAT4 color,
			EffectClipMode clipMode = EffectClipMode::BoundsVisible);
		std::optional<ProjectedEffectSegment> ProjectSegment(
			DirectX::FXMVECTOR head, DirectX::FXMVECTOR tail) const;
		void DrawStreak(const ProjectedEffectSegment& segment, float outerWidth, DirectX::XMFLOAT4 outerColor,
			float innerWidth, DirectX::XMFLOAT4 innerColor);

	  private:
		struct ProjectedBillboard {
			DirectX::XMFLOAT2 center;
			float radius;
		};
		std::optional<ProjectedBillboard> ProjectBillboard(
			DirectX::FXMVECTOR position, float radius, EffectClipMode clipMode) const;
		const Engine::RenderView& m_view;
		Engine::HudDrawList& m_list;
		HudProjector m_projector;
		DirectX::XMFLOAT3 m_cameraRight;
	};
} // namespace Game
