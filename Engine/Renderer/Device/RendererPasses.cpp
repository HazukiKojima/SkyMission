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

	void ModelRenderPass::Record(const FrameContext& frame, const Scene& scene,
		std::span<const ModelDrawDecision> decisions, RenderStatistics& statistics) const {
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
				const auto decision = decisions[objectIndex];
				if (!decision.visible) {
					++statistics.culledObjects;
					continue;
				}
				if (!object.model || !object.model->IsLoaded()) continue;

				commandList->SetGraphicsRootConstantBufferView(RootBinding::FrameConstants,
					constantBuffers.GetModelResource(frameIndex, objectIndex)->GetGPUVirtualAddress());
				++statistics.visibleObjects;
				++statistics.lodObjects[decision.lod];
				statistics.modelDrawCalls += object.model->GetMeshCount(decision.lod);
				statistics.modelTriangles += object.model->GetTriangleCount(decision.lod);
				object.model->Draw(commandList, device, object.nodeRotationRadians, decision.lod);
			}
		}
	}

	void CloudRenderPass::Record(const FrameContext& frame) const {
		auto* commands = frame.commandList;
		auto* device = frame.device;
		auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(device->GetCloudRenderTarget(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commands->ResourceBarrier(1, &barrier);
		const D3D12_VIEWPORT viewport{
			0, 0, float(device->GetCloudWidth()), float(device->GetCloudHeight()), 0, 1};
		const D3D12_RECT scissor{0, 0, LONG(device->GetCloudWidth()), LONG(device->GetCloudHeight())};
		commands->RSSetViewports(1, &viewport);
		commands->RSSetScissorRects(1, &scissor);
		const auto rtv = device->GetCloudRtvHandle();
		commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		commands->SetGraphicsRootSignature(frame.pipelines.GetClouds()->GetRootSignature());
		commands->SetPipelineState(frame.pipelines.GetClouds()->GetPSO());
		ID3D12DescriptorHeap* heaps[] = {device->GetSrvDescriptorHeap()};
		commands->SetDescriptorHeaps(_countof(heaps), heaps);
		commands->SetGraphicsRootDescriptorTable(
			RootBinding::TextureTable, device->GetSrvGpuHandle(device->GetSceneColorSrvIndex()));
		commands->SetGraphicsRootConstantBufferView(RootBinding::FrameConstants,
			frame.constantBuffers.GetOceanFogResource(frame.frameIndex)->GetGPUVirtualAddress());
		commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commands->DrawInstanced(3, 1, 0, 0);
		barrier = CD3DX12_RESOURCE_BARRIER::Transition(device->GetCloudRenderTarget(),
			D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		commands->ResourceBarrier(1, &barrier);
		const D3D12_VIEWPORT full{0, 0, float(device->GetWidth()), float(device->GetHeight()), 0, 1};
		const D3D12_RECT fullScissor{0, 0, LONG(device->GetWidth()), LONG(device->GetHeight())};
		commands->RSSetViewports(1, &full);
		commands->RSSetScissorRects(1, &fullScissor);
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
			RootBinding::TextureTable, device->GetSrvGpuHandle(device->GetSceneColorSrvIndex()));
		const UINT frameIndex = frame.frameIndex;
		commandList->SetGraphicsRootConstantBufferView(
			RootBinding::FrameConstants, constantBuffers.GetFilmResource(frameIndex)->GetGPUVirtualAddress());
		commandList->SetGraphicsRootConstantBufferView(RootBinding::FogConstants,
			constantBuffers.GetOceanFogResource(frameIndex)->GetGPUVirtualAddress());
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->DrawInstanced(3, 1, 0, 0);
	}
} // namespace Engine
