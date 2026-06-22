#pragma once
#include "../EngineCommon.h"
#include "../Resources/Buffer/VertexBuffer.h"
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

		std::unique_ptr<Engine::Texture> m_texture;
		UINT m_textureSrvIndex = 0;

		// 頂点シェーダ用の定数バッファ (MVP 行列)
		Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
		UINT8* m_cbvDataPtr = nullptr;

		UINT m_vertexCount;

		// Camera
		std::unique_ptr<Engine::Camera> m_camera;
		// timing
		std::chrono::steady_clock::time_point m_lastTime;
	};
}