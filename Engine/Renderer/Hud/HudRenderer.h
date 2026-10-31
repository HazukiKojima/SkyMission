#pragma once
#include "HudDrawList.h"
#include "Engine/Renderer/Device/RenderDevice.h"
#include "Engine/Resources/Texture/Texture.h"

namespace Engine {
	class HudRenderer {
	  public:
		void Initialize(RenderDevice& device, ID3D12GraphicsCommandList* commands);
		void ReleaseUploadResources() {
			m_atlas.ReleaseUploadResources();
		}
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
