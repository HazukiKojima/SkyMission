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
		// sizeは総バイト数、strideは1頂点のバイト数
		// commandsへコピーを記録し、実行とGPU完了待機は呼び出し元が行う
		void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commands, const void* data, UINT size,
						UINT stride);
		// 呼び出し元が転送コマンドの GPU 完了を待機した後に呼ぶ
		void ReleaseUploadResources() {
			m_upload.Reset();
		}
		ID3D12Resource* GetResource() const {
			return m_buffer.Get();
		}

		// コマンドリスト設定用Viewの取得
		D3D12_VERTEX_BUFFER_VIEW GetView() const {
			return m_view;
		}

	  private:
		ComPtr<ID3D12Resource> m_buffer;
		ComPtr<ID3D12Resource> m_upload;
		D3D12_VERTEX_BUFFER_VIEW m_view = {};
	};
} // namespace Engine
