#pragma once
#include "../../EngineCommon.h"

namespace Engine {
	class GraphicsPipeline {
	public:
		void Initialize(ID3D12Device* device);

		ID3D12PipelineState* GetPSO() const { return m_pso.Get(); }
		ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

	private:
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pso;
	};
}
