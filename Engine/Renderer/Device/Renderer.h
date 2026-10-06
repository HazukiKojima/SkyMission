#pragma once
#include "../../EngineCommon.h"
#include "../../Resources/Buffer/VertexBuffer.h"
#include "../../Resources/Mesh/Model.h"
#include "../../Resources/Mesh/SkySphere.h"
#include "../Pipeline/GraphicsPipeline.h"
#include "../../Resources/Texture/Texture.h"

namespace Engine {
	class RenderDevice;
	class CommandContext;
	class Camera;

	class Renderer {
	public:
		Renderer() = default;
		~Renderer() = default;

		void Initialize(RenderDevice* device, CommandContext* context);
		void InitializeConstantBuffers(Camera* camera);
		void Update(Camera* camera, float elapsedTime);
		void Render(UINT width, UINT height);

	private:
		RenderDevice* m_device = nullptr;
		CommandContext* m_context = nullptr;

		std::unique_ptr<Engine::VertexBuffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
		UINT m_indexCount;
		std::unique_ptr<Engine::GraphicsPipeline> m_pipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_oceanPipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_skyPipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_cloudPipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_modelPipeline;

		std::unique_ptr<Engine::Texture> m_texture;
		UINT m_textureSrvIndex = 0;
		std::unique_ptr<Engine::Texture> m_oceanNormalTexture;
		UINT m_oceanNormalTextureSrvIndex = 0;

		std::unique_ptr<Engine::Texture> m_skyTexture;
		UINT m_skyTextureSrvIndex = 0;

		// 定数バッファ
		Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
		UINT8* m_cbvDataPtr = nullptr;
		// Cloud 専用定数バッファ
		Microsoft::WRL::ComPtr<ID3D12Resource> m_cloudConstantBuffer;
		UINT8* m_cloudCbvDataPtr = nullptr;

		UINT m_vertexCount;

		// Sky Sphere
		std::unique_ptr<Engine::SkySphere> m_skySphere;
		std::unique_ptr<Engine::Model> m_model;
	};
}