#pragma once
#include "../../../Engine/Scene/OceanParameters.h"
#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<Engine::OceanParameters>);
static_assert(offsetof(Engine::OceanParameters, size) == 0 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, waveLength) == 1 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, waveSteepness) == 2 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, normalUvScale) == 3 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, wavePatternUvScale) == 4 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, fogStartDistance) == 5 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, fogEndDistance) == 6 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, edgeFadeStart) == 7 * sizeof(float));
static_assert(offsetof(Engine::OceanParameters, edgeFadeWidth) == 8 * sizeof(float));
static_assert(sizeof(Engine::OceanParameters) == 9 * sizeof(float));
