#pragma once
#include "../../EngineCommon.h"
#include <cstring>

namespace Engine {
	ComPtr<ID3D12Resource> CreateUploadBuffer(ID3D12Device* device, UINT64 size, UINT8** mappedData);

	ComPtr<ID3D12Resource> CreateInitializedUploadBuffer(
		ID3D12Device* device, const void* data, UINT64 size);

	// このコピーのGPU完了まで、転送元のuploadリソースを保持する
	ComPtr<ID3D12Resource> CreateStaticBuffer(ID3D12Device* device,
		ID3D12GraphicsCommandList* commands, const void* data, UINT64 size, D3D12_RESOURCE_STATES finalState,
		ComPtr<ID3D12Resource>& upload);
} // namespace Engine
