#pragma once
#include "RenderDevice.h"
#include <array>

namespace Engine {
	struct GpuFrameTimings {
		bool valid = false;
		double skyMs = 0;
		double oceanMs = 0;
		double modelsMs = 0;
		double cloudsMs = 0;
		double postProcessMs = 0;
		double hudMs = 0;

		double TotalMs() const {
			return skyMs + oceanMs + modelsMs + cloudsMs + postProcessMs + hudMs;
		}
	};

	enum class GpuTimestamp : unsigned {
		FrameBegin,
		SkyEnd,
		OceanEnd,
		ModelsEnd,
		CloudsEnd,
		PostProcessEnd,
		HudEnd,
		Count
	};

	class GpuFrameProfiler {
	  public:
		void Initialize(ID3D12Device* device, ID3D12CommandQueue* queue);

		void Begin(ID3D12GraphicsCommandList* commands, UINT frame);

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

		void Submitted(ID3D12CommandQueue* queue);

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
