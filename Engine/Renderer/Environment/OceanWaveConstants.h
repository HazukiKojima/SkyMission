#ifndef SKYMISSION_OCEAN_WAVE_CONSTANTS_H
#define SKYMISSION_OCEAN_WAVE_CONSTANTS_H

#ifdef __cplusplus
namespace Engine {
#define OCEAN_WAVE_CONSTANT inline constexpr
#else
#define OCEAN_WAVE_CONSTANT static const
#endif

// CPUで計算する海面の高さと、頂点シェーダーの波に共通する値。
OCEAN_WAVE_CONSTANT int kOceanGridResolution = 257;
OCEAN_WAVE_CONSTANT float kOceanGridExponent = 5.5f;
OCEAN_WAVE_CONSTANT int kOceanWaveCount = 6;
OCEAN_WAVE_CONSTANT float kOceanWaveGravity = 9.8f;
OCEAN_WAVE_CONSTANT float kOceanWindZ = 0.6f;
OCEAN_WAVE_CONSTANT float kOceanWavePhaseStride = 21.53f;
OCEAN_WAVE_CONSTANT float kOceanWaveDirectionStride = 7.3f;
OCEAN_WAVE_CONSTANT float kOceanWaveDirectionSpread = 0.22f;
OCEAN_WAVE_CONSTANT float kOceanWaveTimeScale = 3.0f;
OCEAN_WAVE_CONSTANT float kOceanWavelengthDecay = 0.82f;
OCEAN_WAVE_CONSTANT float kOceanWaveSteepnessDecay = 0.72f;

#undef OCEAN_WAVE_CONSTANT
#ifdef __cplusplus
}
#endif
#endif
