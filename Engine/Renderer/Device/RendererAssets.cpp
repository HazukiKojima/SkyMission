#include "RendererAssets.h"

namespace Engine {
	void RendererAssets::InitializeSkySphere(ID3D12Device* device, float radius) {
		m_skySphere = std::make_unique<SkySphere>();
		m_skySphere->Initialize(device, radius, 64, 32);
	}

	void RendererAssets::Load(RenderDevice* device, CommandContext* context, const SceneDefinition& assets) {
		context->BeginFrame();
		UINT oceanTextureSrvIndex = 0;
		const D3D12_CPU_DESCRIPTOR_HANDLE oceanTextureCpuHandle = device->AllocateSrvDescriptor(&oceanTextureSrvIndex);
		m_oceanTexture = Texture::LoadCached(device->GetDevice(), context->GetCommandList(), assets.oceanTexture.wstring(), true, true);
		if (!m_oceanTexture) {
			OutputDebugStringA("Application::Initialize - failed to load texture\n");
		}
		else {
			m_oceanTexture->CreateShaderResourceView(device->GetDevice(), oceanTextureCpuHandle);
		}

		UINT oceanNormalTextureSrvIndex = 0;
		const D3D12_CPU_DESCRIPTOR_HANDLE oceanNormalTextureCpuHandle = device->AllocateSrvDescriptor(&oceanNormalTextureSrvIndex);
		m_oceanNormalTexture = Texture::LoadCached(device->GetDevice(), context->GetCommandList(), assets.oceanNormalTexture.wstring(), false, true);
		if (!m_oceanNormalTexture) {
			OutputDebugStringA("Application::Initialize - failed to load ocean normal map\n");
		}
		else {
			m_oceanNormalTexture->CreateShaderResourceView(device->GetDevice(), oceanNormalTextureCpuHandle);
		}

		UINT oceanNormalTexture2SrvIndex = 0;
		const D3D12_CPU_DESCRIPTOR_HANDLE oceanNormalTexture2CpuHandle = device->AllocateSrvDescriptor(&oceanNormalTexture2SrvIndex);
		m_oceanNormalTexture2 = Texture::LoadCached(device->GetDevice(), context->GetCommandList(), assets.oceanNormalTexture2.wstring(), false, true);
		if (!m_oceanNormalTexture2) {
			OutputDebugStringA("Application::Initialize - failed to load second ocean normal map\n");
		}
		else {
			m_oceanNormalTexture2->CreateShaderResourceView(device->GetDevice(), oceanNormalTexture2CpuHandle);
		}
		context->EndFrame();
		context->WaitForGpu();
		if (m_oceanTexture) {
			m_oceanTexture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture) {
			m_oceanNormalTexture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture2) {
			m_oceanNormalTexture2->ReleaseUploadResources();
		}
		m_oceanTextureSrvIndex = oceanTextureSrvIndex;
		m_oceanNormalTextureSrvIndex = oceanNormalTextureSrvIndex;
		m_oceanNormalTexture2SrvIndex = oceanNormalTexture2SrvIndex;

		context->BeginFrame();
		UINT skyTextureSrvIndex = 0;
		const D3D12_CPU_DESCRIPTOR_HANDLE skyTextureCpuHandle = device->AllocateSrvDescriptor(&skyTextureSrvIndex);
		m_skyTexture = Texture::LoadCached(
			device->GetDevice(),
			context->GetCommandList(),
			assets.skyTexture.wstring());
		if (!m_skyTexture) {
			OutputDebugStringA("Failed to load sky texture from JSON path\n");
		}

		if (m_skyTexture) {
			m_skyTexture->CreateShaderResourceView(device->GetDevice(), skyTextureCpuHandle);
		}

		m_model = std::make_shared<Model>();
		if (!m_model->Load(device->GetDevice(), context->GetCommandList(), device, assets.playerAircraft.model.wstring())) {
			OutputDebugStringA("Failed to load player aircraft model\n");
			m_model.reset();
		}

		m_enemyAircraftModel = std::make_shared<Model>();
		if (!m_enemyAircraftModel->Load(device->GetDevice(), context->GetCommandList(), device, assets.enemyAircraft.model.wstring())) {
			OutputDebugStringA("Failed to load F4U4 Corsair model\n");
			m_enemyAircraftModel.reset();
		}

		context->EndFrame();
		context->WaitForGpu();
		if (m_oceanTexture) {
			m_oceanTexture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture) {
			m_oceanNormalTexture->ReleaseUploadResources();
		}
		if (m_oceanNormalTexture2) {
			m_oceanNormalTexture2->ReleaseUploadResources();
		}
		if (m_skyTexture) {
			m_skyTexture->ReleaseUploadResources();
		}
		if (m_model) {
			m_model->ReleaseUploadResources();
		}
		if (m_enemyAircraftModel) {
			m_enemyAircraftModel->ReleaseUploadResources();
		}
		m_skyTextureSrvIndex = skyTextureSrvIndex;
	}
}
