#include "RendererConstantBuffers.h"
#include "CommandContext.h"
#include "../../Resources/Buffer/BufferUtilities.h"
#include <algorithm>

namespace Engine {
	namespace {
		UINT64 AlignConstantBufferSize(size_t size) {
			return (static_cast<UINT64>(size) + 255) & ~static_cast<UINT64>(255);
		}

	} // namespace

	static_assert(sizeof(OceanFogConstantBufferData) == 160);
	static_assert(sizeof(FilmConstantBufferData) == 224);

	void RendererConstantBuffers::Initialize(ID3D12Device* device) {
		for (auto& buffer : m_oceanBuffers)
			buffer.resource = CreateUploadBuffer(device, AlignConstantBufferSize(sizeof(OceanConstantBufferData)),
												 &buffer.mappedData);
		for (auto& buffer : m_oceanFogBuffers)
			buffer.resource = CreateUploadBuffer(device, AlignConstantBufferSize(sizeof(OceanFogConstantBufferData)),
												 &buffer.mappedData);

		const UINT64 filmBufferSize = AlignConstantBufferSize(sizeof(FilmConstantBufferData));
		for (auto& filmBuffer : m_filmBuffers) {
			filmBuffer.resource = CreateUploadBuffer(device, filmBufferSize, &filmBuffer.mappedData);
		}
	}

	void RendererConstantBuffers::EnsureModelCapacity(ID3D12Device* device, CommandContext* context,
													  size_t objectCount) {
		if (std::all_of(std::begin(m_modelBuffers), std::end(m_modelBuffers),
						[objectCount](const auto& buffers) { return buffers.size() >= objectCount; })) {
			return;
		}

		context->WaitForGpu();
		const UINT64 bufferSize = AlignConstantBufferSize(sizeof(ModelConstantBufferData));
		for (UINT frameIndex = 0; frameIndex < RenderDevice::FrameCount; ++frameIndex) {
			auto& buffers = m_modelBuffers[frameIndex];
			while (buffers.size() < objectCount) {
				MappedBuffer buffer;
				buffer.resource = CreateUploadBuffer(device, bufferSize, &buffer.mappedData);
				buffers.push_back(std::move(buffer));
			}
		}
	}
} // namespace Engine
