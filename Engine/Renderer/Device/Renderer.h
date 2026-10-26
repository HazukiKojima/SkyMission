#include "../../EngineCommon.h"
#include "../../Resources/Buffer/VertexBuffer.h"
#include "../../Resources/Mesh/Model.h"
#include "../../Resources/Mesh/SkySphere.h"
#include "../Pipeline/GraphicsPipeline.h"
#include "../../Resources/Texture/Texture.h"
#include "../RetroFilmParameters.h"
#include "../Device/RenderDevice.h"
#include "../Device/CommandContext.h"
#include <vector>

namespace Engine {
	class Scene;
	struct SceneAssetPaths;

	class Renderer {
	public:
		Renderer() = default;
		~Renderer();

		void Initialize(HWND hwnd, UINT width, UINT height, const SceneAssetPaths& assets);
		void InitializeConstantBuffers(const Scene& scene);
		void Update(const Scene& scene, float elapsedTime, float deltaSeconds);
		void Render(UINT width, UINT height, const Scene& scene);
		void Resize(UINT width, UINT height);
		float GetOceanSize() const { return m_oceanSize; }
		std::shared_ptr<Engine::Model> GetModel() const { return m_model; }

	private:
		struct ModelConstantBuffer {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			UINT8* mappedData = nullptr;
		};

		struct FilmConstantBuffer {
			Microsoft::WRL::ComPtr<ID3D12Resource> resource;
			UINT8* mappedData = nullptr;
		};

		void EnsureModelConstantBufferCapacity(size_t objectCount);

		std::unique_ptr<RenderDevice> m_device;
		std::unique_ptr<CommandContext> m_context;

		std::unique_ptr<Engine::VertexBuffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
		UINT m_indexCount;
		std::unique_ptr<Engine::GraphicsPipeline> m_pipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_oceanPipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_skyPipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_oceanFogPipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_retroFilmPipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_modelPipeline;

		std::shared_ptr<Engine::Texture> m_texture;
		UINT m_textureSrvIndex = 0;
		std::shared_ptr<Engine::Texture> m_oceanNormalTexture;
		UINT m_oceanNormalTextureSrvIndex = 0;
		std::shared_ptr<Engine::Texture> m_oceanNormalTexture2;
		UINT m_oceanNormalTexture2SrvIndex = 0;

		std::shared_ptr<Engine::Texture> m_skyTexture;
		UINT m_skyTextureSrvIndex = 0;

		// 定数バッファ
		Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
		UINT8* m_cbvDataPtr = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_oceanFogConstantBuffer;
		UINT8* m_oceanFogCbvDataPtr = nullptr;
		FilmConstantBuffer m_filmConstantBuffers[RenderDevice::FrameCount];
		std::vector<ModelConstantBuffer> m_modelConstantBuffers[RenderDevice::FrameCount];

		UINT m_vertexCount;
		float m_oceanSize = 10000000.0f;
		float m_oceanWaveLength = 40000.0f;
		float m_oceanWaveSteepness = 0.0005f;
		float m_oceanNormalUvScale = 0.04f;
		float m_oceanWavePatternUvScale = 0.04f;
		float m_oceanFogStartDistance = 6500000.0f;
		float m_oceanFogEndDistance = 10000000.0f;
		float m_oceanEdgeFadeStart = 0.98f;
		float m_oceanEdgeFadeWidth = 0.015f;
		float m_propellerAngle = 0.0f;
		float m_elapsedTime = 0.0f;
		RetroFilmParameters m_retroFilmParameters{};

		// Sky Sphere
		std::unique_ptr<Engine::SkySphere> m_skySphere;
		std::shared_ptr<Engine::Model> m_model;
	};
}