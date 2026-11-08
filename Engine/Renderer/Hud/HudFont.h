#pragma once
#include <DirectXMath.h>
#include <string_view>

namespace Engine::HudFont {
	inline constexpr unsigned Width = 1024, Height = 768, Cell = 48, Columns = Width / Cell;
	inline constexpr std::u32string_view Japanese =
		U"敵機接近照準速度高度北体ロケット使用可秒再装填挑戦捕捉警告被弾任務達成自撃墜目標なし射程外命中全ミ"
		U"サイル左右▶："
		U"空母を破ク完了哨戒すべてせよで残りの闘編隊長失敗リ銃切替操縦にえ、後発艦交。増援はこれ最だ区域合わ"
		U"方が出現！から作法ヨー加減マウス見回正面よキップ";
	inline constexpr unsigned AsciiCount = 95;
	inline constexpr unsigned GlyphCount = AsciiCount + unsigned(Japanese.size());
	static_assert(((GlyphCount + Columns - 1) / Columns) * Cell < Height - 8);
	inline const DirectX::XMFLOAT2 SolidUv{(Width - 0.5f) / Width, (Height - 0.5f) / Height};

	inline char32_t Next(std::string_view& text) {
		const auto first = static_cast<unsigned char>(text.front());
		text.remove_prefix(1);
		if (first < 0x80) return first;
		unsigned count = 0;
		char32_t value = 0, minimum = 0;
		if (first >= 0xc2 && first <= 0xdf) {
			count = 1;
			value = first & 0x1f;
			minimum = 0x80;
		} else if (first >= 0xe0 && first <= 0xef) {
			count = 2;
			value = first & 0x0f;
			minimum = 0x800;
		} else if (first >= 0xf0 && first <= 0xf4) {
			count = 3;
			value = first & 0x07;
			minimum = 0x10000;
		} else
			return U'?';
		if (text.size() < count) return U'?';
		for (unsigned i = 0; i < count; ++i) {
			const auto next = static_cast<unsigned char>(text[i]);
			if ((next & 0xc0) != 0x80) return U'?';
			value = (value << 6) | (next & 0x3f);
		}
		text.remove_prefix(count);
		return value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff) ? U'?' : value;
	}
	inline unsigned Glyph(char32_t character) {
		if (character >= 32 && character <= 126) return unsigned(character - 32);
		const auto index = Japanese.find(character);
		return index == Japanese.npos ? unsigned('?' - 32) : AsciiCount + unsigned(index);
	}
	inline float GlyphWidth(unsigned glyph) {
		return glyph < AsciiCount ? 32.0f : 48.0f;
	}
	inline float Advance(unsigned glyph) {
		return glyph < AsciiCount ? 24.0f : 44.0f;
	}
} // namespace Engine::HudFont
