#pragma once
#include "../../EngineCommon.h"
#include "Engine/Renderer/Device/RenderDevice.h"
#include "../RetroFilmParameters.h"
#include <vector>

namespace Engine {
	class CommandContext;

	// GPUへそのまま転送するため、メンバーの順序とパディングを対応するHLSLのcbufferに揃える
	struct OceanConstantBufferData {
		DirectX::XMFLOAT4X4 mvp;
		float time;
		float oceanOffset[3];

		DirectX::XMFLOAT3 cameraPos;
		float pad2;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float pad3;
		float oceanSize;
		float oceanWaveLength;
		float oceanWaveSteepness;
		float oceanNormalUvScale;
		float oceanWavePatternUvScale;
		float oceanFogStartDistance;
		float oceanFogEndDistance;
		float oceanEdgeFadeStart;
		float oceanEdgeFadeWidth;
		float oceanWaveHeightOffset;
	};

	struct OceanFogConstantBufferData {
		DirectX::XMFLOAT4X4 inverseViewProjection;
		DirectX::XMFLOAT3 cameraPosition;
		float padding0;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float oceanFogBottom;
		float oceanFogTop;
		float oceanFogDensity;
		float oceanFogStartDistance;
		float oceanFogEndDistance;
		float oceanEdgeFadeStart;
		float oceanEdgeFadeWidth;
		float padding1[2];

		DirectX::XMFLOAT4 cloudLayer; // bottom, top, coverage, extinction density
		DirectX::XMFLOAT4 cloudMotion; // 逆スケール、X/Z方向の風、有効フラグ
		DirectX::XMFLOAT4 cloudQuality; // 近距離レイの最大距離、ステップ数、シード、経過秒数
	};

	struct ModelConstantBufferData {
		DirectX::XMFLOAT4X4 mvp;
		DirectX::XMFLOAT4X4 world;
		float time;
		float padding[3];

		DirectX::XMFLOAT3 cameraPos;
		float pad2;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float pad3;
		DirectX::XMFLOAT4 localClipPlane;
	};

	struct FilmConstantBufferData {
		float time;
		float padding[3];
		RetroFilmParameters parameters;
	};

	class RendererConstantBuffers {
	  public:
		void Initialize(ID3D12Device* device);
		void EnsureModelCapacity(ID3D12Device* device, CommandContext* context, size_t objectCount);

		size_t GetModelCapacity() const {
			return m_modelBuffers[0].size();
		}

		ID3D12Resource* GetOceanResource(UINT frame) const {
			return m_oceanBuffers[frame].resource.Get();
		}

		OceanConstantBufferData* GetOceanData(UINT frame) const {
			return reinterpret_cast<OceanConstantBufferData*>(m_oceanBuffers[frame].mappedData);
		}

		ID3D12Resource* GetOceanFogResource(UINT frame) const {
			return m_oceanFogBuffers[frame].resource.Get();
		}

		OceanFogConstantBufferData* GetOceanFogData(UINT frame) const {
			return reinterpret_cast<OceanFogConstantBufferData*>(m_oceanFogBuffers[frame].mappedData);
		}

		ID3D12Resource* GetFilmResource(UINT frameIndex) const {
			return m_filmBuffers[frameIndex].resource.Get();
		}

		FilmConstantBufferData* GetFilmData(UINT frameIndex) const {
			return reinterpret_cast<FilmConstantBufferData*>(m_filmBuffers[frameIndex].mappedData);
		}

		ID3D12Resource* GetModelResource(UINT frameIndex, size_t objectIndex) const {
			return m_modelBuffers[frameIndex][objectIndex].resource.Get();
		}

		ModelConstantBufferData* GetModelData(UINT frameIndex, size_t objectIndex) const {
			return reinterpret_cast<ModelConstantBufferData*>(
				m_modelBuffers[frameIndex][objectIndex].mappedData);
		}

	  private:
		struct MappedBuffer {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			UINT8* mappedData = nullptr;
		};

		MappedBuffer m_oceanBuffers[RenderDevice::FrameCount];
		MappedBuffer m_oceanFogBuffers[RenderDevice::FrameCount];
		MappedBuffer m_filmBuffers[RenderDevice::FrameCount];
		std::vector<MappedBuffer> m_modelBuffers[RenderDevice::FrameCount];
	};
} // namespace Engine
