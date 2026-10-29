#pragma once
#include "RendererAssets.h"
#include "RendererPipelines.h"
#include "RendererConstantBuffers.h"
#include "OceanGeometry.h"
#include "../RetroFilmParameters.h"

namespace Engine {
	class Scene;
	struct FrameContext {
		ID3D12GraphicsCommandList* commandList;
		RenderDevice* device;
		RendererPipelines& pipelines;
		RendererConstantBuffers& constantBuffers;
		UINT frameIndex;
	};

	class SkyRenderPass {
	  public:
		void Record(const FrameContext& frame, RendererAssets& assets) const;
	};

	class OceanRenderPass {
	  public:
		void Record(const FrameContext& frame, RendererAssets& assets, OceanGeometry& geometry) const;
	};

	class ModelRenderPass {
	  public:
		void Record(const FrameContext& frame, const Scene& scene) const;
	};

	class OceanFogRenderPass {
	  public:
		void Record(const FrameContext& frame) const;
	};

	class RetroFilmRenderPass {
	  public:
		void Record(const FrameContext& frame) const;
	};
} // namespace Engine
