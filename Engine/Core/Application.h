#pragma once
#include "../EngineCommon.h"
#include "../Resources/Buffer/VertexBuffer.h"
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

	// アプリケーション本体。ウィンドウ、デバイス、レンダリングループを管理する。
	class Application {
	public:
		Application(HINSTANCE hInstance);
		virtual ~Application();

		// 初期化とメインループ
		void Initialize();
		int Run();

	protected:
		virtual void Update();
		virtual void Render();

	private:
		HINSTANCE m_hInstance;
		// メインウィンドウ
		std::unique_ptr<Window> m_window;

		// デバイスとコマンドコンテキスト
		std::unique_ptr<RenderDevice> m_device;
		std::unique_ptr<CommandContext> m_context;

		// 海面頂点バッファ
		std::unique_ptr<Engine::VertexBuffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
		D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
		UINT m_indexCount;

		// 描画パイプライン
		std::unique_ptr<Engine::GraphicsPipeline> m_pipeline;
		std::unique_ptr<Engine::GraphicsPipeline> m_skyPipeline;

		// テクスチャとSRVインデックス
		std::unique_ptr<Engine::Texture> m_texture;
		UINT m_textureSrvIndex = 0;
		std::unique_ptr<Engine::Texture> m_oceanNormalTexture;
		UINT m_oceanNormalTextureSrvIndex = 0;

		std::unique_ptr<Engine::Texture> m_skyTexture;
		UINT m_skyTextureSrvIndex = 0;

		// 定数バッファ（MVP 等）
		Microsoft::WRL::ComPtr<ID3D12Resource> m_constantBuffer;
		UINT8* m_cbvDataPtr = nullptr;

		UINT m_vertexCount = 0;

		// カメラ
		std::unique_ptr<Engine::Camera> m_camera;
		// タイミング
		std::chrono::steady_clock::time_point m_lastTime;

		// 空球メッシュ
		std::unique_ptr<Engine::SkySphere> m_skySphere;
	};
}