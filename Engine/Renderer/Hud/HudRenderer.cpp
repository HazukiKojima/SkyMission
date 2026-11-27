#include "HudRenderer.h"
#include "HudFont.h"
#include "Engine/ExecutablePath.h"
#include "Engine/Resources/Buffer/BufferUtilities.h"
#include <d3dcompiler.h>
#include <climits>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <limits>

namespace Engine {
	namespace {
		constexpr int kFontCharacterHeightPixels = -36;
		constexpr int kAsciiFontAverageWidthPixels = 20;
		constexpr int kGlyphLeftInsetPixels = 2;
		constexpr int kGlyphTopInsetPixels = 3;
		constexpr size_t kAtlasBytesPerPixel = 4;
		constexpr size_t kAtlasAlphaChannel = 3;
		constexpr auto kOpaqueChannel = std::numeric_limits<uint8_t>::max();

		std::vector<uint8_t> CreateFontAtlas() {
			// 文字アトラスの描画環境を用意する
			const HDC dc = CreateCompatibleDC(nullptr);
			if (!dc) throw std::runtime_error("Could not create HUD font DC");

			BITMAPINFO info{};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = HudFont::Width;
			info.bmiHeader.biHeight = -LONG(HudFont::Height);
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = kAtlasBytesPerPixel * CHAR_BIT;
			void* pixels = nullptr;

			const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
			const auto font = CreateFontW(kFontCharacterHeightPixels, kAsciiFontAverageWidthPixels, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
			const auto japaneseFont = CreateFontW(kFontCharacterHeightPixels, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
				SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH,
				L"UD デジタル 教科書体 N");
			if (!bitmap || !font || !japaneseFont) {
				if (font) DeleteObject(font);
				if (bitmap) DeleteObject(bitmap);
				if (japaneseFont) DeleteObject(japaneseFont);
				DeleteDC(dc);
				throw std::runtime_error("Could not create HUD font bitmap");
			}

			const auto oldBitmap = SelectObject(dc, bitmap);
			const auto oldFont = SelectObject(dc, font);
			std::memset(pixels, 0, HudFont::Width * HudFont::Height * kAtlasBytesPerPixel);
			SetTextColor(dc, RGB(kOpaqueChannel, kOpaqueChannel, kOpaqueChannel));
			SetBkColor(dc, RGB(0, 0, 0));
			SetBkMode(dc, TRANSPARENT);

			// 各文字を対応するセルへ描く
			for (unsigned glyph = 0; glyph < HudFont::GlyphCount; ++glyph) {
				const bool ascii = glyph < HudFont::AsciiCount;
				const wchar_t text =
					wchar_t(ascii ? glyph + HudFont::FirstAsciiGlyph : HudFont::Japanese[glyph - HudFont::AsciiCount]);
				SelectObject(dc, ascii || text == L'▶' ? font : japaneseFont);

				const int x = int(glyph % HudFont::Columns * HudFont::Cell);
				const int y = int(glyph / HudFont::Columns * HudFont::Cell);
				const RECT cell{x, y, x + LONG(HudFont::GlyphWidth(glyph)), y + LONG(HudFont::Cell)};
				ExtTextOutW(dc, x + kGlyphLeftInsetPixels, y + kGlyphTopInsetPixels, ETO_CLIPPED, &cell, &text, 1, nullptr);
			}

			// 文字の明るさをアルファへ変換する
			GdiFlush();
			std::vector<uint8_t> rgba(HudFont::Width * HudFont::Height * kAtlasBytesPerPixel);
			const auto* source = static_cast<const uint8_t*>(pixels);
			for (size_t i = 0; i < rgba.size(); i += kAtlasBytesPerPixel) {
				rgba[i] = rgba[i + 1] = rgba[i + 2] = kOpaqueChannel;
				rgba[i + kAtlasAlphaChannel] = source[i];
			}

			// 図形の塗りつぶし用の領域を確保する
			for (unsigned y = HudFont::Height - HudFont::SolidRegionSize; y < HudFont::Height; ++y)
				for (unsigned x = HudFont::Width - HudFont::SolidRegionSize; x < HudFont::Width; ++x)
					rgba[(y * HudFont::Width + x) * kAtlasBytesPerPixel + kAtlasAlphaChannel] = kOpaqueChannel;

			// 使用したGDIリソースを解放する
			SelectObject(dc, oldFont);
			SelectObject(dc, oldBitmap);
			DeleteObject(font);
			DeleteObject(japaneseFont);
			DeleteObject(bitmap);
			DeleteDC(dc);

			return rgba;
		}
	} // namespace

