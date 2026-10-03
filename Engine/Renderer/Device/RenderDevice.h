#pragma once
#include "../../EngineCommon.h"

namespace Engine {
	class RenderDevice {
	public:
		// バックバッファ数
		static const UINT FrameCount = 2;

		RenderDevice() = default;
		~RenderDevice() = default;

		void Initialize(HWND hwnd, UINT width, UINT height);
		void Present();
		void Resize(UINT width, UINT height); // add resize

		ID3D12Device* GetDevice() const { return m_device.Get(); }
		ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
		IDXGISwapChain3* GetSwapChain() const { return m_swapChain.Get(); }

		// SRV 用ディスクリプタヒープから CPU ハンドルを割り当てる
		D3D12_CPU_DESCRIPTOR_HANDLE AllocateSrvDescriptor(UINT* outIndex = nullptr);
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(UINT index) const;
		UINT GetSrvDescriptorSize() const { return m_srvDescriptorSize; }

		// SRV ヒープ本体へのアクセス（コマンドリストにセットするため）
		ID3D12DescriptorHeap* GetSrvDescriptorHeap() const { return m_srvHeap.Get(); }

		UINT GetFrameIndex() const { return m_swapChain->GetCurrentBackBufferIndex(); }
		ID3D12Resource* GetCurrentRenderTarget() const { return m_renderTargets[GetFrameIndex()].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtvHandle() const;
		ID3D12Resource* GetSceneRenderTarget() const { return m_sceneRenderTargets[GetFrameIndex()].Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetSceneRtvHandle() const;
		ID3D12Resource* GetSceneDepth() const { return m_sceneDepth.Get(); }
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const { return m_dsvHeap->GetCPUDescriptorHandleForHeapStart(); }
		UINT GetSceneColorSrvIndex() const { return GetFrameIndex() * 2; }
		UINT GetSceneDepthSrvIndex() const { return GetFrameIndex() * 2 + 1; }

	private:
		void CreateSceneResources();
		ComPtr<IDXGIFactory4>        m_factory;
		ComPtr<ID3D12Device>         m_device;
		ComPtr<ID3D12CommandQueue>   m_commandQueue;
		ComPtr<IDXGISwapChain3>      m_swapChain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap; // CBV_SRV_UAV ヒープ（シェーダ可視）
		UINT                         m_srvDescriptorSize = 0;
		UINT                         m_srvDescriptorCount = 0; // 使用済みディスクリプタ数
		ComPtr<ID3D12Resource>       m_renderTargets[FrameCount];
		ComPtr<ID3D12Resource>       m_sceneRenderTargets[FrameCount];
		ComPtr<ID3D12Resource>       m_sceneDepth;
		UINT                         m_rtvDescriptorSize = 0;
		UINT                         m_width = 0;
		UINT                         m_height = 0;

		ComPtr<ID3D12Resource> m_cloudRenderTarget;

		UINT m_cloudSrvIndex = 0;

		void CreateCloudResources();

		D3D12_CPU_DESCRIPTOR_HANDLE GetCloudRtvHandle() const;

		ID3D12Resource* GetCloudRenderTarget() const {
			return m_cloudRenderTarget.Get();
		}

		UINT GetCloudSrvIndex() const {
			return m_cloudSrvIndex;
		}
	};
}