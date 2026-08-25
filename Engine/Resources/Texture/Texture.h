#pragma once
#include "../../EngineCommon.h"
#include <DirectXTex.h>
#include "../../../d3dx12.h"

namespace Engine {
	// Texture: DirectXTex ‚ğg—p‚µ‚Äƒtƒ@ƒCƒ‹‚©‚ç“Ç‚İ‚İAD3D12 ƒŠƒ\[ƒX‚Æ SRV ‚ğì¬‚·‚é
	class Texture {
	public:
		Texture() = default;
		~Texture() = default;

<<<<<<< Updated upstream
		// ƒtƒ@ƒCƒ‹‚©‚ç“Ç‚İ‚İBdevice ‚ÆƒRƒ}ƒ“ƒhƒŠƒXƒg‚ÍƒAƒbƒvƒ[ƒh‚Ég—p‚·‚éB
=======
<<<<<<< Updated upstream
		// ãƒ•ã‚¡ã‚¤ãƒ«ã‹ã‚‰èª­ã¿è¾¼ã¿ã€‚device ã¨ã‚³ãƒãƒ³ãƒ‰ãƒªã‚¹ãƒˆã¯ã‚¢ãƒƒãƒ—ãƒ­ãƒ¼ãƒ‰ã«ä½¿ç”¨ã™ã‚‹ã€‚
>>>>>>> Stashed changes
		bool LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath);
=======
		// ƒtƒ@ƒCƒ‹‚©‚ç“Ç‚İ‚İBdevice ‚ÆƒRƒ}ƒ“ƒhƒŠƒXƒg‚ÍƒAƒbƒvƒ[ƒh‚Ég—p‚·‚éB
		bool LoadFromFile(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const std::wstring& filePath, bool useSrgb = true);
>>>>>>> Stashed changes

		// SRV ‚ğì¬‚·‚é‚½‚ß‚Ìƒwƒ‹ƒp[BRenderDevice ‘¤‚ÅŠ„“–‚Ä‚½ CPU ƒnƒ“ƒhƒ‹‚ğ“n‚·B
		void CreateShaderResourceView(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE srvHandle) const;

		ID3D12Resource* GetResource() const { return m_texture.Get(); }
		bool IsLoaded() const { return m_texture != nullptr; }

	private:
		ComPtr<ID3D12Resource> m_texture;        // GPU ã‚ÌƒeƒNƒXƒ`ƒƒƒŠƒ\[ƒX
		ComPtr<ID3D12Resource> m_uploadHeap;     // ƒAƒbƒvƒ[ƒh—pƒq[ƒviƒ‰ƒCƒtƒ^ƒCƒ€‚ğ•Û‚·‚é‚½‚ß•Û‘¶j
		DirectX::TexMetadata m_meta;
		DirectX::ScratchImage m_image;
	};
}
