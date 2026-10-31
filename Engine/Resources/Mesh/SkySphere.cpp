#include "SkySphere.h"
#include "../Buffer/BufferUtilities.h"

namespace Engine {
	void SkySphere::Initialize(
		ID3D12Device* device, ID3D12GraphicsCommandList* commands, float radius, UINT slices, UINT stacks) {
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

				indices.push_back(a);
				indices.push_back(c);
				indices.push_back(b);

				indices.push_back(b);
				indices.push_back(c);
				indices.push_back(d);
			}
		}

		m_indexCount = static_cast<UINT>(indices.size());

		const UINT64 vertexBytes = vertices.size() * sizeof(Vertex);
		const UINT64 indexBytes = indices.size() * sizeof(uint32_t);
		m_vertexBuffer = CreateStaticBuffer(device, commands, vertices.data(), vertexBytes,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, m_vertexUpload);
		m_indexBuffer = CreateStaticBuffer(
			device, commands, indices.data(), indexBytes, D3D12_RESOURCE_STATE_INDEX_BUFFER, m_indexUpload);
		m_vertexBufferView = {
			m_vertexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(vertexBytes), sizeof(Vertex)};
		m_indexBufferView = {
			m_indexBuffer->GetGPUVirtualAddress(), static_cast<UINT>(indexBytes), DXGI_FORMAT_R32_UINT};
	}

	void SkySphere::ReleaseUploadResources() {
		m_vertexUpload.Reset();
		m_indexUpload.Reset();
	}
} // namespace Engine
