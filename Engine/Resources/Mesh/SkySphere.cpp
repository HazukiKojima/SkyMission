#include "SkySphere.h"
#include "../../../d3dx12.h"

namespace Engine {
	void SkySphere::Initialize(ID3D12Device* device, float radius, UINT slices, UINT stacks) {
		using namespace DirectX;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		// 球体の頂点を生成
		for (UINT i = 0; i <= stacks; ++i) {
			float phi = XM_PI * i / stacks;
			float sinPhi = sin(phi);
			float cosPhi = cos(phi);

			for (UINT j = 0; j <= slices; ++j) {
				float theta = 2.0f * XM_PI * j / slices;
				float sinTheta = sin(theta);
				float cosTheta = cos(theta);

				Vertex v;
				v.position.x = radius * sinPhi * cosTheta;
				v.position.y = radius * cosPhi;
				v.position.z = radius * sinPhi * sinTheta;

				v.texcoord.x = static_cast<float>(j) / slices;
				v.texcoord.y = static_cast<float>(i) / stacks;

				vertices.push_back(v);
			}
		}

		// インデックスを生成
		for (UINT i = 0; i < stacks; ++i) {
			for (UINT j = 0; j < slices; ++j) {
				uint32_t a = i * (slices + 1) + j;
				uint32_t b = a + 1;
				uint32_t c = (i + 1) * (slices + 1) + j;
				uint32_t d = c + 1;

				// 最初の三角形
				indices.push_back(a);
				indices.push_back(c);
				indices.push_back(b);

				// 2番目の三角形
				indices.push_back(b);
				indices.push_back(c);
				indices.push_back(d);
			}
		}

		m_indexCount = static_cast<UINT>(indices.size());

		// 頂点バッファを作成
		{
			UINT64 vertexBufferSize = vertices.size() * sizeof(Vertex);
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			auto desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
			ThrowIfFailed(device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_vertexBuffer)));

			void* pData;
			m_vertexBuffer->Map(0, nullptr, &pData);
			memcpy(pData, vertices.data(), vertexBufferSize);
			m_vertexBuffer->Unmap(0, nullptr);

			m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
			m_vertexBufferView.StrideInBytes = sizeof(Vertex);
			m_vertexBufferView.SizeInBytes = static_cast<UINT>(vertexBufferSize);
		}

		// インデックスバッファを作成
		{
			UINT64 indexBufferSize = indices.size() * sizeof(uint32_t);
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			auto desc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
			ThrowIfFailed(device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_indexBuffer)));

			void* pData;
			m_indexBuffer->Map(0, nullptr, &pData);
			memcpy(pData, indices.data(), indexBufferSize);
			m_indexBuffer->Unmap(0, nullptr);

			m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
			m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			m_indexBufferView.SizeInBytes = static_cast<UINT>(indexBufferSize);
		}
	}
}
