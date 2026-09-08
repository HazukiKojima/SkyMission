#include "Application.h"
#include "Window.h"
#include "../Renderer/Device/RenderDevice.h"
#include "../Renderer/Device/CommandContext.h"
#include "../Resources/Texture/Texture.h"
#include <DirectXMath.h>
#include <chrono>

namespace Engine {
	Application::Application(HINSTANCE hInstance) : m_hInstance(hInstance) {}

	Application::~Application() {
		if (m_context) {
			m_context->WaitForGpu(); // �������R�}���h�ɂ�郁�������[�N�⋭���I����}�~
		}
	}

	struct ConstantBufferData {
		DirectX::XMFLOAT4X4 mvp;
		float time;
		float padding[3];
		DirectX::XMFLOAT3 cameraPos;
		float pad2;
		DirectX::XMFLOAT3 sunDirection;
		float sunIntensity;
		DirectX::XMFLOAT3 sunColor;
		float ambientIntensity;
		DirectX::XMFLOAT3 ambientColor;
		float pad3;
	};

	// �A�v���P�[�V������Ղ���ъe�O���t�B�b�N�X�R���|�[�l���g�̍\�z
	void Application::Initialize() {
		m_window = std::make_unique<Window>(800, 600, L"SkyMission", m_hInstance);
		ShowWindow(m_window->GetHandle(), SW_SHOW);

		// ���T�C�Y�C�x���g��w�ǂ��ăf�o�C�X�Ⓤ�e�s���X�V
		m_window->SetOnResize([this](UINT w, UINT h) {
			if (w == 0 || h == 0) return; // �ŏ������Ȃǖ����Ȓl�𖳎�
			if (m_context) m_context->WaitForGpu(); // �������R�}���h����������Ă��烊�T�C�Y
			m_device->Resize(w, h);
			if (m_camera) m_camera->OnResize(w, h);
			// ���e�� Update() �Ŗ��t���[���Čv�Z���Ă��邽�߂����ł͉�����Ȃ�
		});

		m_device = std::make_unique<RenderDevice>();
		m_device->Initialize(m_window->GetHandle(), m_window->GetWidth(), m_window->GetHeight());

		m_context = std::make_unique<CommandContext>();
		m_context->Initialize(m_device.get());

		// �p�C�v���C���̏�����
		m_pipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_pipeline->Initialize(m_device->GetDevice());

		// Sky Sphere �p�C�v���C���̏�����
		m_skyPipeline = std::make_unique<Engine::GraphicsPipeline>();
		m_skyPipeline->InitializeWithShaders(m_device->GetDevice(), L"SkyVS.cso", L"SkyPS.cso");

		// Sky Sphere ���b�V���̏�����
		m_skySphere = std::make_unique<Engine::SkySphere>();
		m_skySphere->Initialize(m_device->GetDevice(), 100.0f, 64, 32);

		// �l�p�`�̒��_�f�[�^�쐬
		struct Vertex {
			float pos[3];
			float uv[2];
		};
		// --- 10x10 �O���b�h�̒��_�E�C���f�b�N�X���� ---
		const int gridSize = 256;
		const float gridExtent = 4000.0f;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		for (int z = 0; z < gridSize; ++z) {
			for (int x = 0; x < gridSize; ++x) {
				float px = (float)x / (gridSize - 1) * gridExtent - gridExtent * 0.5f;
				float pz = (float)z / (gridSize - 1) * gridExtent - gridExtent * 0.5f;
				float u = (float)x / (gridSize - 1);
				float v = (float)z / (gridSize - 1);
				vertices.push_back({ {px, 0.0f, pz}, {u, v} });
			}
		}

		for (int z = 0; z < gridSize - 1; ++z) {
			for (int x = 0; x < gridSize - 1; ++x) {
				uint32_t i0 = z * gridSize + x;
				uint32_t i1 = i0 + 1;
				uint32_t i2 = (z + 1) * gridSize + x;
				uint32_t i3 = i2 + 1;
				indices.push_back(i0); indices.push_back(i1); indices.push_back(i2);
				indices.push_back(i1); indices.push_back(i3); indices.push_back(i2);
			}
		}
		m_indexCount = (UINT)indices.size();

		// --- ���_�o�b�t�@�̏����� ---
		m_vertexBuffer = std::make_unique<Engine::VertexBuffer>();
		m_vertexBuffer->Initialize(m_device->GetDevice(), vertices.data(), sizeof(Vertex) * vertices.size(), sizeof(Vertex));

		// --- �C���f�b�N�X�o�b�t�@�̍쐬 ---
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		auto desc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(uint32_t) * indices.size());
		ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr, IID_PPV_ARGS(&m_indexBuffer)));

		void* pData;
		m_indexBuffer->Map(0, nullptr, &pData);
		memcpy(pData, indices.data(), sizeof(uint32_t) * indices.size());
		m_indexBuffer->Unmap(0, nullptr);

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		m_indexBufferView.SizeInBytes = sizeof(uint32_t) * indices.size();

		// �e�N�X�`����ǂݍ��݁ASRV ��쐬���ăf�B�X�N���v�^�q�[�v�֔z�u
		m_texture = std::make_unique<Engine::Texture>();
		// �R�}���h���X�g����Z�b�g���ăA�b�v���[�h������s��
		m_context->BeginFrame();
		UINT srvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_device->AllocateSrvDescriptor(&srvIndex);
		// ���s�t�@�C���̃p�X��擾����ȈՓI�Ȏ�@
		wchar_t buffer[MAX_PATH];
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::wstring exePath = buffer;
		std::wstring exeDir = exePath.substr(0, exePath.find_last_of(L"\\/"));

		// Assets �ւ̃p�X�𓮓I�ɉ��
		std::wstring path = exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg";
		if (!m_texture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
		}
		m_texture->CreateShaderResourceView(m_device->GetDevice(), cpuHandle);

		m_oceanNormalTexture = std::make_unique<Engine::Texture>();
		UINT normalSrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE normalCpuHandle = m_device->AllocateSrvDescriptor(&normalSrvIndex);
		std::vector<std::wstring> normalTexturePaths = {
			exeDir + L"\\..\\..\\Assets\\Images\\T_Ocean_Normal.jpg",
			exeDir + L"\\..\\..\\..\\Assets\\Images\\T_Ocean_Normal.jpg",
			exeDir + L"\\..\\Assets\\Images\\T_Ocean_Normal.jpg",
			exeDir + L"\\Assets\\Images\\T_Ocean_Normal.jpg"
		};
		bool normalTextureLoaded = false;
		for (const auto& normalPath : normalTexturePaths) {
			WIN32_FILE_ATTRIBUTE_DATA normalFileInfo;
			if (GetFileAttributesExW(normalPath.c_str(), GetFileExInfoStandard, &normalFileInfo) != 0 &&
				m_oceanNormalTexture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), normalPath, false)) {
				normalTextureLoaded = true;
				break;
			}
		}
		if (!normalTextureLoaded) {
			OutputDebugStringA("Application::Initialize - failed to load ocean normal map\n");
		}
		m_oceanNormalTexture->CreateShaderResourceView(m_device->GetDevice(), normalCpuHandle);
		m_context->EndFrame();
		// �A�b�v���[�h�I���܂őҋ@
		m_context->WaitForGpu();
		m_textureSrvIndex = srvIndex;
		m_oceanNormalTextureSrvIndex = normalSrvIndex;

		// Sky Sphere �e�N�X�`���iHDR/EXR�j�̓ǂݍ���
		// Sky Sphere ?e?N?X?`???iHDR/EXR?j???????
		m_skyTexture = std::make_unique<Engine::Texture>();
		m_context->BeginFrame();
		UINT skySrvIndex = 0;
		D3D12_CPU_DESCRIPTOR_HANDLE skyCpuHandle = m_device->AllocateSrvDescriptor(&skySrvIndex);
		
		// �����̃p�X�����
		std::vector<std::wstring> skyTexturePaths = {
			L"C:\\Users\\hazu0\\DX12\\SkyMission\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\..\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\Assets\\Images\\citrus_orchard_road_puresky_4k.hdr",
			exeDir + L"\\..\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.exr",
			exeDir + L"\\..\\Assets\\Images\\citrus_orchard_road_puresky_4k.exr",
			exeDir + L"\\Assets\\Images\\citrus_orchard_road_puresky_4k.exr",
			exeDir + L"\\Assets\\Images\\water-bg-pattern-04.jpg",
			exeDir + L"\\..\\..\\Assets\\Images\\water-bg-pattern-04.jpg",
		};
		
		bool skyTextureLoaded = false;
		for (const auto& path : skyTexturePaths) {
			// �t�@�C�������݂��邩�m�F
			WIN32_FILE_ATTRIBUTE_DATA fileInfo;
			if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fileInfo) != 0) {
				char pathBuffer[512];
				size_t converted = 0;
				wcstombs_s(&converted, pathBuffer, sizeof(pathBuffer), path.c_str(), _TRUNCATE);
				OutputDebugStringA("Trying to load sky texture from: ");
				OutputDebugStringA(pathBuffer);
				OutputDebugStringA("\n");
				
				if (m_skyTexture->LoadFromFile(m_device->GetDevice(), m_context->GetCommandList(), path)) {
					OutputDebugStringA("Sky texture loaded successfully!\n");
					skyTextureLoaded = true;
					break;
				}
				else {
					OutputDebugStringA("Failed to load this file.\n");
				}
			}
			else {
				char pathBuffer[512];
				size_t converted = 0;
				wcstombs_s(&converted, pathBuffer, sizeof(pathBuffer), path.c_str(), _TRUNCATE);
				OutputDebugStringA("File not found: ");
				OutputDebugStringA(pathBuffer);
				OutputDebugStringA("\n");
			}
		}
		
		if (!skyTextureLoaded) {
			OutputDebugStringA("Warning: No sky texture could be loaded from any path.\n");
		}
		
		m_skyTexture->CreateShaderResourceView(m_device->GetDevice(), skyCpuHandle);
		m_context->EndFrame();
		m_context->WaitForGpu();
		m_skyTextureSrvIndex = skySrvIndex;

		// �萔�o�b�t�@ (MVP) ��쐬���ăg�b�v�_�E�����_�̍s���ݒ�
		{
			using namespace DirectX;
			UINT64 cbSize = (sizeof(ConstantBufferData) + 255) & ~255; // 256 �o�C�g���E�ɃA���C��

			CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);
			CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
			ThrowIfFailed(m_device->GetDevice()->CreateCommittedResource(
				&heapProps,
				D3D12_HEAP_FLAG_NONE,
				&desc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_constantBuffer)));

			// �}�b�v���čs����������
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_cbvDataPtr)));

			XMMATRIX world = XMMatrixIdentity();
			// �J���������ɒu���A���_����� (Y���������)
			XMVECTOR eye = XMVectorSet(10.0f, 15.0f, -10.0f, 0.0f);
			XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
			XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			XMMATRIX view = XMMatrixLookAtLH(eye, at, up);
			float aspect = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
			XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, aspect, 0.1f, 100.0f);
			XMMATRIX mvp = world * view * proj;
			XMMATRIX mvpT = XMMatrixTranspose(mvp); // �V�F�[�_�Ƃ̍s��I�[�_�݊��̂��ߓ]�u

			XMFLOAT4X4 m;
			XMStoreFloat4x4(&m, mvpT);
			// �����l��������ށi�J�����͏����� eye �ƍ��킹��j
			ConstantBufferData* cbInit = reinterpret_cast<ConstantBufferData*>(m_cbvDataPtr);
			cbInit->mvp = m;
			cbInit->time = 0.0f;
			cbInit->cameraPos = DirectX::XMFLOAT3(10.0f, 15.0f, -10.0f);
			cbInit->sunDirection = DirectX::XMFLOAT3(0.32f, 0.88f, -0.28f);
			cbInit->sunIntensity = 1.8f;
			cbInit->sunColor = DirectX::XMFLOAT3(1.0f, 0.91f, 0.76f);
			cbInit->ambientIntensity = 0.32f;
			cbInit->ambientColor = DirectX::XMFLOAT3(0.18f, 0.32f, 0.48f);
		}

		m_camera = std::make_unique<Engine::Camera>();
		m_camera->Initialize(m_window->GetHandle(), DirectX::XM_PIDIV4, static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight()), 0.1f, 5000.0f);
		m_lastTime = std::chrono::steady_clock::now();
	}

	// ���b�Z�[�W���[�v�̋쓮����у��C���X�V�E�`��p�X�̐���
	int Application::Run() {
		MSG msg = {};
		while (msg.message != WM_QUIT) {
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else {
				Update();
				Render();
			}
		}
		return static_cast<int>(msg.wParam);
	}

	void Application::Update() {
		static float time = 0.0f;
		// compute delta
		auto now = std::chrono::steady_clock::now();
		std::chrono::duration<float> dt = now - m_lastTime;
		m_lastTime = now;
		float deltaSeconds = dt.count();

		time += deltaSeconds; // use real delta time for animation speed

		// Update camera first
		if (m_camera) m_camera->Update(deltaSeconds);

		// MVP�s���Čv�Z
		using namespace DirectX;
		XMMATRIX world = XMMatrixIdentity();
		XMMATRIX view = m_camera->GetView();

		float aspect = static_cast<float>(m_window->GetWidth()) / static_cast<float>(m_window->GetHeight());
		XMMATRIX proj = m_camera->GetProjection();
		XMMATRIX mvp = world * view * proj;
		XMMATRIX mvpT = XMMatrixTranspose(mvp);

		DirectX::XMFLOAT4X4 m;
		XMStoreFloat4x4(&m, mvpT);

		// �萔�o�b�t�@��X�V
		ConstantBufferData* data;
		m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&data));

		data->mvp = m;
		data->time = time;
		// �J�����ʒu����݂̃J��������擾�iVS/PS �̃t���l���v�Z�p�j
		if (m_camera) {
			auto camPos = m_camera->GetPosition();
			data->cameraPos = camPos;
		}
		data->sunDirection = DirectX::XMFLOAT3(0.32f, 0.88f, -0.28f);
		data->sunIntensity = 1.8f;
		data->sunColor = DirectX::XMFLOAT3(1.0f, 0.91f, 0.76f);
		data->ambientIntensity = 0.32f;
		data->ambientColor = DirectX::XMFLOAT3(0.18f, 0.32f, 0.48f);

		m_constantBuffer->Unmap(0, nullptr);
	}

	// �t���[���̃����_�����O�R�}���h�����E���s�p�X
	void Application::Render() {
		m_context->BeginFrame();

		auto cmd = m_context->GetCommandList();
		auto resource = m_device->GetCurrentRenderTarget();

		// �����_�[�^�[�Q�b�g�֑J��
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(m_window->GetWidth()), static_cast<float>(m_window->GetHeight()), 0.0f, 1.0f };
		D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(m_window->GetWidth()), static_cast<LONG>(m_window->GetHeight()) };

		cmd->RSSetViewports(1, &viewport);
		cmd->RSSetScissorRects(1, &scissorRect);

		// �N���A�Ɛݒ�
		auto rtv = m_device->GetCurrentRtvHandle();
		const float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
		cmd->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
		cmd->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

		// Sky Sphere ��ŏ��ɕ`��i�w�i�Ƃ��āj
		if (m_skySphere && m_skyTexture && m_skyTexture->GetResource()) {
			cmd->SetGraphicsRootSignature(m_skyPipeline->GetRootSignature());
			cmd->SetPipelineState(m_skyPipeline->GetPSO());

			// Sky Sphere �p�̃e�N�X�`����o�C���h
			ID3D12DescriptorHeap* heaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_skyTextureSrvIndex));

			// �萔�o�b�t�@��o�C���h
			if (m_constantBuffer) {
				cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
			}

			// Sky Sphere �̒��_�E�C���f�b�N�X�o�b�t�@��o�C���h
			auto skyView = m_skySphere->GetVertexBufferView();
			auto& skyIndexView = m_skySphere->GetIndexBufferView();
			cmd->IASetVertexBuffers(0, 1, &skyView);
			cmd->IASetIndexBuffer(&skyIndexView);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// Sky Sphere ��`��
			cmd->DrawIndexedInstanced(m_skySphere->GetIndexCount(), 1, 0, 0, 0);
		}

		// ���ɐ��ʃ��b�V����`��i�O�i�Ƃ��āj
		cmd->SetGraphicsRootSignature(m_pipeline->GetRootSignature());
		cmd->SetPipelineState(m_pipeline->GetPSO());

		// �e�N�X�`��������΃f�B�X�N���v�^�q�[�v��Z�b�g���ă��[�g�� SRV ��o�C���h
		if (m_texture && m_oceanNormalTexture) {
			ID3D12DescriptorHeap* heaps[] = { m_device->GetSrvDescriptorHeap() };
			cmd->SetDescriptorHeaps(_countof(heaps), heaps);
			cmd->SetGraphicsRootDescriptorTable(0, m_device->GetSrvGpuHandle(m_textureSrvIndex));
		}

		// ���_�V�F�[�_�p�̒萔�o�b�t�@����[�g�Ƀo�C���h
		if (m_constantBuffer) {
			cmd->SetGraphicsRootConstantBufferView(1, m_constantBuffer->GetGPUVirtualAddress());
		}

		// �`��ݒ�i���_�o�b�t�@��o�C���h�j
		auto view = m_vertexBuffer->GetView();
		cmd->IASetVertexBuffers(0, 1, &view);
		cmd->IASetIndexBuffer(&m_indexBufferView);
		cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// ���ʃ��b�V����`��
		cmd->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

		// Present �֑J��
		m_context->TransitionResource(resource, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		m_context->EndFrame();
		m_device->Present();
		m_context->MoveToNextFrame(m_device.get());
	}

}