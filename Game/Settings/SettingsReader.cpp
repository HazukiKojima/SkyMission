#include "SettingsReader.h"

namespace Game {
	namespace {
		constexpr size_t kMaximumSettingsTextBytes = 192;
	}
	void SettingsReader::Number(const std::string& key, float& value, float minimum, float maximum) const {
		if (!exists) return;
		if (auto values = Read(key, [&] {
				return reader.TryResolveFloatArray(path, key);
			})) {
			Require(values->size() == 1, key, "expected a number");
			Require(
				std::isfinite(values->front()) && values->front() >= minimum && values->front() <= maximum,
				key, "number outside supported range");
			value = values->front();
		}
	}

	void SettingsReader::Vector(const std::string& key, DirectX::XMFLOAT3& value) const {
		std::array<float, 3> v{value.x, value.y, value.z};
		Array(key, v);
		value = {v[0], v[1], v[2]};
	}

	void SettingsReader::Color(const std::string& key, DirectX::XMFLOAT4& value) const {
		std::array<float, 4> v{value.x, value.y, value.z, value.w};
		Array(key, v, 0, 1);
		value = {v[0], v[1], v[2], v[3]};
	}

	void SettingsReader::Text(const std::string& key, std::string& value) const {
		if (!exists) return;
		if (auto text = Read(key, [&] {
				return reader.TryResolveString(path, key);
			})) {
			Require(!text->empty() && text->size() <= kMaximumSettingsTextBytes, key, "expected 1..192 UTF-8 bytes");
			value = *text;
		}
	}

	void SettingsReader::Require(bool valid, const std::string& key, const char* reason) const {
		if (!valid) throw std::runtime_error(path.string() + ": " + key + ": " + reason);
	}
} // namespace Game
