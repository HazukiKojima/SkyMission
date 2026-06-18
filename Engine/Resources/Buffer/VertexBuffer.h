#pragma once
#include "../../EngineCommon.h"

namespace Engine {
	// GPUメモリ上の頂点データを管理するクラス
	// 描画コマンドで必要となるD3D12_VERTEX_BUFFER_VIEWを内包する
	class VertexBuffer {
	public:
		VertexBuffer() = default;
		~VertexBuffer() = default;

		// 頂点バッファの生成とGPUメモリへのデータ転送
		// @param device: GPUデバイス
		// @param data: 頂点データの配列ポインタ
		// @param size: バッファ全体のサイズ（バイト）
		// @param stride: 1頂点あたりのサイズ（バイト）
		void Initialize(ID3D12Device* device, const void* data, UINT size, UINT stride);

		// コマンドリスト設定用Viewの取得
		D3D12_VERTEX_BUFFER_VIEW GetView() const { return m_view; }

	private:
		ComPtr<ID3D12Resource> m_buffer;
		D3D12_VERTEX_BUFFER_VIEW m_view = {};
	};
}