	void HudRenderer::Initialize(RenderDevice& device, ID3D12GraphicsCommandList* commands) {
		// 文字アトラスをGPUへ転送する
		auto* gpu = device.GetDevice();
		const auto pixels = CreateFontAtlas();
		m_atlas.CreateFromRgba(gpu, commands, HudFont::Width, HudFont::Height, pixels);
		m_atlas.CreateShaderResourceView(gpu, device.AllocateSrvDescriptor(&m_srv));

		// HUD用の描画パイプラインを作る
		const CD3DX12_DESCRIPTOR_RANGE ranges[] = {
			{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0}, {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1}};

		CD3DX12_ROOT_PARAMETER parameters[std::size(ranges)];
		for (size_t i = 0; i < std::size(ranges); ++i)
			parameters[i].InitAsDescriptorTable(1, &ranges[i], D3D12_SHADER_VISIBILITY_PIXEL);
		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		const CD3DX12_ROOT_SIGNATURE_DESC rootDesc(
			static_cast<UINT>(std::size(parameters)), parameters, 1, &sampler,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> rootBlob, error, vs, ps;
		ThrowIfFailed(
			D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &error));
		ThrowIfFailed(gpu->CreateRootSignature(
			0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&m_root)));
		ThrowIfFailed(D3DReadFileToBlob((ShaderDirectory() / L"HudVS.cso").c_str(), &vs));
		ThrowIfFailed(D3DReadFileToBlob((ShaderDirectory() / L"HudPS.cso").c_str(), &ps));
		const D3D12_INPUT_ELEMENT_DESC layout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0},
			{"TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT, 0, static_cast<UINT>(offsetof(HudVertex, sceneDepth)),
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = m_root.Get();
		desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
		desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
		desc.InputLayout = {layout, static_cast<UINT>(std::size(layout))};
		desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

		auto& blend = desc.BlendState.RenderTarget[0];
		blend.BlendEnable = TRUE;
		blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		desc.DepthStencilState.DepthEnable = FALSE;
		desc.SampleMask = UINT_MAX;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		ThrowIfFailed(gpu->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_pipeline)));

		// フレーム別に頂点の転送領域を確保する
		for (UINT frame = 0; frame < RenderDevice::FrameCount; ++frame)
			m_vertices[frame] =
				CreateUploadBuffer(gpu, HudDrawList::MaxVertices * sizeof(HudVertex), &m_mapped[frame]);
	}

	void HudRenderer::Record(
		RenderDevice& device, ID3D12GraphicsCommandList* commands, const HudDrawList& list) {
		const auto& vertices = list.GetVertices();
		if (vertices.empty()) return;
		if (vertices.size() > HudDrawList::MaxVertices)
			throw std::length_error("HUD vertex capacity exceeded");
		const auto frame = device.GetFrameIndex();

		// このフレーム用の領域へHUD頂点を書き込む
		std::memcpy(m_mapped[frame], vertices.data(), vertices.size() * sizeof(HudVertex));
		commands->SetGraphicsRootSignature(m_root.Get());
		commands->SetPipelineState(m_pipeline.Get());
		ID3D12DescriptorHeap* heaps[] = {device.GetSrvDescriptorHeap()};
		commands->SetDescriptorHeaps(1, heaps);
		commands->SetGraphicsRootDescriptorTable(0, device.GetSrvGpuHandle(m_srv));
		commands->SetGraphicsRootDescriptorTable(1, device.GetSrvGpuHandle(device.GetSceneDepthSrvIndex()));

		const D3D12_VERTEX_BUFFER_VIEW view{m_vertices[frame]->GetGPUVirtualAddress(),
			UINT(vertices.size() * sizeof(HudVertex)), sizeof(HudVertex)};
		commands->IASetVertexBuffers(0, 1, &view);
		commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commands->DrawInstanced(UINT(vertices.size()), 1, 0, 0);
	}
} // namespace Engine
