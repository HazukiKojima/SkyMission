#include "RendererConstantBuffers.h"
#include "CommandContext.h"

namespace Engine {
	namespace {
		UINT64 AlignConstantBufferSize(size_t size) {
			return (static_cast<UINT64>(size) + 255) & ~static_cast<UINT64>(255);
		}

		Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, UINT64 size, UINT8** mappedData) {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
			const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&resource)));

			const CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(resource->Map(0, &readRange, reinterpret_cast<void**>(mappedData)));
			return resource;
		}
	}

	static_assert(sizeof(OceanFogConstantBufferData) == 160);
	static_assert(sizeof(FilmConstantBufferData) == 224);

	void RendererConstantBuffers::Initialize(ID3D12Device* device) {
		UINT8* oceanMappedData = nullptr;
		m_oceanResource = CreateUploadBuffer(device, AlignConstantBufferSize(sizeof(OceanConstantBufferData)), &oceanMappedData);
		m_oceanData = reinterpret_cast<OceanConstantBufferData*>(oceanMappedData);

		UINT8* oceanFogMappedData = nullptr;
		m_oceanFogResource = CreateUploadBuffer(device, AlignConstantBufferSize(sizeof(OceanFogConstantBufferData)), &oceanFogMappedData);
		m_oceanFogData = reinterpret_cast<OceanFogConstantBufferData*>(oceanFogMappedData);

		const UINT64 filmBufferSize = AlignConstantBufferSize(sizeof(FilmConstantBufferData));
		for (auto& filmBuffer : m_filmBuffers) {
			filmBuffer.resource = CreateUploadBuffer(device, filmBufferSize, &filmBuffer.mappedData);
		}
	}

	void RendererConstantBuffers::EnsureModelCapacity(ID3D12Device* device, CommandContext* context, size_t objectCount) {
		if (objectCount <= m_modelBuffers[0].size() && objectCount <= m_modelBuffers[1].size()) {
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
}
