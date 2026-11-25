#pragma once
#include <DirectXMath.h>
#include <string_view>

namespace Engine::HudFont {
	inline constexpr unsigned Width = 1024, Height = 1024, Cell = 48, Columns = Width / Cell;
	inline constexpr std::u32string_view Japanese =
		U"敵機接近照準速度高度北体ロケット使用可秒再装填挑戦捕捉警告被弾任務達成自撃墜目標なし射程外命中全ミ"
		U"サイル左右▶："
		U"空母を破ク完了哨戒すべてせよで残りの闘編隊長失敗リ銃切替操縦にえ、後発艦交。増援はこれ最だ区域合わ"
		U"方が出現！から作法ヨー加減マウス見回正面よキップ"
		U"と護衛確認基地襲った部進元在も攻受けおい君一排除沈"
		U"指揮官直ち迎≪≫ホ押込選択注視司令"
		U"燃るうあメデ落嘘ろつやんくそま…来パじゃ化物ぎ振めエンジ脱死頼むどば？僚"
		U"ぞ調子４次爆阻止"
		U"能力奪"
		U"オタト戻ショア"
		U"抜前帰待動き滅生還新俺必ず叩×○△ティ"
		U"ゲムバ聞応答";
	inline constexpr unsigned AsciiCount = 95;
	inline constexpr unsigned GlyphCount = AsciiCount + unsigned(Japanese.size());
	static_assert(((GlyphCount + Columns - 1) / Columns) * Cell < Height - 8);

	inline const DirectX::XMFLOAT2 SolidUv{(Width - 0.5f) / Width, (Height - 0.5f) / Height};

	char32_t Next(std::string_view& text);

	inline unsigned Glyph(char32_t character) {
		if (character >= 32 && character <= 126) return unsigned(character - 32);

		// 日本語グリフがなければ疑問符で代替する
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
