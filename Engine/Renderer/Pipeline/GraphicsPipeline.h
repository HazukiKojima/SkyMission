#pragma once
#include "../../EngineCommon.h"

namespace Engine {
	// 描画パイプラインを構築・管理するクラス
	// ルートシグネチャとPSOを生成し、外部から取得できるようにする
	class GraphicsPipeline {
	public:
		// デフォルトのシェーダを用いて初期化
		void Initialize(ID3D12Device* device);
		// 指定したシェーダファイルを使用して初期化
		void InitializeWithShaders(ID3D12Device* device, const std::wstring& vsPath, const std::wstring& psPath);

		ID3D12PipelineState* GetPSO() const { return m_pso.Get(); }
		ID3D12RootSignature* GetRootSignature() const { return m_rootSignature.Get(); }

	private:
		ComPtr<ID3D12RootSignature> m_rootSignature;
		ComPtr<ID3D12PipelineState> m_pso;
	};
}
