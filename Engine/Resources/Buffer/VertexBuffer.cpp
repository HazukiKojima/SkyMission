#include "VertexBuffer.h"

namespace Engine {
	void VertexBuffer::Initialize(ID3D12Device* device, const void* data, UINT size, UINT stride) {
		// GPUへデータを転送するためのUploadHeap設定
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(size);

		ThrowIfFailed(device->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_buffer)));

		// CPUメモリのデータをGPUバッファへコピー
		void* pMappedData;
		m_buffer->Map(0, nullptr, &pMappedData);
		memcpy(pMappedData, data, size);
		m_buffer->Unmap(0, nullptr);

		// 描画時に必要なView情報を構築
		m_view.BufferLocation = m_buffer->GetGPUVirtualAddress();
		m_view.StrideInBytes = stride;
		m_view.SizeInBytes = size;
	}
}
