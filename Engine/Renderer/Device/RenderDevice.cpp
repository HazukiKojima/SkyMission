#include "RenderDevice.h"

namespace Engine {
	// DX12描画に必要なデバイス、スワップチェーン、RTVヒープを初期化
	void RenderDevice::Initialize(HWND hwnd, UINT width, UINT height) {
#if defined(_DEBUG)
		// デバイス生成より先に有効化しないと警告をキャッチできないため最優先で実行
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
		}
#endif

		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)));
		ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
		swapChainDesc.BufferCount = FrameCount;
		swapChainDesc.Width = width;
		swapChainDesc.Height = height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // Win10以降の標準フリップモデル
		swapChainDesc.SampleDesc.Count = 1;                       // スワップチェーンのバックバッファはMSAA非対応のため1サンプルで生成

		ComPtr<IDXGISwapChain1> swapChain;
		ThrowIfFailed(m_factory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));
		ThrowIfFailed(swapChain.As(&m_swapChain));

		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = FrameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		// RTVのサイズはGPUごとに異なるためAPI経由で動的に取得
		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < FrameCount; i++) {
			ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += m_rtvDescriptorSize; // 次のRTVディスクリプタへ移動
		}

		// CBV/SRV/UAV ヒープを作成（テクスチャなどをシェーダから参照するためのシェーダ可視ヒープ）
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 256; // 十分な数を確保
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

		// SRV ヒープのディスクリプタサイズを取得しておく
		m_srvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		m_width = width;
		m_height = height;
	}

	void RenderDevice::Present() {
		ThrowIfFailed(m_swapChain->Present(1, 0)); // ティアリング防止のためVSync有効
	}

	D3D12_CPU_DESCRIPTOR_HANDLE RenderDevice::GetCurrentRtvHandle() const {
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		rtvHandle.ptr += GetFrameIndex() * m_rtvDescriptorSize;
		return rtvHandle;
	}


	D3D12_CPU_DESCRIPTOR_HANDLE RenderDevice::AllocateSrvDescriptor(UINT* outIndex) {
		D3D12_CPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(m_srvDescriptorCount) * m_srvDescriptorSize;
		if (outIndex) *outIndex = m_srvDescriptorCount;
		m_srvDescriptorCount++;
		return handle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE RenderDevice::GetSrvGpuHandle(UINT index) const {
		D3D12_GPU_DESCRIPTOR_HANDLE handle = m_srvHeap->GetGPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>(index) * m_srvDescriptorSize;
		return handle;
	}

	void RenderDevice::Resize(UINT width, UINT height) {
		// ウィンドウサイズが変わらない場合は何もしない
		if (width == m_width && height == m_height) return;

		m_width = width;
		m_height = height;

		// GPU の使用を待ってからリソースを解放
		for (UINT i = 0; i < FrameCount; ++i) {
			m_renderTargets[i].Reset();
		}

		// スワップチェインのバッファサイズを更新
		ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));

		// RTV を再作成
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < FrameCount; ++i) {
			ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
			m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
			rtvHandle.ptr += m_rtvDescriptorSize;
		}
	}
}