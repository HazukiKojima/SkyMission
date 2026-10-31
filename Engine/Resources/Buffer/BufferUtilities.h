#pragma once
#include "../../EngineCommon.h"
#include <cstring>

namespace Engine {
	inline ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, UINT64 size, UINT8** mappedData) {
		ComPtr<ID3D12Resource> resource;
		const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_UPLOAD);
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource)));
		const CD3DX12_RANGE noCpuReads(0, 0);
		ThrowIfFailed(resource->Map(0, &noCpuReads, reinterpret_cast<void**>(mappedData)));
		return resource;
	}

	inline ComPtr<ID3D12Resource> CreateInitializedUploadBuffer(
		ID3D12Device* device, const void* data, UINT64 size) {
		UINT8* mapped = nullptr;
		auto resource = CreateUploadBuffer(device, size, &mapped);
		std::memcpy(mapped, data, static_cast<size_t>(size));
		resource->Unmap(0, nullptr);
		return resource;
	}

	// このコピーのGPU完了まで、転送元のuploadリソースを保持する
	inline ComPtr<ID3D12Resource> CreateStaticBuffer(ID3D12Device* device,
		ID3D12GraphicsCommandList* commands, const void* data, UINT64 size, D3D12_RESOURCE_STATES finalState,
		ComPtr<ID3D12Resource>& upload) {
		upload = CreateInitializedUploadBuffer(device, data, size);
		ComPtr<ID3D12Resource> resource;
		const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(size);
		ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
			D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource)));
		commands->CopyBufferRegion(resource.Get(), 0, upload.Get(), 0, size);
		const auto barrier =
			CD3DX12_RESOURCE_BARRIER::Transition(resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, finalState);
		commands->ResourceBarrier(1, &barrier);
		return resource;
	}
} // namespace Engine
