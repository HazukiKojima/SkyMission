#include "CommandContext.h"
#include "RenderDevice.h"

namespace Engine {
	CommandContext::~CommandContext() {
		if (m_fenceEvent) {
			CloseHandle(m_fenceEvent);
		}
	}

	// コマンドリストおよびダブルバッファリング用同期オブジェクトの初期化
	void CommandContext::Initialize(RenderDevice* renderDevice) {
		auto device = renderDevice->GetDevice();
		m_targetQueue = renderDevice->GetCommandQueue();
		m_currentFrameIndex = renderDevice->GetFrameIndex();

		// マルチバッファリング対応のため各フレーム専用のアロケータを生成
		for (UINT i = 0; i < RenderDevice::FrameCount; i++) {
			ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
		}

		// コマンドリスト生成直後のOpen状態を即Closeして初期状態を揃える
		ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(), nullptr, IID_PPV_ARGS(&m_commandList)));
		ThrowIfFailed(m_commandList->Close());

		ThrowIfFailed(device->CreateFence(m_fenceValues[m_currentFrameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValues[m_currentFrameIndex]++;

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_fenceEvent) {
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	// 新しいフレームのコマンド記録を開始
	void CommandContext::BeginFrame() {
		// GPU側の処理完了を前提に、該当フレーム用のアロケータとリストをリサイクル
		ThrowIfFailed(m_commandAllocators[m_currentFrameIndex]->Reset());
		ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_currentFrameIndex].Get(), nullptr));
	}

	// コマンド記録を終了し、コマンドキューへ実行をキック
	void CommandContext::EndFrame() {
		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
		m_targetQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	// リソースのパイプラインステート遷移に伴うリソースバリアの設定
	void CommandContext::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter) {
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = resource;
		barrier.Transition.StateBefore = stateBefore;
		barrier.Transition.StateAfter = stateAfter;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		m_commandList->ResourceBarrier(1, &barrier);
	}

	// 次のフレームに進むためのシグナル送信およびGPUの進行待機同期
	void CommandContext::MoveToNextFrame(RenderDevice* renderDevice) {
		const UINT64 currentFenceValue = m_fenceValues[m_currentFrameIndex];
		ThrowIfFailed(m_targetQueue->Signal(m_fence.Get(), currentFenceValue));

		m_currentFrameIndex = renderDevice->GetFrameIndex();

		// 次に進むフレームのバッファがまだGPUで使用中の場合、完了通知をイベント待機
		if (m_fence->GetCompletedValue() < m_fenceValues[m_currentFrameIndex]) {
			ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrameIndex], m_fenceEvent));
			WaitForSingleObject(m_fenceEvent, INFINITE);
		}

		m_fenceValues[m_currentFrameIndex] = currentFenceValue + 1;
	}

	// キック済みの全コマンドが完了するまでCPUを強制ブロック（解放同期用）
	void CommandContext::WaitForGpu() {
		if (!m_targetQueue || !m_fence) return;

		// 現在キックしている全コマンドの完了をその場で待機（リソース破棄時の安全同期用）
		ThrowIfFailed(m_targetQueue->Signal(m_fence.Get(), m_fenceValues[m_currentFrameIndex]));
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrameIndex], m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);

		m_fenceValues[m_currentFrameIndex]++;
	}
}