#include "HudRenderer.h"
#include "Engine/ExecutablePath.h"
#include "Engine/Resources/Buffer/BufferUtilities.h"
#include <d3dcompiler.h>
#include <cstring>

namespace Engine {
	namespace {
		std::vector<uint8_t> CreateFontAtlas() {
			const HDC dc = CreateCompatibleDC(nullptr);
			if (!dc)
				throw std::runtime_error("Could not create HUD font DC");
			BITMAPINFO info{};
			info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			info.bmiHeader.biWidth = 512;
			info.bmiHeader.biHeight = -384;
			info.bmiHeader.biPlanes = 1;
			info.bmiHeader.biBitCount = 32;
			void* pixels = nullptr;
			const auto bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
			const auto font = CreateFontW(-36, 20, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, ANSI_CHARSET,
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
			if (!bitmap || !font) {
				if (font)
					DeleteObject(font);
				if (bitmap)
					DeleteObject(bitmap);
				DeleteDC(dc);
				throw std::runtime_error("Could not create HUD font bitmap");
			}
			const auto oldBitmap = SelectObject(dc, bitmap);
			const auto oldFont = SelectObject(dc, font);
			std::memset(pixels, 0, 512 * 384 * 4);
			SetTextColor(dc, RGB(255, 255, 255));
			SetBkColor(dc, RGB(0, 0, 0));
			SetBkMode(dc, TRANSPARENT);
			for (int c = 32; c <= 126; ++c) {
				const wchar_t text = wchar_t(c);
				TextOutW(dc, (c - 32) % 16 * 32 + 2, (c - 32) / 16 * 48 + 3, &text, 1);
			}
			GdiFlush();
			std::vector<uint8_t> rgba(512 * 384 * 4);
			const auto* source = static_cast<const uint8_t*>(pixels);
			for (size_t i = 0; i < rgba.size(); i += 4) {
				rgba[i] = rgba[i + 1] = rgba[i + 2] = 255;
				rgba[i + 3] = source[i];
			}
			for (int y = 376; y < 384; ++y)
				for (int x = 504; x < 512; ++x)
					rgba[(y * 512 + x) * 4 + 3] = 255;
			SelectObject(dc, oldFont);
			SelectObject(dc, oldBitmap);
			DeleteObject(font);
			DeleteObject(bitmap);
			DeleteDC(dc);
			return rgba;
		}
	} // namespace
	void HudRenderer::Initialize(RenderDevice& device, ID3D12GraphicsCommandList* commands) {
		auto* gpu = device.GetDevice();
		const auto pixels = CreateFontAtlas();
		m_atlas.CreateFromRgba(gpu, commands, 512, 384, pixels);
		m_atlas.CreateShaderResourceView(gpu, device.AllocateSrvDescriptor(&m_srv));
		CD3DX12_DESCRIPTOR_RANGE range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		CD3DX12_ROOT_PARAMETER parameter;
		parameter.InitAsDescriptorTable(1, &range, D3D12_SHADER_VISIBILITY_PIXEL);
		CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		const CD3DX12_ROOT_SIGNATURE_DESC rootDesc(
			1, &parameter, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
		ComPtr<ID3DBlob> rootBlob, error, vs, ps;
		ThrowIfFailed(
			D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootBlob, &error));
		ThrowIfFailed(gpu->CreateRootSignature(
			0, rootBlob->GetBufferPointer(), rootBlob->GetBufferSize(), IID_PPV_ARGS(&m_root)));
		ThrowIfFailed(D3DReadFileToBlob((ExecutableDirectory() / L"HudVS.cso").c_str(), &vs));
		ThrowIfFailed(D3DReadFileToBlob((ExecutableDirectory() / L"HudPS.cso").c_str(), &ps));
		const D3D12_INPUT_ELEMENT_DESC layout[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0}};
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
		desc.pRootSignature = m_root.Get();
		desc.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
		desc.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
		desc.InputLayout = {layout, 3};
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
		for (UINT frame = 0; frame < RenderDevice::FrameCount; ++frame)
			m_vertices[frame] =
				CreateUploadBuffer(gpu, HudDrawList::MaxVertices * sizeof(HudVertex), &m_mapped[frame]);
	}
	void HudRenderer::Record(
		RenderDevice& device, ID3D12GraphicsCommandList* commands, const HudDrawList& list) {
		const auto& vertices = list.GetVertices();
		if (vertices.empty())
			return;
		if (vertices.size() > HudDrawList::MaxVertices)
			throw std::length_error("HUD vertex capacity exceeded");
		const auto frame = device.GetFrameIndex();
		std::memcpy(m_mapped[frame], vertices.data(), vertices.size() * sizeof(HudVertex));
		commands->SetGraphicsRootSignature(m_root.Get());
		commands->SetPipelineState(m_pipeline.Get());
		ID3D12DescriptorHeap* heaps[] = {device.GetSrvDescriptorHeap()};
		commands->SetDescriptorHeaps(1, heaps);
		commands->SetGraphicsRootDescriptorTable(0, device.GetSrvGpuHandle(m_srv));
		const D3D12_VERTEX_BUFFER_VIEW view{m_vertices[frame]->GetGPUVirtualAddress(),
			UINT(vertices.size() * sizeof(HudVertex)), sizeof(HudVertex)};
		commands->IASetVertexBuffers(0, 1, &view);
		commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commands->DrawInstanced(UINT(vertices.size()), 1, 0, 0);
	}
} // namespace Engine
