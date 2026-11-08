#pragma once
#include "../../EngineCommon.h"

namespace Engine {
	class RenderDevice {
	  public:
		// バックバッファ数
		static const UINT FrameCount = 2;
		static constexpr DXGI_FORMAT CloudFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		static constexpr UINT CloudResolutionDivisor = 3;

		RenderDevice() = default;
		~RenderDevice() = default;

		void Initialize(HWND hwnd, UINT width, UINT height);
		void Present();
		UINT GetWidth() const {
			return m_width;
		}
		UINT GetHeight() const {
			return m_height;
		}
		void Resize(UINT width, UINT height);

		void SetVerticalSync(bool enabled) {
			m_verticalSync = enabled;
		}
		bool IsVerticalSyncEnabled() const {
			return m_verticalSync;
		}

		ID3D12Device* GetDevice() const {
			return m_device.Get();
		}
		ID3D12CommandQueue* GetCommandQueue() const {
			return m_commandQueue.Get();
		}
		IDXGISwapChain3* GetSwapChain() const {
			return m_swapChain.Get();
		}
		const std::wstring& GetAdapterName() const {
			return m_adapterName;
		}

		// SRV 用ディスクリプタヒープから CPU ハンドルを割り当てる
		D3D12_CPU_DESCRIPTOR_HANDLE AllocateSrvDescriptor(UINT* outIndex = nullptr);
		D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle(UINT index) const;
		UINT GetSrvDescriptorSize() const {
			return m_srvDescriptorSize;
		}

		// SRV ヒープ本体へのアクセス（コマンドリストにセットするため）
		ID3D12DescriptorHeap* GetSrvDescriptorHeap() const {
			return m_srvHeap.Get();
		}

		UINT GetFrameIndex() const {
			return m_swapChain->GetCurrentBackBufferIndex();
		}
		ID3D12Resource* GetCurrentRenderTarget() const {
			return m_renderTargets[GetFrameIndex()].Get();
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRtvHandle() const;
		ID3D12Resource* GetSceneRenderTarget() const {
			return m_sceneRenderTargets[GetFrameIndex()].Get();
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetSceneRtvHandle() const;
		UINT GetCloudWidth() const {
			return (m_width + CloudResolutionDivisor - 1) / CloudResolutionDivisor;
		}
		UINT GetCloudHeight() const {
			return (m_height + CloudResolutionDivisor - 1) / CloudResolutionDivisor;
		}
		ID3D12Resource* GetCloudRenderTarget() const {
			return m_cloudRenderTargets[GetFrameIndex()].Get();
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetCloudRtvHandle() const;
		ID3D12Resource* GetSceneDepth() const {
			return m_sceneDepth.Get();
		}
		D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const {
			return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
		}
		// SRVはフレーム別の［シーンカラー、深度、低解像度の雲］を配置する
		static constexpr UINT SceneColorSrvIndex(UINT frame) {
			return frame * 3;
		}
		static constexpr UINT SceneDepthSrvIndex(UINT frame) {
			return SceneColorSrvIndex(frame) + 1;
		}
		static constexpr UINT CloudSrvIndex(UINT frame) {
			return SceneColorSrvIndex(frame) + 2;
		}
		static constexpr UINT ReservedSrvCount = FrameCount * 3;
		UINT GetSceneColorSrvIndex() const {
			return SceneColorSrvIndex(GetFrameIndex());
		}
		UINT GetSceneDepthSrvIndex() const {
			return SceneDepthSrvIndex(GetFrameIndex());
		}

	  private:
		static constexpr UINT SrvDescriptorCapacity = 4096;
		bool TryCreateHardwareDevice();
		void CreateDevice();
		void CreateCommandQueue();
		void CreateSwapChain(HWND hwnd, UINT width, UINT height);
		void CreateDescriptorHeaps();
		void CreateBackBufferViews();

		void CreateSceneResources();
		void CreateSceneRenderTargets();
		void CreateCloudRenderTargets();
		void CreateSceneDepth();
		void CreateSceneShaderResourceViews();
		ComPtr<IDXGIFactory4> m_factory;
		ComPtr<ID3D12Device> m_device;
		ComPtr<ID3D12CommandQueue> m_commandQueue;
		ComPtr<IDXGISwapChain3> m_swapChain;
		ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
		ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
		ComPtr<ID3D12DescriptorHeap> m_srvHeap; // CBV_SRV_UAV ヒープ（シェーダ可視）
		UINT m_srvDescriptorSize = 0;
		UINT m_srvDescriptorCount = 0; // 使用済みディスクリプタ数
		ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
		ComPtr<ID3D12Resource> m_sceneRenderTargets[FrameCount];
		ComPtr<ID3D12Resource> m_cloudRenderTargets[FrameCount];
		ComPtr<ID3D12Resource> m_sceneDepth;
		UINT m_rtvDescriptorSize = 0;
		UINT m_width = 0;
		UINT m_height = 0;
		std::wstring m_adapterName = L"Unknown";
		bool m_verticalSync = true;
		bool m_allowTearing = false;
	};
} // namespace Engine
