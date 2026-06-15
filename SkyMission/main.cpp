#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

// ウィンドウのサイズ
const UINT WindowWidth = 800;
const UINT WindowHeight = 600;
const UINT FrameCount = 2; // ダブルバッファリング

// DX12のグローバルオブジェクト
ComPtr<ID3D12Device>        g_device;
ComPtr<ID3D12CommandQueue>   g_commandQueue;
ComPtr<IDXGISwapChain3>     g_swapChain;
ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
ComPtr<ID3D12CommandAllocator> g_commandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_commandList;
ComPtr<ID3D12Resource>      g_renderTargets[FrameCount];
UINT                        g_rtvDescriptorSize = 0;

// 同期オブジェクト
ComPtr<ID3D12Fence>         g_fence;
HANDLE                      g_fenceEvent;
UINT64                      g_fenceValue = 0;
UINT                        g_frameIndex = 0;

// 関数宣言
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitDirectX12(HWND hwnd);
void Render();
void WaitForPreviousFrame();
void CleanupDirectX12();

// エントリーポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	// ウィンドウクラスの登録
	WNDCLASS wc = {};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"DX12WindowClass";

	RegisterClass(&wc);

	// ウィンドウサイズをクライアント領域に合わせる調整
	RECT rc = { 0, 0, static_cast<LONG>(WindowWidth), static_cast<LONG>(WindowHeight) };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	// ウィンドウの作成
	HWND hwnd = CreateWindowEx(
		0, L"DX12WindowClass", L"DirectX 12 Window (VS2026)",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		nullptr, nullptr, hInstance, nullptr
	);

	if (hwnd == nullptr) return 0;

	ShowWindow(hwnd, nCmdShow);

	// DirectX 12の初期化
	InitDirectX12(hwnd);

	// メッセージループ
	MSG msg = {};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			Render();
		}
	}

	// 後片付け
	WaitForPreviousFrame();
	CleanupDirectX12();

	return 0;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// DirectX 12 初期化
void InitDirectX12(HWND hwnd) {
	// デバッグレイヤーの有効化
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
	}
#endif

	// ファクトリの作成
	ComPtr<IDXGIFactory4> factory;
	CreateDXGIFactory1(IID_PPV_ARGS(&factory));

	// デバイスの作成
	D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device));

	// コマンドキューの作成
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue));

	// スワップチェーンの作成
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = FrameCount;
	swapChainDesc.Width = WindowWidth;
	swapChainDesc.Height = WindowHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	factory->CreateSwapChainForHwnd(g_commandQueue.Get(), hwnd, &swapChainDesc, nullptr, nullptr, &swapChain);
	swapChain.As(&g_swapChain);
	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

	// RTV用デスクリプタヒープの作成
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = FrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_rtvHeap));
	g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// レンダーターゲットの登録
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < FrameCount; i++) {
		g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i]));
		g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += g_rtvDescriptorSize;
	}

	// コマンドアロケータとリストの作成
	g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator));
	g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList));
	g_commandList->Close(); // 最初は閉じておく

	// フェンスの作成
	g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence));
	g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

// 描画処理
void Render() {
	// コマンドの記録開始
	g_commandAllocator->Reset();
	g_commandList->Reset(g_commandAllocator.Get(), nullptr);

	// リソースバリア
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = g_renderTargets[g_frameIndex].Get();
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	g_commandList->ResourceBarrier(1, &barrier);

	// レンダーターゲットのハンドルを取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	rtvHandle.ptr += g_frameIndex * g_rtvDescriptorSize;

	// 背景のクリア
	const float clearColor[] = { 0.1f, 0.2f, 0.4f, 1.0f };
	g_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// リソースバリア
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	g_commandList->ResourceBarrier(1, &barrier);

	// コマンドの記録終了と実行
	g_commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// 画面のフリップ
	g_swapChain->Present(1, 0);

	// GPUの処理完了を待つ
	WaitForPreviousFrame();
}

// GPUの同期）
void WaitForPreviousFrame() {
	const UINT64 fence = g_fenceValue;
	g_commandQueue->Signal(g_fence.Get(), fence);
	g_fenceValue++;

	if (g_fence->GetCompletedValue() < fence) {
		g_fence->SetEventOnCompletion(fence, g_fenceEvent);
		WaitForSingleObject(g_fenceEvent, INFINITE);
	}

	g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();
}

// 解放処理
void CleanupDirectX12() {
	if (g_fenceEvent) {
		CloseHandle(g_fenceEvent);
	}
}