#pragma once
#include "../../EngineCommon.h"

namespace Engine {
	class GraphicsPipeline {
	public:
		void Initialize(ID3D12Device* device);
		// 指定したシェーダファイルを使用して初期化
		void InitializeWithShaders(ID3D12Device* device, const std::wstring& vsPath, const std::wstring& psPath, UINT srvCount = 1);

		ID3D12PipelineState* GetPSO() const { return m_pso.Get(); }
		ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

	private:
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pso;
	};
}
