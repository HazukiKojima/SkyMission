#include "GraphicsPipeline.h"
#include <d3dcompiler.h> // コンパイル用

namespace Engine {
	void GraphicsPipeline::Initialize(ID3D12Device* device) {
		OutputDebugStringA("DEBUG: Starting Pipeline Initialize\n");

		CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> signature;
		ComPtr<ID3DBlob> error;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
		ThrowIfFailed(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
		OutputDebugStringA("DEBUG: RootSignature created\n");

		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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