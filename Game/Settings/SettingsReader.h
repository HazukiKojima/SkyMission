#pragma once
#include "Engine/Resources/JsonAssetReader.h"
#include <DirectXMath.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <array>
namespace Game {
	// Missing fields retain defaults; malformed values always report the file and field.
	class SettingsReader {
	  private:
		template <class F> auto Read(const std::string& key, F operation) const {
			try {
				return operation();
			} catch (const std::exception& error) {
				throw std::runtime_error(path.string() + ": " + key + ": " + error.what());
			}
		}

	  public:
		SettingsReader(const Engine::JsonAssetReader& reader, std::filesystem::path path)
			: reader(reader), path(std::move(path)), exists(std::filesystem::exists(this->path)) {}
		void Number(
			const std::string& key, float& value, float minimum = 0.000001f, float maximum = 1000000) const {
			if (!exists) return;
			if (auto values = Read(key, [&] {
					return reader.TryResolveFloatArray(path, key);
				})) {
				Require(values->size() == 1, key, "expected a number");
				Require(std::isfinite(values->front()) && values->front() >= minimum &&
						values->front() <= maximum,
					key, "number outside supported range");
				value = values->front();
			}
		}
		template <class T> void Integer(const std::string& key, T& value, unsigned maximum) const {
			float number = static_cast<float>(value);
			Number(key, number, 1, static_cast<float>(maximum));
			Require(std::floor(number) == number, key, "expected an integer");
			value = static_cast<T>(number);
		}
		template <size_t N>
		void Array(const std::string& key, std::array<float, N>& value, float minimum = -1000000,
			float maximum = 1000000) const {
			if (!exists) return;
			// REDox numeric arrays support up to three components; read larger arrays by pointer.
			auto length = Read(key, [&] {
				return reader.TryResolveArrayLength(path, key);
			});
			if (!length) return;
			Require(*length == N, key, "unexpected array length");
			for (size_t i = 0; i < N; ++i)
				Number((key.starts_with("/") ? key : "/" + key) + "/" + std::to_string(i), value[i], minimum,
					maximum);
		}
		void Vector(const std::string& key, DirectX::XMFLOAT3& value) const {
			std::array<float, 3> v{value.x, value.y, value.z};
			Array(key, v);
			value = {v[0], v[1], v[2]};
		}
		void Color(const std::string& key, DirectX::XMFLOAT4& value) const {
			std::array<float, 4> v{value.x, value.y, value.z, value.w};
			Array(key, v, 0, 1);
			value = {v[0], v[1], v[2], v[3]};
		}
		void Text(const std::string& key, std::string& value) const {
			if (!exists) return;
			if (auto text = Read(key, [&] {
					return reader.TryResolveString(path, key);
				})) {
				Require(!text->empty() && text->size() <= 192, key, "expected 1..192 UTF-8 bytes");
				value = *text;
			}
		}
		void Require(bool valid, const std::string& key, const char* reason) const {
			if (!valid) throw std::runtime_error(path.string() + ": " + key + ": " + reason);
		}

	  private:
		const Engine::JsonAssetReader& reader;
		std::filesystem::path path;
		bool exists;
	};
} // namespace Game
