#pragma once
#include "../EngineCommon.h"
#include "../Resources/Buffer/VertexBuffer.h"
#include "../Resources/Mesh/Model.h"
#include "../Resources/Mesh/SkySphere.h"
#include "../Renderer/Pipeline/GraphicsPipeline.h"
#include "../Resources/Texture/Texture.h"
#include "Camera.h"
#include <chrono>

class RenderDevice;
class CommandContext;

namespace Engine {
	class Window;
	class RenderDevice;
	class CommandContext;

	class Application {
	public:
		Application(HINSTANCE hInstance);
		virtual ~Application();

		void Initialize();
		int Run();

	protected:
		virtual void Update();
		virtual void Render();

	private:
		HINSTANCE m_hInstance;
		std::unique_ptr<Window> m_window;

		std::unique_ptr<RenderDevice> m_device;
		std::unique_ptr<CommandContext> m_context;

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

		// 定数バッファ（MVP および関連データ）
		Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
		UINT8* m_cbvDataPtr = nullptr;
		// Cloud 専用定数バッファ
		Microsoft::WRL::ComPtr<ID3D12Resource> m_cloudConstantBuffer;
		UINT8* m_cloudCbvDataPtr = nullptr;

		UINT m_vertexCount;

		// Camera
		std::unique_ptr<Engine::Camera> m_camera;
		// timing
		std::chrono::steady_clock::time_point m_lastTime;
		float m_fpsTimer = 0.0f;
		UINT m_fpsFrameCount = 0;
		
		// Sky Sphere
		std::unique_ptr<Engine::SkySphere> m_skySphere;
		std::unique_ptr<Engine::Model> m_model;
	};
}