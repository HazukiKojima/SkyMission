#include "GpuFrameProfiler.h"

namespace Engine {
	void GpuFrameProfiler::Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
		ThrowIfFailed(queue->GetTimestampFrequency(&m_frequency));

		// フレーム別のGPU計測領域を確保する
		D3D12_QUERY_HEAP_DESC desc{};
		desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
		desc.Count = Marks * RenderDevice::FrameCount;
		ThrowIfFailed(device->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_heap)));
		const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);

		const auto buffer = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint64_t) * desc.Count);
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_readback)));
		ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	}

	void GpuFrameProfiler::Begin(ID3D12GraphicsCommandList* commands, UINT frame) {
		// GPU使用中の計測領域は再利用しない
		m_frame = frame;
		m_capture = m_pending[frame] == 0 || m_fence->GetCompletedValue() >= m_pending[frame];
		if (!m_capture) return;

		// 完了済みのGPU計測結果を読み戻す
		if (m_pending[frame] != 0) {
			const SIZE_T offset = sizeof(uint64_t) * Marks * frame;
			const CD3DX12_RANGE reads(offset, offset + sizeof(uint64_t) * Marks);
			void* data = nullptr;
			ThrowIfFailed(m_readback->Map(0, &reads, &data));

			const auto* ticks = static_cast<const uint64_t*>(data) + Marks * frame;
			std::array<double, 6> ms{};
			for (unsigned i = 0; i < 6; ++i)
				ms[i] = double(ticks[i + 1] - ticks[i]) * 1000.0 / double(m_frequency);
			m_latest = {true, ms[0], ms[1], ms[2], ms[3], ms[4], ms[5]};
			const CD3DX12_RANGE noWrites(0, 0);
			m_readback->Unmap(0, &noWrites);
		}

		// 新しいフレームの計測を始める
		Mark(commands, GpuTimestamp::FrameBegin);
	}

	void GpuFrameProfiler::Submitted(ID3D12CommandQueue* queue) {
		if (m_capture) {
			m_pending[m_frame] = ++m_serial;
			ThrowIfFailed(queue->Signal(m_fence.Get(), m_serial));
		}
	}
} // namespace Engine
