#pragma once
#include "../../EngineCommon.h"
#include "../RootBindings.h"

namespace Engine {
	struct PipelineOptions {
		std::wstring vs = L"BasicVS.cso";
		std::wstring ps = L"BasicPS.cso";
		UINT srvCount = 1;
		bool alphaBlend = false;
		bool depth = false;
		bool vertexNormalTangent = false;
		bool depthWrite = true;
		bool additiveBlend = false;
		D3D12_CULL_MODE cullMode = D3D12_CULL_MODE_NONE;
		D3D12_TEXTURE_ADDRESS_MODE addressMode = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		RootSignatureLayout rootLayout = RootSignatureLayout::Textured;
	};

	class GraphicsPipeline {
	  public:
		void Initialize(ID3D12Device* device, const PipelineOptions& options = {});

		ID3D12PipelineState* GetPSO() const {
			return m_pso.Get();
		}
		ID3D12RootSignature* GetRootSignature() const {
			return m_rootSignature.Get();
		}

	  private:
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pso;
	};
} // namespace Engine
