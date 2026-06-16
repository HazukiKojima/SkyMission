#pragma once
#include "../EngineCommon.h"

namespace Engine {
	class RenderDevice {
	public:
		// バックバッファ数
		static const UINT FrameCount = 2;

		RenderDevice() = default;
		~RenderDevice() = default;

		void Initialize(HWND hwnd, UINT width, UINT height);
		void Present();

		ID3D12Device* GetDevice() const { return m_device.Get(); }
		ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
		IDXGISwapChain3* GetSwapChain() const { return m_swapChain.Get(); }

		UINT GetFrameIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
		ID3D12Resource* GetCurrentRenderTarget() const { return m_renderTargets[GetFrameIndex()].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtvHandle() const;

	private:
		ComPtr<IDXGIFactory4>        m_factory;
		ComPtr<ID3D12Device>         m_device;
		ComPtr<ID3D12CommandQueue>   m_commandQueue;
		ComPtr<IDXGISwapChain3>      m_swapChain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12Resource>       m_renderTargets[FrameCount];
		UINT                         m_rtvDescriptorSize = 0;
	};
}