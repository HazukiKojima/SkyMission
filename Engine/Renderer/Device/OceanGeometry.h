#pragma once
#include "../../EngineCommon.h"
#include "../../Resources/Buffer/VertexBuffer.h"

namespace Engine {
	class OceanGeometry {
	  public:
		void Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commands, float oceanSize);
		// 転送コマンドの GPU 完了後に呼ぶ
		void ReleaseUploadResources();
		ID3D12Resource* GetVertexBufferResource() const {
			return m_vertexBuffer->GetResource();
		}
		ID3D12Resource* GetIndexBufferResource() const {
			return m_indexBuffer.Get();
		}

		D3D12_VERTEX_BUFFER_VIEW GetVertexView() const {
			return m_vertexBuffer->GetView();
		}
		const D3D12_INDEX_BUFFER_VIEW& GetIndexView() const {
			return m_indexBufferView;
		}
		UINT GetIndexCount() const {
			return m_indexCount;
		}

		static float CalculateWaveHeight(float x, float z, float time, float wavelength, float steepness);

	  private:
		std::unique_ptr<VertexBuffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexUpload;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
		UINT m_indexCount = 0;
	};
} // namespace Engine
