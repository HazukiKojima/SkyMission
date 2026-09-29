#include "GraphicsPipeline.h"
#include <d3dcompiler.h> // シェーダコンパイル用ヘッダ

namespace Engine {
	void GraphicsPipeline::Initialize(ID3D12Device* device) {
		OutputDebugStringA("DEBUG: Starting Pipeline Initialize\n");

		// ルートシグネチャ: ピクセルシェーダ用にSRVテーブルを用意
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0); // t0: diffuse, t1: normal map

		// ルートパラメータ: 0=SRVテーブル(ピクセル), 1=CBV(b0)（全シェーダ可視）
		CD3DX12_ROOT_PARAMETER1 rootParams[2];
		rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		// CBV を頂点/ピクセル両方で使用
		CD3DX12_ROOT_PARAMETER1::InitAsConstantBufferView(rootParams[1], 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);

		// デフォルトサンプラ設定
		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0; // s0
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		// ルートシグネチャ記述構築
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		// シリアライズしてルートシグネチャを生成
		ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSigDesc, &signature, &error));
		ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		OutputDebugStringA("DEBUG: RootSignature created\n");

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		OutputDebugStringA("DEBUG: Loading Shaders\n");
		ComPtr<ID3DBlob> vertexShader, pixelShader;

		HRESULT hrVS = D3DReadFileToBlob(L"BasicVS.cso", &vertexShader);
		if (FAILED(hrVS)) { OutputDebugStringA("ERROR: Failed to load BasicVS.cso\n"); ThrowIfFailed(hrVS); }

		HRESULT hrPS = D3DReadFileToBlob(L"BasicPS.cso", &pixelShader);
		if (FAILED(hrPS)) { OutputDebugStringA("ERROR: Failed to load BasicPS.cso\n"); ThrowIfFailed(hrPS); }
		OutputDebugStringA("DEBUG: Shaders loaded successfully\n");

		// PSO設定
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_rootSignature.Get();

		// シェーダーバイトコードを設定
		psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// ラスタライザ設定（裏面カリング無効にして板ポリが見えるように）
		CD3DX12_RASTERIZER_DESC rastDesc(D3D12_DEFAULT);
		rastDesc.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.RasterizerState = rastDesc;

		// ブレンド設定
		CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
		psoDesc.BlendState = blendDesc;

		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
		psoDesc.SampleMask = UINT_MAX;

		ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));

		HRESULT hrPSO = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso));
		if (FAILED(hrPSO)) {
			OutputDebugStringA("ERROR: CreateGraphicsPipelineState failed\n");
			ThrowIfFailed(hrPSO);
		}
		OutputDebugStringA("DEBUG: PipelineStateObject created\n");
	}

	void GraphicsPipeline::InitializeWithShaders(ID3D12Device* device, const std::wstring& vsPath, const std::wstring& psPath) {
		OutputDebugStringA("DEBUG: Starting Pipeline InitializeWithShaders\n");

		// ルートシグネチャ: 単一SRVテーブルを使用
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		CD3DX12_ROOT_PARAMETER1 rootParams[2];
		rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		CD3DX12_ROOT_PARAMETER1::InitAsConstantBufferView(rootParams[1], 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_ALL);

		// サンプラ設定
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
		rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSigDesc, &signature, &error));
		ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		ComPtr<ID3DBlob> vertexShader, pixelShader;

		HRESULT hrVS = D3DReadFileToBlob(vsPath.c_str(), &vertexShader);
		if (FAILED(hrVS)) {
			OutputDebugStringA("ERROR: Failed to load vertex shader\n");
			ThrowIfFailed(hrVS);
		}

		HRESULT hrPS = D3DReadFileToBlob(psPath.c_str(), &pixelShader);
		if (FAILED(hrPS)) {
			OutputDebugStringA("ERROR: Failed to load pixel shader\n");
			ThrowIfFailed(hrPS);
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		CD3DX12_RASTERIZER_DESC rastDesc(D3D12_DEFAULT);
		rastDesc.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.RasterizerState = rastDesc;

		CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
		psoDesc.BlendState = blendDesc;

		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;
		psoDesc.SampleMask = UINT_MAX;

		ThrowIfFailed(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pso)));
		OutputDebugStringA("DEBUG: PipelineStateObject created with custom shaders\n");
	}
}