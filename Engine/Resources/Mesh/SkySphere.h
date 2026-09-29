#pragma once
#include "../../EngineCommon.h"
#include <vector>
#include <DirectXMath.h>

namespace Engine {
	// SkySphere: 球体メッシュを生成し、GPU上での描画をサポートするクラス
	class SkySphere {
	public:
		struct Vertex {
			DirectX::XMFLOAT3 position;
			DirectX::XMFLOAT2 texcoord;
		};

		SkySphere() = default;
		~SkySphere() = default;

		// 球体メッシュを生成し、GPUリソースを作成
		// @param device: GPUデバイス
		// @param radius: 球体の半径
		// @param slices: 経度方向の分割数
		// @param stacks: 緯度方向の分割数
		void Initialize(ID3D12Device* device, float radius = 100.0f, UINT slices = 64, UINT stacks = 32);

		// 頂点バッファビューを取得
		D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_vertexBufferView; }

		// インデックスバッファビューを取得
		const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return m_indexBufferView; }

		// インデックス数を取得
		UINT GetIndexCount() const { return m_indexCount; }

	private:
		ComPtr<ID3D12Resource> m_vertexBuffer;
		ComPtr<ID3D12Resource> m_indexBuffer;
		D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView = {};
		UINT m_indexCount = 0;
	};
}
