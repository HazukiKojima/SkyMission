#pragma once
#include "Engine/Resources/JsonAssetReader.h"
#include <DirectXMath.h>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <array>
namespace Game {
	// 欠けた項目は既定値のまま。不正な値は、必ずファイル名と項目名を報告する
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
			const std::string& key, float& value, float minimum = 0.000001f, float maximum = 1000000) const;

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

			// REDoxの数値配列は最大3成分まで。それより大きい配列はポインタで読む
			auto length = Read(key, [&] {
				return reader.TryResolveArrayLength(path, key);
			});

			if (!length) return;

			Require(*length == N, key, "unexpected array length");

			for (size_t i = 0; i < N; ++i)
				Number((key.starts_with("/") ? key : "/" + key) + "/" + std::to_string(i), value[i], minimum,
					maximum);
		}

		void Vector(const std::string& key, DirectX::XMFLOAT3& value) const;

		void Color(const std::string& key, DirectX::XMFLOAT4& value) const;

		void Text(const std::string& key, std::string& value) const;

		void Require(bool valid, const std::string& key, const char* reason) const;

	  private:
		const Engine::JsonAssetReader& reader;
		std::filesystem::path path;
		bool exists;
	};
} // namespace Game
