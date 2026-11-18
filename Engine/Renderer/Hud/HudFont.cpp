#include "HudFont.h"

namespace Engine::HudFont {
	char32_t Next(std::string_view& text) {
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
} // namespace Engine::HudFont
