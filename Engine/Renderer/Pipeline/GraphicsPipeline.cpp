#include "GraphicsPipeline.h"
#include <d3dcompiler.h> // コンパイル用

namespace
{
	// ルートシグネチャを生成するヘルパー
	// srvCount: ルートテーブル内のSRV数
	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignatureWithSrvCount(
		ID3D12Device* device,
		UINT srvCount)
	{
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(
			D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
			srvCount,
			0);

		// ルートパラメータを2つ用意
		// 0 = SRVテーブル（ピクセルシェーダ用）
		// 1 = CBV(b0)（頂点/ピクセル両方で使用）
		CD3DX12_ROOT_PARAMETER1 rootParams[2];

		rootParams[0].InitAsDescriptorTable(
			1,
			&ranges[0],
			D3D12_SHADER_VISIBILITY_PIXEL);

		CD3DX12_ROOT_PARAMETER1::InitAsConstantBufferView(
			rootParams[1],
			0,
			0,
			D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
			D3D12_SHADER_VISIBILITY_ALL);

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;

		rootSigDesc.Init_1_1(
			_countof(rootParams),
			rootParams,
			1,
			&samplerDesc,
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		Microsoft::WRL::ComPtr<ID3DBlob> signature;
		Microsoft::WRL::ComPtr<ID3DBlob> error;

		ThrowIfFailed(
			D3D12SerializeVersionedRootSignature(
				&rootSigDesc,
				&signature,
				&error));

		Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;

		ThrowIfFailed(
			device->CreateRootSignature(
				0,
				signature->GetBufferPointer(),
				signature->GetBufferSize(),
				IID_PPV_ARGS(&rootSignature)));

		return rootSignature;
	}

	// グラフィックスパイプラインステートを生成するヘルパー
	Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePipelineState(
		ID3D12Device* device,
		ID3D12RootSignature* rootSignature,
		ID3DBlob* vertexShader,
		ID3DBlob* pixelShader,
		bool enableAlphaBlend,
		bool enableDepth)
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{
				"POSITION",
				0,
				DXGI_FORMAT_R32G32B32_FLOAT,
				0,
				0,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			},
			{
				"TEXCOORD",
				0,
				DXGI_FORMAT_R32G32_FLOAT,
				0,
				12,
				D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
				0
			}
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

		psoDesc.pRootSignature = rootSignature;

		psoDesc.VS =
		{
			vertexShader->GetBufferPointer(),
			vertexShader->GetBufferSize()
		};

		psoDesc.PS =
		{
			pixelShader->GetBufferPointer(),
			pixelShader->GetBufferSize()
		};

		psoDesc.InputLayout =
		{
			inputElementDescs,
			_countof(inputElementDescs)
		};

		psoDesc.PrimitiveTopologyType =
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// ラスタライザ設定
		CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);

		// 裏面カリングを無効化
		rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;

		psoDesc.RasterizerState = rasterizerDesc;

		// ブレンド設定
		CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
		if (enableAlphaBlend) {
			blendDesc.RenderTarget[0].BlendEnable = TRUE;
			blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
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
		depthDesc.DepthEnable = enableDepth;
		depthDesc.DepthWriteMask = enableDepth ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		psoDesc.DepthStencilState = depthDesc;
		psoDesc.DSVFormat = enableDepth ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_UNKNOWN;

		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;

		ThrowIfFailed(
			device->CreateGraphicsPipelineState(
				&psoDesc,
				IID_PPV_ARGS(&pso)));

		return pso;
	}
}

namespace Engine
{
	void GraphicsPipeline::Initialize(ID3D12Device* device)
	{
		OutputDebugStringA(
			"DEBUG: Starting Pipeline Initialize\n");

		// BasicPSはDiffuseのみ使用
		m_rootSignature =
			CreateRootSignatureWithSrvCount(device, 1);

		OutputDebugStringA(
			"DEBUG: RootSignature created\n");

		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

		HRESULT hrVS =
			D3DReadFileToBlob(
				L"BasicVS.cso",
				&vertexShader);

		if (FAILED(hrVS))
		{
			OutputDebugStringA(
				"ERROR: Failed to load BasicVS.cso\n");

			ThrowIfFailed(hrVS);
		}

		HRESULT hrPS =
			D3DReadFileToBlob(
				L"BasicPS.cso",
				&pixelShader);

		if (FAILED(hrPS))
		{
			OutputDebugStringA(
				"ERROR: Failed to load BasicPS.cso\n");

			ThrowIfFailed(hrPS);
		}

		m_pso =
			CreatePipelineState(
				device,
				m_rootSignature.Get(),
				vertexShader.Get(),
				pixelShader.Get(),
				false,
				false);

		OutputDebugStringA(
			"DEBUG: PipelineStateObject created\n");
	}

	void GraphicsPipeline::InitializeWithShaders(
		ID3D12Device* device,
		const std::wstring& vsPath,
		const std::wstring& psPath,
		UINT srvCount,
		bool enableAlphaBlend,
		bool enableDepth)
	{
		OutputDebugStringA(
			"DEBUG: Starting Pipeline InitializeWithShaders\n");

		m_rootSignature =
			CreateRootSignatureWithSrvCount(
				device,
				srvCount);

		Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
		Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;

		HRESULT hrVS =
			D3DReadFileToBlob(
				vsPath.c_str(),
				&vertexShader);

		if (FAILED(hrVS))
		{
			OutputDebugStringA(
				"ERROR: Failed to load vertex shader\n");

			ThrowIfFailed(hrVS);
		}

		HRESULT hrPS =
			D3DReadFileToBlob(
				psPath.c_str(),
				&pixelShader);

		if (FAILED(hrPS))
		{
			OutputDebugStringA(
				"ERROR: Failed to load pixel shader\n");

			ThrowIfFailed(hrPS);
		}

		m_pso =
			CreatePipelineState(
				device,
				m_rootSignature.Get(),
				vertexShader.Get(),
				pixelShader.Get(),
				enableAlphaBlend,
				enableDepth);

		OutputDebugStringA(
			"DEBUG: PipelineStateObject created with custom shaders\n");
	}
}