#pragma once
#include "Engine/Renderer/Hud/HudDrawList.h"
#include <optional>

namespace Game {
	// 描画可能な部分までクリップした線分。両端は画面左上を原点とするピクセル座標
	struct HudSegmentProjection {
		DirectX::XMFLOAT2 head;
		DirectX::XMFLOAT2 tail;
	};
	struct HudProjection {
		// 画面外・背面でも方向案内に利用できる座標。画面内に収まる保証はない
		DirectX::XMFLOAT2 pixel;
		// 正のwを持ち、左右・上下・深度のすべてが描画範囲内ならtrue
		bool visible = false;
		// カメラの背面側（クリップ座標のw <= 0）にあるか
		bool behind = false;
	};

	// 同じフレームの投影行列と画面サイズを保持し、複数のHUD要素で共有する
	class HudProjector {
	  public:
		explicit HudProjector(const Engine::RenderView& view);
		// ワールド座標を投影する。描画可否は戻り値のvisibleで判定する
		HudProjection Project(const DirectX::XMFLOAT3& point) const;
		// 視錐台と交差しない線分や無効な画面サイズではnulloptを返す
		std::optional<HudSegmentProjection> ProjectClippedSegment(
			const DirectX::XMFLOAT3& head, const DirectX::XMFLOAT3& tail) const;

	  private:
		DirectX::XMFLOAT4X4 m_viewProjection;
		unsigned m_width;
		unsigned m_height;
	};

	// 単発投影用。多数の点を処理する場合はHudProjectorを再利用する
	HudProjection ProjectHudPoint(const DirectX::XMFLOAT3& point, const Engine::RenderView& view);
} // namespace Game
