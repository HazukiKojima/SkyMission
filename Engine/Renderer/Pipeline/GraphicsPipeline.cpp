#include "GraphicsPipeline.h"
#include <d3dcompiler.h> // コンパイル用

namespace Engine {
	void GraphicsPipeline::Initialize(ID3D12Device* device) {
		OutputDebugStringA("DEBUG: Starting Pipeline Initialize\n");

		// ルートシグネチャ: t0 に SRV をバインドするディスクリプタテーブル
		// 頂点シェーダ用の定数バッファ(b0)とサンプラを用意する
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

		// ルートパラメータを2つ用意: 0 = SRV テーブル (ピクセルシェーダ用), 1 = CBV(b0) (頂点シェーダ用)
		CD3DX12_ROOT_PARAMETER1 rootParams[2];
		rootParams[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
		// static helper を使って rootParams[1] を初期化
		CD3DX12_ROOT_PARAMETER1::InitAsConstantBufferView(rootParams[1], 0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX);

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

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		// CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC は D3D12_VERSIONED_ROOT_SIGNATURE_DESC のラッパーなので、そのアドレスを渡す
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

		// PSOの構築
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_rootSignature.Get();

		// シェーダーのポインタとサイズを渡す
		psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
		psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };

		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// ラスタライザ設定を明示的に構築
		CD3DX12_RASTERIZER_DESC rastDesc(D3D12_DEFAULT);
		// 一時的に裏面カリングを無効化して板ポリゴンが見えるようにする
		rastDesc.CullMode = D3D12_CULL_MODE_NONE;
		psoDesc.RasterizerState = rastDesc;

		// ブレンドステート設定
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
}