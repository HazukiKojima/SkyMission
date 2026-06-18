#pragma once

#ifndef UNICODE
#define UNICODE
#endif

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <stdexcept>
#include <memory>
#include "../d3dx12.h"

using Microsoft::WRL::ComPtr;

// 拡張APIエラーハンドリング用マクロ
inline void ThrowIfFailed(HRESULT hr) {
	if (FAILED(hr)) {
		throw std::runtime_error("DirectX12APIの実行に失敗しました。");
	}
}