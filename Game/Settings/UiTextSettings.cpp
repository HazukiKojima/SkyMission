#include "UiTextSettings.h"

namespace Game {
	const UiTextSettings& UiTextSettings::Defaults() {
		static const UiTextSettings defaults;
		return defaults;
	}

	void UiTextSettings::Load(const Engine::JsonAssetReader& reader, const std::filesystem::path& path) {
		SettingsReader r(reader, path);

		for (auto& [key, value] : labels) {
			std::string escaped;

			for (char c : key) {
				if (c == '~')
					escaped += "~0";
				else if (c == '/')
					escaped += "~1";
				else
					escaped += c;
			}

			r.Text("/labels/" + escaped, value);
		}
	}
} // namespace Game
