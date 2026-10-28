#pragma once
#include "../../EngineCommon.h"
#include "../../Resources/Buffer/VertexBuffer.h"

namespace Engine {
	class OceanGeometry {
	public:
		void Initialize(ID3D12Device* device, float oceanSize);

		D3D12_VERTEX_BUFFER_VIEW GetVertexView() const { return m_vertexBuffer->GetView(); }
		const D3D12_INDEX_BUFFER_VIEW& GetIndexView() const { return m_indexBufferView; }
		UINT GetIndexCount() const { return m_indexCount; }

		static float CalculateWaveHeight(float x, float z, float time, float wavelength, float steepness);

	private:
		std::unique_ptr<VertexBuffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
		UINT m_indexCount = 0;
	};
}
