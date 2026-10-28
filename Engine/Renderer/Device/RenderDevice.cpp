#include "RenderDevice.h"
#include <stdexcept>

namespace Engine {

	// ============================================================
	// Initialize
	// ============================================================

	void RenderDevice::Initialize(HWND hwnd, UINT width, UINT height) {
#if defined(_DEBUG)
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
		}
#endif

		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));
		HRESULT deviceResult = E_FAIL;
		ComPtr<IDXGIFactory6> factory6;
		const bool hasGpuPreferenceApi = SUCCEEDED(m_factory.As(&factory6));

		for (UINT index = 0; ; ++index) {
			ComPtr<IDXGIAdapter1> adapter;
			HRESULT adapterResult = hasGpuPreferenceApi
				? factory6->EnumAdapterByGpuPreference(
					index,
					DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
					IID_PPV_ARGS(&adapter))
				: m_factory->EnumAdapters1(index, &adapter);
			if (adapterResult == DXGI_ERROR_NOT_FOUND) {
				break;
			}
			ThrowIfFailed(adapterResult);

			DXGI_ADAPTER_DESC1 description = {};
			ThrowIfFailed(adapter->GetDesc1(&description));
			if ((description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) {
				continue;
			}

			deviceResult = D3D12CreateDevice(
				adapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&m_device)
			);
			if (SUCCEEDED(deviceResult)) {
				m_adapterName = description.Description;
				break;
			}
		}

		if (FAILED(deviceResult)) {
			// Hardware adapter が利用できない環境でも起動できるよう WARP を試す。
			ComPtr<IDXGIAdapter> warpAdapter;
			ThrowIfFailed(m_factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
			deviceResult = D3D12CreateDevice(
				warpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&m_device)
			);
			m_adapterName = L"WARP (Software Rasterizer)";
		}
		ThrowIfFailed(deviceResult);

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(m_device->CreateCommandQueue(
			&queueDesc,
			IID_PPV_ARGS(&m_commandQueue)
		));

		// ========================================================
		// SwapChain
		// ========================================================

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.SampleDesc.Count = 1;

		ComPtr<IDXGISwapChain1> swapChain;

		ThrowIfFailed(m_factory->CreateSwapChainForHwnd(
			m_commandQueue.Get(),
			hwnd,
			&swapChainDesc,
			nullptr,
			nullptr,
			&swapChain
		));

		ThrowIfFailed(swapChain.As(&m_swapChain));

		// ========================================================
		// RTV Heap
		//
		// BackBuffer, SceneColor, PostProcessColor の順に各フレーム分を配置する。
		//
		// ========================================================

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount * 3;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		ThrowIfFailed(m_device->CreateDescriptorHeap(
			&rtvHeapDesc,
			IID_PPV_ARGS(&m_rtvHeap)
		));

		m_rtvDescriptorSize =
			m_device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV
			);

		// BackBuffer RTV
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart()
		);

		for (UINT i = 0; i < FrameCount; ++i) {
			ThrowIfFailed(m_swapChain->GetBuffer(
				i,
				IID_PPV_ARGS(&m_renderTargets[i])
			));

			m_device->CreateRenderTargetView(
				m_renderTargets[i].Get(),
				nullptr,
				rtvHandle
			);

			rtvHandle.ptr += m_rtvDescriptorSize;
		}

		// ========================================================
		// SRV Heap
		// ========================================================

		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = SrvDescriptorCapacity;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

		ThrowIfFailed(m_device->CreateDescriptorHeap(
			&srvHeapDesc,
			IID_PPV_ARGS(&m_srvHeap)
		));

		m_srvDescriptorSize =
			m_device->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
			);

		// ========================================================
		// DSV Heap
		// ========================================================

		D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
		dsvHeapDesc.NumDescriptors = 1;
		dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		ThrowIfFailed(m_device->CreateDescriptorHeap(
			&dsvHeapDesc,
			IID_PPV_ARGS(&m_dsvHeap)
		));

		m_width = width;
		m_height = height;

		m_srvDescriptorCount = FrameCount * 3;
		CreateSceneResources();
	}


	// ============================================================
	// RTV
	// ============================================================

	D3D12_CPU_DESCRIPTOR_HANDLE RenderDevice::GetSceneRtvHandle() const {
		D3D12_CPU_DESCRIPTOR_HANDLE handle(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart()
		);

		handle.ptr += static_cast<SIZE_T>(
			FrameCount + GetFrameIndex()
			) * m_rtvDescriptorSize;

		return handle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RenderDevice::GetPostProcessRtvHandle() const {
		D3D12_CPU_DESCRIPTOR_HANDLE handle(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart()
		);

		handle.ptr += static_cast<SIZE_T>(
			FrameCount * 2 + GetFrameIndex()
			) * m_rtvDescriptorSize;

		return handle;
	}

	// ============================================================
	// Present
	// ============================================================

	void RenderDevice::Present() {
		ThrowIfFailed(m_swapChain->Present(1, 0));
	}


	D3D12_CPU_DESCRIPTOR_HANDLE RenderDevice::GetCurrentRtvHandle() const {
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart()
		);

		rtvHandle.ptr +=
			GetFrameIndex() * m_rtvDescriptorSize;

		return rtvHandle;
	}


	// ============================================================
	// SRV
	// ============================================================

	D3D12_CPU_DESCRIPTOR_HANDLE RenderDevice::AllocateSrvDescriptor(
		UINT* outIndex
	) {
		if (m_srvDescriptorCount >= SrvDescriptorCapacity) {
			throw std::runtime_error("SRV descriptor heap capacity exceeded");
		}

		D3D12_CPU_DESCRIPTOR_HANDLE handle =
			m_srvHeap->GetCPUDescriptorHandleForHeapStart();

		handle.ptr += static_cast<SIZE_T>(
			m_srvDescriptorCount
			) * m_srvDescriptorSize;

		if (outIndex) {
			*outIndex = m_srvDescriptorCount;
		}

		m_srvDescriptorCount++;

		return handle;
	}


	D3D12_GPU_DESCRIPTOR_HANDLE RenderDevice::GetSrvGpuHandle(
		UINT index
	) const {
		D3D12_GPU_DESCRIPTOR_HANDLE handle =
			m_srvHeap->GetGPUDescriptorHandleForHeapStart();

		handle.ptr += static_cast<SIZE_T>(
			index
			) * m_srvDescriptorSize;

		return handle;
	}


	// ============================================================
	// Resize
	// ============================================================

	void RenderDevice::Resize(UINT width, UINT height) {
		if (width == m_width && height == m_height) {
			return;
		}

		m_width = width;
		m_height = height;

		// GPU完了後にリソースを解放
		for (UINT i = 0; i < FrameCount; ++i) {
			m_renderTargets[i].Reset();
			m_sceneRenderTargets[i].Reset();
			m_postProcessRenderTargets[i].Reset();
		}

		m_sceneDepth.Reset();

		// --------------------------------------------------------
		// SwapChain
		// --------------------------------------------------------

		ThrowIfFailed(m_swapChain->ResizeBuffers(
			FrameCount,
			width,
			height,
			DXGI_FORMAT_R8G8B8A8_UNORM,
			0
		));

		// --------------------------------------------------------
		// BackBuffer RTV
		// --------------------------------------------------------

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(
			m_rtvHeap->GetCPUDescriptorHandleForHeapStart()
		);

		for (UINT i = 0; i < FrameCount; ++i) {
			ThrowIfFailed(m_swapChain->GetBuffer(
				i,
				IID_PPV_ARGS(&m_renderTargets[i])
			));

			m_device->CreateRenderTargetView(
				m_renderTargets[i].Get(),
				nullptr,
				rtvHandle
			);

			rtvHandle.ptr += m_rtvDescriptorSize;
		}

		CreateSceneResources();
	}


	// ============================================================
	// Scene Resources
	// ============================================================

	void RenderDevice::CreateSceneResources() {
		CreateSceneRenderTargets();
		CreateSceneDepth();
		CreateSceneShaderResourceViews();
	}

	void RenderDevice::CreateSceneRenderTargets() {
		CD3DX12_HEAP_PROPERTIES heapProps(
			D3D12_HEAP_TYPE_DEFAULT
		);

		// ========================================================
		// Scene Color
		// ========================================================

		for (UINT i = 0; i < FrameCount; ++i) {

			CD3DX12_RESOURCE_DESC colorDesc =
				CD3DX12_RESOURCE_DESC::Tex2D(
					DXGI_FORMAT_R8G8B8A8_UNORM,
					m_width,
					m_height,
					1,
					1,
					1,
					0,
					D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
				);

			const FLOAT clearColor[] = {
				0.1f,
				0.1f,
				0.1f,
				1.0f
			};

			CD3DX12_CLEAR_VALUE clearValue(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				clearColor
			);

			ThrowIfFailed(
				m_device->CreateCommittedResource(
					&heapProps,
					D3D12_HEAP_FLAG_NONE,
					&colorDesc,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					&clearValue,
					IID_PPV_ARGS(&m_sceneRenderTargets[i])
				)
			);

			// Scene RTV
			D3D12_CPU_DESCRIPTOR_HANDLE rtv =
				m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

			rtv.ptr += static_cast<SIZE_T>(
				FrameCount + i
				) * m_rtvDescriptorSize;

			m_device->CreateRenderTargetView(
				m_sceneRenderTargets[i].Get(),
				nullptr,
				rtv
			);

			CD3DX12_RESOURCE_DESC postProcessDesc =
				CD3DX12_RESOURCE_DESC::Tex2D(
					DXGI_FORMAT_R8G8B8A8_UNORM,
					m_width,
					m_height,
					1,
					1,
					1,
					0,
					D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
				);
			CD3DX12_CLEAR_VALUE postProcessClearValue(
				DXGI_FORMAT_R8G8B8A8_UNORM,
				clearColor
			);
			ThrowIfFailed(m_device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&postProcessDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&postProcessClearValue,
				IID_PPV_ARGS(&m_postProcessRenderTargets[i])));

			D3D12_CPU_DESCRIPTOR_HANDLE postProcessRtv =
				m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
			postProcessRtv.ptr += static_cast<SIZE_T>(FrameCount * 2 + i) * m_rtvDescriptorSize;
			m_device->CreateRenderTargetView(m_postProcessRenderTargets[i].Get(), nullptr, postProcessRtv);
		}
	}

	void RenderDevice::CreateSceneDepth() {
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		// ========================================================
		// Scene Depth
		// ========================================================

		CD3DX12_RESOURCE_DESC depthDesc =
			CD3DX12_RESOURCE_DESC::Tex2D(
				DXGI_FORMAT_R32_TYPELESS,
				m_width,
				m_height,
				1,
				1,
				1,
				0,
				D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
			);

		CD3DX12_CLEAR_VALUE depthClear(
			DXGI_FORMAT_D32_FLOAT,
			1.0f,
			0
		);

		ThrowIfFailed(
			m_device->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&depthDesc,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				&depthClear,
				IID_PPV_ARGS(&m_sceneDepth)
			)
		);

		D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = DXGI_FORMAT_D32_FLOAT;
		dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		m_device->CreateDepthStencilView(
			m_sceneDepth.Get(),
			&dsv,
			GetDsvHandle()
		);
	}

	void RenderDevice::CreateSceneShaderResourceViews() {
		D3D12_SHADER_RESOURCE_VIEW_DESC colorSrv = {};
		colorSrv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		colorSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		colorSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		colorSrv.Texture2D.MipLevels = 1;

		D3D12_SHADER_RESOURCE_VIEW_DESC depthSrv = {};
		depthSrv.Format = DXGI_FORMAT_R32_FLOAT;
		depthSrv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		depthSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		depthSrv.Texture2D.MipLevels = 1;

		for (UINT i = 0; i < FrameCount; ++i) {
			D3D12_CPU_DESCRIPTOR_HANDLE srv = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
			srv.ptr += static_cast<SIZE_T>(i * 2) * m_srvDescriptorSize;

			m_device->CreateShaderResourceView(m_sceneRenderTargets[i].Get(), &colorSrv, srv);
			srv.ptr += m_srvDescriptorSize;
			m_device->CreateShaderResourceView(m_sceneDepth.Get(), &depthSrv, srv);
		}

		for (UINT i = 0; i < FrameCount; ++i) {
			D3D12_CPU_DESCRIPTOR_HANDLE srv = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
			srv.ptr += static_cast<SIZE_T>(FrameCount * 2 + i) * m_srvDescriptorSize;
			m_device->CreateShaderResourceView(m_postProcessRenderTargets[i].Get(), &colorSrv, srv);
		}

	}

}