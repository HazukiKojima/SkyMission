#include "VertexBuffer.h"
#include "BufferUtilities.h"

namespace Engine {
	void VertexBuffer::Initialize(ID3D12Device* device, const void* data, UINT size, UINT stride) {
		// Procedural geometry currently has no initialization command list; retain the
		// upload allocation here. Imported static meshes use DEFAULT heaps.
		m_buffer = CreateInitializedUploadBuffer(device, data, size);

		// 描画時に必要なView情報を構築
		m_view.BufferLocation = m_buffer->GetGPUVirtualAddress();
		m_view.StrideInBytes = stride;
		m_view.SizeInBytes = size;
	}
} // namespace Engine
