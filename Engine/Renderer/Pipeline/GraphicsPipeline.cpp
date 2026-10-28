#include "GraphicsPipeline.h"
#include "../../ExecutablePath.h"
#include "../../Log.h"
#include <d3dcompiler.h> // コンパイル用

namespace {
	namespace Binding = Engine::RootBinding;
	// ルートシグネチャを生成するヘルパー
	// srvCount: ルートテーブル内のSRV数
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignatureWithSrvCount(ID3D12Device* device,
		UINT srvCount, D3D12_TEXTURE_ADDRESS_MODE addressMode, Engine::RootSignatureLayout layout) {
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, srvCount, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[Binding::NodeRotationParameterCount];
		rootParams[Binding::TextureTable].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		rootParams[Binding::FrameConstants].InitAsConstantBufferView(
			0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);
		const bool nodeRotation = layout == Engine::RootSignatureLayout::NodeRotation;
		if (nodeRotation)
			rootParams[Binding::NodeRotation].InitAsConstants(
				Binding::NodeRotationDwords, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		const UINT parameterCount =
			nodeRotation ? Binding::NodeRotationParameterCount : Binding::TexturedParameterCount;

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = addressMode;
		samplerDesc.AddressV = addressMode;
		samplerDesc.AddressW = addressMode;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;

		rootSigDesc.Init_1_1(parameterCount, rootParams, 1, &samplerDesc,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;

		const HRESULT result = D3D12SerializeVersionedRootSignature(&rootSigDesc, &signature, &error);
		if (FAILED(result) && error) {
			Engine::Log("Root signature serialization: {}\n",
				std::string(static_cast<const char*>(error->GetBufferPointer()), error->GetBufferSize()));
		}

		ThrowIfFailed(result);

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

		ThrowIfFailed(device->CreateRootSignature(
			0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)));

		return rootSignature;
	}

	// グラフィックスパイプラインステートを生成するヘルパー
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineState(ID3D12Device* device,
		ID3D12RootSignature* rootSignature, ID3DBlob* vertexShader, ID3DBlob* pixelShader,
		const Engine::PipelineOptions& options) {
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
			{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0}};
		const UINT inputElementCount =
			options.vertexNormalTangent ? _countof(inputElementDescs) : _countof(inputElementDescs) - 2;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		psoDesc.pRootSignature = rootSignature;

		psoDesc.VS = {vertexShader->GetBufferPointer(), vertexShader->GetBufferSize()};

		psoDesc.PS = {pixelShader->GetBufferPointer(), pixelShader->GetBufferSize()};

		psoDesc.InputLayout = {inputElementDescs, inputElementCount};

		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// ラスタライザ設定
		CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);

		// Imported model winding is converted to left-handed coordinates by Assimp.
		rasterizerDesc.CullMode = options.cullMode;

		psoDesc.RasterizerState = rasterizerDesc;

		// ブレンド設定
		CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
		if (options.alphaBlend) {
			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend =
				options.additiveBlend ? D3D12_BLEND_ONE : D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
			blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
			blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		}

		psoDesc.BlendState = blendDesc;

		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		psoDesc.SampleMask = UINT_MAX;

		CD3DX12_DEPTH_STENCIL_DESC depthDesc(D3D12_DEFAULT);
		depthDesc.DepthEnable = options.depth;
		depthDesc.DepthWriteMask =
			options.depth && options.depthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState = depthDesc;
		psoDesc.DSVFormat = options.depth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;

		ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)));

		return pso;
	}
} // namespace

namespace Engine {
	void GraphicsPipeline::Initialize(ID3D12Device* device, const PipelineOptions& options) {
		m_rootSignature = CreateRootSignatureWithSrvCount(
			device, options.srvCount, options.addressMode, options.rootLayout);
		const auto directory = ExecutableDirectory();
		ComPtr<ID3DBlob> vertexShader, pixelShader;
		const auto loadShader = [&](const std::wstring& path, ID3DBlob** blob) {
			const auto absolute = directory / std::filesystem::path(path);
			const HRESULT result = D3DReadFileToBlob(absolute.c_str(), blob);
			if (FAILED(result)) {
				const auto utf8 = absolute.u8string();
				Log("Failed to load shader {} (HR: 0x{:08X})\n",
					std::string(reinterpret_cast<const char*>(utf8.data()), utf8.size()),
					static_cast<unsigned long>(result));
			}

			ThrowIfFailed(result);
		};
		loadShader(options.vs, &vertexShader);
		loadShader(options.ps, &pixelShader);
		m_pso = CreatePipelineState(
			device, m_rootSignature.Get(), vertexShader.Get(), pixelShader.Get(), options);
	}
} // namespace Engine
