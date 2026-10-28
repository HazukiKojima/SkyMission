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
#include <iomanip>
#include <sstream>
#include "../d3dx12.h"

using Microsoft::WRL::ComPtr;

// 拡張APIエラーハンドリング用マクロ
inline void ThrowIfFailed(HRESULT hr) {
	if (FAILED(hr)) {
		std::ostringstream message;
		message << "DirectX 12 API call failed. HRESULT=0x"
			<< std::uppercase << std::hex << static_cast<unsigned long>(hr);
		throw std::runtime_error(message.str());
	}
}