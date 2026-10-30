#include "HudDrawList.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Engine {
	void HudDrawList::Quad(const DirectX::XMFLOAT2 (&positions)[4], DirectX::XMFLOAT2 uv0,
		DirectX::XMFLOAT2 uv1, DirectX::XMFLOAT4 color) {
		if (m_width == 0 || m_height == 0) return;
		if (m_vertices.size()+6 > MaxVertices) throw std::length_error("HUD vertex capacity exceeded");
		const DirectX::XMFLOAT2 uv[] = {uv0, {uv1.x,uv0.y}, uv1, {uv0.x,uv1.y}};
		for (const int index : {0,1,2,0,2,3})
			m_vertices.push_back({{positions[index].x*2/m_width-1, 1-positions[index].y*2/m_height}, uv[index], color});
	}
	void HudDrawList::Line(DirectX::XMFLOAT2 a, DirectX::XMFLOAT2 b, float width, DirectX::XMFLOAT4 color) {
		const float dx=b.x-a.x, dy=b.y-a.y, length=std::hypot(dx,dy);
		if (length < 0.001f) return;
		const float x=-dy/length*width*0.5f, y=dx/length*width*0.5f;
		const DirectX::XMFLOAT2 p[]={{a.x+x,a.y+y},{b.x+x,b.y+y},{b.x-x,b.y-y},{a.x-x,a.y-y}};
		Quad(p,{511.5f/512,383.5f/384},{511.5f/512,383.5f/384},color);
	}
	void HudDrawList::Circle(DirectX::XMFLOAT2 center, float radius, float progress, float width, DirectX::XMFLOAT4 color) {
		const float arc = std::clamp(progress,0.0f,1.0f)*DirectX::XM_2PI;
		for (float begin=0; begin<arc; begin+=DirectX::XM_2PI/64) {
			const float end=std::min(begin+DirectX::XM_2PI/64,arc);
			Line({center.x+std::sin(begin)*radius,center.y-std::cos(begin)*radius},
				{center.x+std::sin(end)*radius,center.y-std::cos(end)*radius},width,color);
		}
	}
	void HudDrawList::Text(DirectX::XMFLOAT2 position, std::string_view text, float height, DirectX::XMFLOAT4 color) {
		for (const unsigned char character : text) {
			const unsigned glyph = character >= 32 && character <= 126 ? character-32 : '?'-32;
			const float x=float(glyph%16)*32, y=float(glyph/16)*48;
			const float width=height*32/48;
			const DirectX::XMFLOAT2 p[]={{position.x,position.y},{position.x+width,position.y},
				{position.x+width,position.y+height},{position.x,position.y+height}};
			Quad(p,{x/512,y/384},{(x+32)/512,(y+48)/384},color);
			position.x += height*24/48;
		}
	}
}
