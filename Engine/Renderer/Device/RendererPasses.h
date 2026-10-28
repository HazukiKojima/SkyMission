#pragma once
#include "RendererAssets.h"
#include "RendererPipelines.h"
#include "RendererConstantBuffers.h"
#include "OceanGeometry.h"
#include "../RetroFilmParameters.h"

namespace Engine {
	class Scene;

	class SkyRenderPass {
	public:
		void Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, RendererAssets& assets, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers) const;
	};

	class OceanRenderPass {
	public:
		void Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, RendererAssets& assets, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers, OceanGeometry& geometry) const;
	};

	class ModelRenderPass {
	public:
		void Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, CommandContext* context, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers, const Scene& scene, float propellerAngle) const;
	};

	class OceanFogRenderPass {
	public:
		void Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers) const;
	};

	class RetroFilmRenderPass {
	public:
		void Record(ID3D12GraphicsCommandList* commandList, RenderDevice* device, CommandContext* context, RendererPipelines& pipelines, RendererConstantBuffers& constantBuffers, const RetroFilmParameters& parameters, float elapsedTime) const;
	};
}
