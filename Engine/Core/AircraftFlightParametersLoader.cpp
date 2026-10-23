#include "AircraftFlightParametersLoader.h"
#include "../../External/REDoxWrapper/Generated/AircraftFlightParametersLayout.h"
#include <Windows.h>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

namespace Engine {
	namespace {
		using RedoxGetAircraftFlightParameters = int(__cdecl*)(
			const unsigned char*, int, float*, int);
	}

	AircraftFlightParameters AircraftFlightParametersLoader::Load(
		const std::filesystem::path& jsonPath) const
	{
		std::ifstream file(jsonPath, std::ios::binary);
		if (!file) {
			throw std::runtime_error("Failed to open aircraft flight parameters JSON.");
		}

		std::vector<char> json(
			(std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		if (json.empty() || json.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
			throw std::runtime_error("Aircraft flight parameters JSON is empty or too large.");
		}

		std::vector<float> values(sizeof(AircraftFlightParameters) / sizeof(float));
		HMODULE module = LoadLibraryW(L"REDoxWrapper.dll");
		if (!module) {
			throw std::runtime_error("Failed to load REDoxWrapper.dll.");
		}

		auto getParameters = reinterpret_cast<RedoxGetAircraftFlightParameters>(
			GetProcAddress(module, "MyRedoxGetAircraftFlightParameters"));
		if (!getParameters) {
			FreeLibrary(module);
			throw std::runtime_error("Failed to find MyRedoxGetAircraftFlightParameters.");
		}

		const int result = getParameters(
			reinterpret_cast<const unsigned char*>(json.data()),
			static_cast<int>(json.size()),
			values.data(),
			static_cast<int>(values.size()));
		FreeLibrary(module);
		if (result != static_cast<int>(values.size())) {
			throw std::runtime_error("Failed to deserialize aircraft flight parameters JSON.");
		}

		AircraftFlightParameters parameters;
		std::memcpy(&parameters, values.data(), sizeof(parameters));
		return parameters;
	}
}
