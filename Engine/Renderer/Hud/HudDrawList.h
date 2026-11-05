#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string_view>

namespace Engine {
	// HUD投影に使うカメラ行列と描画先サイズ。width・heightはピクセル単位
	struct RenderView {
		DirectX::XMFLOAT4X4 view{}, projection{};
		unsigned width = 0;
		unsigned height = 0;
	};
	struct HudSettings {
		// 基準解像度でのスタイル値。解像度に応じた縮尺は描画側で適用する
		DirectX::XMFLOAT4 color{232.0f / 255, 217.0f / 255, 181.0f / 255, 0.95f};
		float lineWidth = 2.0f;
		float fontSize = 40;
	};
	struct HudVertex {
		// GPUへ渡す正規化座標。描画APIが受け取るピクセル座標とは異なる
		DirectX::XMFLOAT2 position;
		DirectX::XMFLOAT2 uv;
		DirectX::XMFLOAT4 color;
	};
	// 左上原点・右向きX・下向きYのピクセル座標からHUD頂点を蓄積する
	// 追加順が描画順となるため、背景・エフェクト・文字の順序は呼び出し側で管理する
	class HudDrawList {
	  public:
		// 容量超過時は描画処理がlength_errorを送出する。追加前の予算管理に利用する
		static constexpr size_t MaxVertices = 32768;
		HudDrawList() {
			m_vertices.reserve(MaxVertices);
		}
		// フレームの先頭で呼び、前フレームの頂点を消去して描画先サイズを設定する
		void Reset(unsigned width, unsigned height) {
			m_vertices.clear();
			m_width = width;
			m_height = height;
		}
		void Line(DirectX::XMFLOAT2 a, DirectX::XMFLOAT2 b, float width, DirectX::XMFLOAT4 color);
		void FilledRect(DirectX::XMFLOAT2 position, DirectX::XMFLOAT2 size, DirectX::XMFLOAT4 color);
		void CloudPuff(
			DirectX::XMFLOAT2 center, float radius, float rotation, unsigned seed, DirectX::XMFLOAT4 color);
		void SoftDisc(DirectX::XMFLOAT2 center, float radius, DirectX::XMFLOAT4 color);
		// progressは0～1の円弧割合。12時方向から時計回りに描き、1で全周となる
		void Circle(
			DirectX::XMFLOAT2 center, float radius, float progress, float width, DirectX::XMFLOAT4 color);
		// textはUTF-8。heightはピクセル単位で、未収録文字は代替グリフで描画する
		void Text(DirectX::XMFLOAT2 position, std::string_view text, float height, DirectX::XMFLOAT4 color);
		// Textと同じ文字幅計算を使う。配置計算のみを行い、頂点は追加しない
		static float MeasureTextWidth(std::string_view text, float height);
		// 内部バッファへの参照。要素のポインタ・イテレータは追加やResetをまたいで保持しない
		const std::vector<HudVertex>& GetVertices() const {
			return m_vertices;
		}

	  private:
		void Quad(const DirectX::XMFLOAT2 (&positions)[4], DirectX::XMFLOAT2 uv0, DirectX::XMFLOAT2 uv1,
			DirectX::XMFLOAT4 color);
		unsigned m_width = 0;
		unsigned m_height = 0;
		std::vector<HudVertex> m_vertices;
	};
} // namespace Engine
