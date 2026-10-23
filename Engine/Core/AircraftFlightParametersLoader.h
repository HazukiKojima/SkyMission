#pragma once

#include "AircraftFlightParameters.h"
#include <filesystem>

namespace Engine {
	class AircraftFlightParametersLoader {
	public:
		AircraftFlightParameters Load(const std::filesystem::path& jsonPath) const;
	};
}
