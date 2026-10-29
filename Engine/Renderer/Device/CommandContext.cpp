#include "CommandContext.h"
#include "RenderDevice.h"

namespace Engine {
	CommandContext::~CommandContext() {
		if (m_fenceEvent) {
			CloseHandle(m_fenceEvent);
		}
	}

	void CommandContext::Initialize(RenderDevice* renderDevice) {
		m_renderDevice = renderDevice;
		auto device = renderDevice->GetDevice();
		m_targetQueue = renderDevice->GetCommandQueue();
		m_currentFrameIndex = renderDevice->GetFrameIndex();

		// GPU使用中の領域を上書きしないよう、フレーム別にアロケータを持つ
		for (UINT i = 0; i < RenderDevice::FrameCount; i++) {
			ThrowIfFailed(
				device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i])));
		}

		// 生成直後のリストを閉じ、BeginFrameから記録できる状態にする
		ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[0].Get(),
												nullptr, IID_PPV_ARGS(&m_commandList)));
		ThrowIfFailed(m_commandList->Close());

		ThrowIfFailed(
			device->CreateFence(m_fenceValues[m_currentFrameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
		m_fenceValues[m_currentFrameIndex]++;

		m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		if (!m_fenceEvent) {
			ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
		}
	}

	void CommandContext::BeginFrame() {
		// GPU側の処理完了を前提に、該当フレーム用のアロケータとリストをリサイクル
		ThrowIfFailed(m_commandAllocators[m_currentFrameIndex]->Reset());
		ThrowIfFailed(m_commandList->Reset(m_commandAllocators[m_currentFrameIndex].Get(), nullptr));
	}

	void CommandContext::EndFrame() {
		ThrowIfFailed(m_commandList->Close());
		ID3D12CommandList* ppCommandLists[] = {m_commandList.Get()};
		m_targetQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
	}

	// 用途切り替え用の状態遷移バリアを全サブリソースに記録する
	void CommandContext::TransitionResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES stateBefore,
											D3D12_RESOURCE_STATES stateAfter) {
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

	// リソースを安全に解放するため、投入済みコマンドのGPU完了を待つ
	void CommandContext::WaitForGpu() {
		if (!m_targetQueue || !m_fence)
			return;

		ThrowIfFailed(m_targetQueue->Signal(m_fence.Get(), m_fenceValues[m_currentFrameIndex]));
		ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrameIndex], m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);

		m_fenceValues[m_currentFrameIndex]++;
	}
} // namespace Engine