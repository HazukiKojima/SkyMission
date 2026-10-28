#include "RendererPasses.h"
#include "CommandContext.h"
#include "../../Scene/Scene.h"
#include <cmath>

namespace Engine {
	void SkyRenderPass::Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, RendererAssets& assets, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers) const {
		if (assets.GetSkySphere() && assets.GetSkyTexture() && assets.GetSkyTexture()->GetResource()) {
			commandList->SetGraphicsRootSignature(pipelines.GetSky()->GetRootSignature());
			commandList->SetPipelineState(pipelines.GetSky()->GetPSO());

			ID3D12DescriptorHeap* heaps[] = { device->GetSrvDescriptorHeap() };
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			commandList->SetGraphicsRootDescriptorTable(0, device->GetSrvGpuHandle(assets.GetSkyTextureSrvIndex()));

			if (constantBuffers.GetOceanResource()) {
				commandList->SetGraphicsRootConstantBufferView(1, constantBuffers.GetOceanResource()->GetGPUVirtualAddress());
			}

			auto skyView = assets.GetSkySphere()->GetVertexBufferView();
			auto& skyIndexView = assets.GetSkySphere()->GetIndexBufferView();
			commandList->IASetVertexBuffers(0, 1, &skyView);
			commandList->IASetIndexBuffer(&skyIndexView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(assets.GetSkySphere()->GetIndexCount(), 1, 0, 0, 0);
		}
	}

	void OceanRenderPass::Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, RendererAssets& assets, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers, OceanGeometry& geometry) const {
		commandList->SetGraphicsRootSignature(pipelines.GetOcean()->GetRootSignature());
		commandList->SetPipelineState(pipelines.GetOcean()->GetPSO());

		if (assets.GetOceanTexture() && assets.GetOceanNormalTexture() && assets.GetOceanNormalTexture2()) {
			ID3D12DescriptorHeap* heaps[] = { device->GetSrvDescriptorHeap() };
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			commandList->SetGraphicsRootDescriptorTable(0, device->GetSrvGpuHandle(assets.GetOceanTextureSrvIndex()));
		}

		if (constantBuffers.GetOceanResource()) {
			commandList->SetGraphicsRootConstantBufferView(1, constantBuffers.GetOceanResource()->GetGPUVirtualAddress());
		}

		auto view = geometry.GetVertexView();
		commandList->IASetVertexBuffers(0, 1, &view);
		commandList->IASetIndexBuffer(&geometry.GetIndexView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawIndexedInstanced(geometry.GetIndexCount(), 1, 0, 0, 0);
	}

	void ModelRenderPass::Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, CommandContext* context, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers, const Scene& scene, float propellerAngle) const {
		const auto& objects = scene.GetObjects();
		const UINT frameIndex = context->GetFrameIndex();
		if (pipelines.GetModel()) {
			commandList->SetGraphicsRootSignature(pipelines.GetModel()->GetRootSignature());
			commandList->SetPipelineState(pipelines.GetModel()->GetPSO());
			ID3D12DescriptorHeap* modelHeaps[] = { device->GetSrvDescriptorHeap() };
			commandList->SetDescriptorHeaps(_countof(modelHeaps), modelHeaps);
			for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
				const SceneObject& object = objects[objectIndex];
				if (!object.model || !object.model->IsLoaded()) {
					continue;
				}
				commandList->SetGraphicsRootConstantBufferView(1, constantBuffers.GetModelResource(frameIndex, objectIndex)->GetGPUVirtualAddress());
				object.model->Draw(commandList, device, propellerAngle);
			}
		}
	}

	void OceanFogRenderPass::Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers) const {
		commandList->SetGraphicsRootSignature(pipelines.GetOceanFog()->GetRootSignature());
		commandList->SetPipelineState(pipelines.GetOceanFog()->GetPSO());
		ID3D12DescriptorHeap* heaps[] = { device->GetSrvDescriptorHeap() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
		commandList->SetGraphicsRootDescriptorTable(0, device->GetSrvGpuHandle(device->GetSceneColorSrvIndex()));
		commandList->SetGraphicsRootConstantBufferView(1, constantBuffers.GetOceanFogResource()->GetGPUVirtualAddress());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	void RetroFilmRenderPass::Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, CommandContext* context, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers, const RetroFilmParameters& parameters, float elapsedTime) const {
		commandList->SetGraphicsRootSignature(pipelines.GetRetroFilm()->GetRootSignature());
		commandList->SetPipelineState(pipelines.GetRetroFilm()->GetPSO());
		ID3D12DescriptorHeap* heaps[] = { device->GetSrvDescriptorHeap() };
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
		commandList->SetGraphicsRootDescriptorTable(0, device->GetSrvGpuHandle(device->GetPostProcessSrvIndex()));
		const UINT frameIndex = context->GetFrameIndex();
		FilmConstantBufferData* filmConstants = constantBuffers.GetFilmData(frameIndex);
		filmConstants->time = std::fmod(elapsedTime, 3600.0f);
		filmConstants->padding[0] = filmConstants->padding[1] = filmConstants->padding[2] = 0.0f;
		filmConstants->parameters = parameters;
		commandList->SetGraphicsRootConstantBufferView(1, constantBuffers.GetFilmResource(frameIndex)->GetGPUVirtualAddress());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}
}
