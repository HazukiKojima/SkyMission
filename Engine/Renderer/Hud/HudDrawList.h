#pragma once
#include <DirectXMath.h>
#include <vector>
#include <string_view>

namespace Engine {
	struct RenderView {
		DirectX::XMFLOAT4X4 view{}, projection{};
		unsigned width = 0, height = 0;
	};
	struct HudSettings {
		DirectX::XMFLOAT4 color{0.15f, 1.0f, 0.35f, 0.8f};
		float lineWidth = 2.5f, fontSize = 40;
	};
	struct HudVertex { DirectX::XMFLOAT2 position, uv; DirectX::XMFLOAT4 color; };
	class HudDrawList {
	public:
		static constexpr size_t MaxVertices = 32768;
		HudDrawList() { m_vertices.reserve(MaxVertices); }
		void Reset(unsigned width, unsigned height) { m_vertices.clear(); m_width = width; m_height = height; }
		void Line(DirectX::XMFLOAT2 a, DirectX::XMFLOAT2 b, float width, DirectX::XMFLOAT4 color);
		void CloudPuff(DirectX::XMFLOAT2 center, float radius, float rotation, unsigned seed, DirectX::XMFLOAT4 color);
		void SoftDisc(DirectX::XMFLOAT2 center, float radius, DirectX::XMFLOAT4 color);
		void Circle(DirectX::XMFLOAT2 center, float radius, float progress, float width, DirectX::XMFLOAT4 color);
		void Text(DirectX::XMFLOAT2 position, std::string_view text, float height, DirectX::XMFLOAT4 color);
		const std::vector<HudVertex>& GetVertices() const { return m_vertices; }
	private:
		void Quad(const DirectX::XMFLOAT2 (&positions)[4], DirectX::XMFLOAT2 uv0, DirectX::XMFLOAT2 uv1, DirectX::XMFLOAT4 color);
		unsigned m_width = 0, m_height = 0;
		std::vector<HudVertex> m_vertices;
	};
}
