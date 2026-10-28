#pragma once
#include "RenderDevice.h"

namespace Engine {
	class RenderDevice;

	class CommandContext {
	  public:
		CommandContext() = default;
		~CommandContext();

		void Initialize(RenderDevice* renderDevice);

		void BeginFrame();
		void EndFrame();
		void MoveToNextFrame(RenderDevice* renderDevice);
		void WaitForGpu();
		// Call only after WaitForGpu; resize can reset the swapchain index to zero.
		void SynchronizeFrameIndex(const RenderDevice& device) {
			const UINT64 nextFenceValue = m_fenceValues[m_currentFrameIndex];
			for (auto& value : m_fenceValues)
				value = nextFenceValue;
			m_currentFrameIndex = device.GetFrameIndex();
		}

		void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore,
								D3D12_RESOURCE_STATES stateAfter);

		ID3D12GraphicsCommandList* GetCommandList() const {
			return m_commandList.Get();
		}
		UINT GetFrameIndex() const {
			return m_renderDevice->GetFrameIndex();
		}

	  private:
		RenderDevice* m_renderDevice = nullptr;
		ID3D12CommandQueue* m_targetQueue = nullptr;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12CommandAllocator> m_commandAllocators[RenderDevice::FrameCount];

		ComPtr<ID3D12Fence> m_fence;
		HANDLE m_fenceEvent = nullptr;
		UINT64 m_fenceValues[RenderDevice::FrameCount] = {0};
		UINT m_currentFrameIndex = 0;
	};
} // namespace Engine