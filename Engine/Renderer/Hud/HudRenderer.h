#pragma once
#include "HudDrawList.h"
#include "Engine/Renderer/Device/RenderDevice.h"
#include "Engine/Resources/Texture/Texture.h"

namespace Engine {
	// フォントアトラスとフレーム別頂点バッファを所有し、HUDのGPU描画を記録する
	class HudRenderer {
	  public:
		void Initialize(RenderDevice& device, ID3D12GraphicsCommandList* commands);
		// Initializeで記録した転送のGPU完了を待ってから呼び出す
		void ReleaseUploadResources() {
			m_atlas.ReleaseUploadResources();
		}
		// 描画命令の記録のみを行う。コマンドの送信と同期は呼び出し側が管理する
		void Record(RenderDevice& device, ID3D12GraphicsCommandList* commands, const HudDrawList& list);

	  private:
		Texture m_atlas;
		UINT m_srv = 0;
		ComPtr<ID3D12RootSignature> m_root;
		ComPtr<ID3D12PipelineState> m_pipeline;
		ComPtr<ID3D12Resource> m_vertices[RenderDevice::FrameCount];
		UINT8* m_mapped[RenderDevice::FrameCount]{};
	};
} // namespace Engine
