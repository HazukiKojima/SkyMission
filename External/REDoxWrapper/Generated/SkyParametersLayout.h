#pragma once
#include "../../../Engine/Core/SkyParameters.h"
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<Engine::SkyParameters>);
static_assert(offsetof(Engine::SkyParameters, radius) == 0 * sizeof(float));
static_assert(offsetof(Engine::SkyParameters, sunDirection) == 1 * sizeof(float));
static_assert(offsetof(Engine::SkyParameters, sunIntensity) == 4 * sizeof(float));
static_assert(offsetof(Engine::SkyParameters, sunColor) == 5 * sizeof(float));
static_assert(offsetof(Engine::SkyParameters, ambientIntensity) == 8 * sizeof(float));
static_assert(offsetof(Engine::SkyParameters, ambientColor) == 9 * sizeof(float));
static_assert(sizeof(Engine::SkyParameters) == 12 * sizeof(float));
