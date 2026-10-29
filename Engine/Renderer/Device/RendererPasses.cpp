#include "../RootBindings.h"
#include "RendererPasses.h"
#include "CommandContext.h"
#include "../../Scene/Scene.h"
#include <cmath>

namespace Engine {
	void SkyRenderPass::Record(const FrameContext& frame, RendererAssets& assets) const {
		auto* commandList = frame.commandList;
		auto* device = frame.device;
		auto& pipelines = frame.pipelines;
		auto& constantBuffers = frame.constantBuffers;
		if (assets.GetSkySphere() && assets.GetSkyTexture() && assets.GetSkyTexture()->GetResource()) {
			commandList->SetGraphicsRootSignature(pipelines.GetSky()->GetRootSignature());
			commandList->SetPipelineState(pipelines.GetSky()->GetPSO());

			ID3D12DescriptorHeap* heaps[] = {device->GetSrvDescriptorHeap()};
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			commandList->SetGraphicsRootDescriptorTable(
				RootBinding::TextureTable, device->GetSrvGpuHandle(assets.GetSkyTextureSrvIndex()));

			if (constantBuffers.GetOceanResource(frame.frameIndex)) {
				commandList->SetGraphicsRootConstantBufferView(RootBinding::FrameConstants,
					constantBuffers.GetOceanResource(frame.frameIndex)->GetGPUVirtualAddress());
			}

			auto skyView = assets.GetSkySphere()->GetVertexBufferView();
			auto& skyIndexView = assets.GetSkySphere()->GetIndexBufferView();
			commandList->IASetVertexBuffers(0, 1, &skyView);
			commandList->IASetIndexBuffer(&skyIndexView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			commandList->DrawIndexedInstanced(assets.GetSkySphere()->GetIndexCount(), 1, 0, 0, 0);
		}
	}

	void OceanRenderPass::Record(
		const FrameContext& frame, RendererAssets& assets, OceanGeometry& geometry) const {
		auto* commandList = frame.commandList;
		auto* device = frame.device;
		auto& pipelines = frame.pipelines;
		auto& constantBuffers = frame.constantBuffers;
		if (!assets.GetOceanTexture() || !assets.GetOceanTexture()->IsLoaded() ||
			!assets.GetOceanNormalTexture() || !assets.GetOceanNormalTexture()->IsLoaded() ||
			!assets.GetOceanNormalTexture2() || !assets.GetOceanNormalTexture2()->IsLoaded() ||
			!constantBuffers.GetOceanResource(frame.frameIndex))
			return;

		commandList->SetGraphicsRootSignature(pipelines.GetOcean()->GetRootSignature());
		commandList->SetPipelineState(pipelines.GetOcean()->GetPSO());

		if (assets.GetOceanTexture() && assets.GetOceanNormalTexture() && assets.GetOceanNormalTexture2()) {
			ID3D12DescriptorHeap* heaps[] = {device->GetSrvDescriptorHeap()};
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			commandList->SetGraphicsRootDescriptorTable(
				RootBinding::TextureTable, device->GetSrvGpuHandle(assets.GetOceanTextureSrvIndex()));
		}

		if (constantBuffers.GetOceanResource(frame.frameIndex)) {
			commandList->SetGraphicsRootConstantBufferView(RootBinding::FrameConstants,
				constantBuffers.GetOceanResource(frame.frameIndex)->GetGPUVirtualAddress());
		}

		auto view = geometry.GetVertexView();
		commandList->IASetVertexBuffers(0, 1, &view);
		commandList->IASetIndexBuffer(&geometry.GetIndexView());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawIndexedInstanced(geometry.GetIndexCount(), 1, 0, 0, 0);
	}

	void ModelRenderPass::Record(const FrameContext& frame, const Scene& scene) const {
		auto* commandList = frame.commandList;
		auto* device = frame.device;
		auto& pipelines = frame.pipelines;
		auto& constantBuffers = frame.constantBuffers;
		const auto& objects = scene.GetObjects();
		const UINT frameIndex = frame.frameIndex;
		if (pipelines.GetModel()) {
			commandList->SetGraphicsRootSignature(pipelines.GetModel()->GetRootSignature());
			commandList->SetPipelineState(pipelines.GetModel()->GetPSO());
			ID3D12DescriptorHeap* modelHeaps[] = {device->GetSrvDescriptorHeap()};
			commandList->SetDescriptorHeaps(_countof(modelHeaps), modelHeaps);
			for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
				const SceneObject& object = objects[objectIndex];
				if (!object.model || !object.model->IsLoaded()) {
					continue;
				}

				commandList->SetGraphicsRootConstantBufferView(RootBinding::FrameConstants,
					constantBuffers.GetModelResource(frameIndex, objectIndex)->GetGPUVirtualAddress());
				object.model->Draw(commandList, device, object.nodeRotationRadians);
			}
		}
	}

	void OceanFogRenderPass::Record(const FrameContext& frame) const {
		auto* commandList = frame.commandList;
		auto* device = frame.device;
		auto& pipelines = frame.pipelines;
		auto& constantBuffers = frame.constantBuffers;
		commandList->SetGraphicsRootSignature(pipelines.GetOceanFog()->GetRootSignature());
		commandList->SetPipelineState(pipelines.GetOceanFog()->GetPSO());
		ID3D12DescriptorHeap* heaps[] = {device->GetSrvDescriptorHeap()};
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
		commandList->SetGraphicsRootDescriptorTable(
			RootBinding::TextureTable, device->GetSrvGpuHandle(device->GetSceneColorSrvIndex()));
		commandList->SetGraphicsRootConstantBufferView(RootBinding::FrameConstants,
			constantBuffers.GetOceanFogResource(frame.frameIndex)->GetGPUVirtualAddress());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}

	void RetroFilmRenderPass::Record(const FrameContext& frame) const {
		auto* commandList = frame.commandList;
		auto* device = frame.device;
		auto& pipelines = frame.pipelines;
		auto& constantBuffers = frame.constantBuffers;
		commandList->SetGraphicsRootSignature(pipelines.GetRetroFilm()->GetRootSignature());
		commandList->SetPipelineState(pipelines.GetRetroFilm()->GetPSO());
		ID3D12DescriptorHeap* heaps[] = {device->GetSrvDescriptorHeap()};
		commandList->SetDescriptorHeaps(_countof(heaps), heaps);
		commandList->SetGraphicsRootDescriptorTable(
			RootBinding::TextureTable, device->GetSrvGpuHandle(device->GetPostProcessSrvIndex()));
		const UINT frameIndex = frame.frameIndex;
		commandList->SetGraphicsRootConstantBufferView(
			RootBinding::FrameConstants, constantBuffers.GetFilmResource(frameIndex)->GetGPUVirtualAddress());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}
} // namespace Engine
