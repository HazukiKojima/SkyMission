#pragma once
#include "RenderDevice.h"
#include <array>

namespace Engine {
	struct GpuFrameTimings {
		bool valid = false;
		double skyMs = 0;
		double oceanMs = 0;
		double modelsMs = 0;
		double postProcessMs = 0;
		double hudMs = 0;
		double TotalMs() const {
			return skyMs + oceanMs + modelsMs + postProcessMs + hudMs;
		}
	};
	enum class GpuTimestamp : unsigned {
		FrameBegin,
		SkyEnd,
		OceanEnd,
		ModelsEnd,
		PostProcessEnd,
		HudEnd,
		Count
	};

	class GpuFrameProfiler {
	  public:
		void Initialize(ID3D12Device* device, ID3D12CommandQueue* queue) {
			ThrowIfFailed(queue->GetTimestampFrequency(&m_frequency));
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
		void Begin(ID3D12GraphicsCommandList* commands, UINT frame) {
			m_frame = frame;
			m_capture = m_pending[frame] == 0 || m_fence->GetCompletedValue() >= m_pending[frame];
			if (!m_capture)
				return;
			if (m_pending[frame] != 0) {
				const SIZE_T offset = sizeof(uint64_t) * Marks * frame;
				const CD3DX12_RANGE reads(offset, offset + sizeof(uint64_t) * Marks);
				void* data = nullptr;
				ThrowIfFailed(m_readback->Map(0, &reads, &data));
				const auto* ticks = static_cast<const uint64_t*>(data) + Marks * frame;
				std::array<double, 5> ms{};
				for (unsigned i = 0; i < 5; ++i)
					ms[i] = double(ticks[i + 1] - ticks[i]) * 1000.0 / double(m_frequency);
				m_latest = {true, ms[0], ms[1], ms[2], ms[3], ms[4]};
				const CD3DX12_RANGE noWrites(0, 0);
				m_readback->Unmap(0, &noWrites);
			}
			Mark(commands, GpuTimestamp::FrameBegin);
		}
		void Mark(ID3D12GraphicsCommandList* commands, GpuTimestamp mark) {
			if (m_capture)
				commands->EndQuery(
					m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_frame * Marks + static_cast<unsigned>(mark));
		}
		void Resolve(ID3D12GraphicsCommandList* commands) {
			if (m_capture)
				commands->ResolveQueryData(m_heap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, m_frame * Marks, Marks,
					m_readback.Get(), sizeof(uint64_t) * m_frame * Marks);
		}
		void Submitted(ID3D12CommandQueue* queue) {
			if (m_capture) {
				m_pending[m_frame] = ++m_serial;
				ThrowIfFailed(queue->Signal(m_fence.Get(), m_serial));
			}
		}
		const GpuFrameTimings& Latest() const {
			return m_latest;
		}

	  private:
		static constexpr UINT Marks = static_cast<UINT>(GpuTimestamp::Count);
		ComPtr<ID3D12QueryHeap> m_heap;
		ComPtr<ID3D12Resource> m_readback;
		ComPtr<ID3D12Fence> m_fence;
		std::array<uint64_t, RenderDevice::FrameCount> m_pending{};
		uint64_t m_serial = 0;
		uint64_t m_frequency = 0;
		UINT m_frame = 0;
		bool m_capture = false;
		GpuFrameTimings m_latest;
	};
} // namespace Engine
