#pragma once
#include "../EngineCommon.h"

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

		void TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);

		ID3D12GraphicsCommandList* GetCommandList() const { return m_commandList.Get(); }

	private:
		ID3D12CommandQueue* m_targetQueue = nullptr;
		ComPtr<ID3D12GraphicsCommandList> m_commandList;
		ComPtr<ID3D12CommandAllocator>    m_commandAllocators[2];

		ComPtr<ID3D12Fence> m_fence;
		HANDLE              m_fenceEvent = nullptr;
		UINT64              m_fenceValues[2] = { 0 };
		UINT                m_currentFrameIndex = 0;
	};
}