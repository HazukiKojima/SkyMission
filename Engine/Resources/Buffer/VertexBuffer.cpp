#include "VertexBuffer.h"
#include "BufferUtilities.h"

namespace Engine {
	void VertexBuffer::Initialize(
		ID3D12Device* device, ID3D12GraphicsCommandList* commands, const void* data, UINT size, UINT stride) {
		m_buffer = CreateStaticBuffer(
			device, commands, data, size, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, m_upload);

		// 描画時に必要なView情報を構築
		m_view.BufferLocation = m_buffer->GetGPUVirtualAddress();
		m_view.StrideInBytes = stride;
		m_view.SizeInBytes = size;
	}
} // namespace Engine
