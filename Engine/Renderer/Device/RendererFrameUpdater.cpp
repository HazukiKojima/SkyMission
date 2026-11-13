#include "RendererFrameUpdater.h"
#include "OceanGeometry.h"
#include "CommandContext.h"
#include "../../Camera/Camera.h"
#include <cmath>

namespace Engine {

	void RendererFrameUpdater::Configure(const OceanSettings& settings, const CloudSettings& clouds) {
		ValidateCloudSettings(clouds);
		m_oceanSettings = settings;
		m_cloudSettings = clouds;
	}

	struct RendererFrameUpdater::FrameViewData {
		DirectX::XMMATRIX view, projection, inverseViewProjection;
		DirectX::XMFLOAT4X4 oceanMvp;
		DirectX::XMFLOAT3 cameraPosition;
		float time;
	};
	RendererFrameUpdater::FrameViewData RendererFrameUpdater::BuildFrameView(
		const Scene& scene, float elapsedTime) const {
		using namespace DirectX;

		constexpr float kProjectionMargin = 1.1f; // 海の四隅を遠クリップ面に収める余裕
		const Camera& camera = scene.GetCamera();

		const auto camPos = camera.GetPosition();
		const float cameraHeight = (camPos.y < 0.0f) ? -camPos.y : camPos.y;
		const float oceanHalfDiagonal = m_oceanSettings.size / std::sqrt(2.0f);
		const float oceanCornerDistance =
			std::sqrt(oceanHalfDiagonal * oceanHalfDiagonal + cameraHeight * cameraHeight);

		const XMMATRIX world = XMMatrixIdentity();
		const XMMATRIX view = camera.GetView();
		const XMMATRIX proj = camera.GetProjection(oceanCornerDistance * kProjectionMargin);
		const XMMATRIX mvp = world * view * proj;
		// 遠方へ移動しても逆行列の平行移動成分で視線の精度を失わない。
		XMMATRIX relativeView = view;
		relativeView.r[3] = XMVectorSet(0, 0, 0, 1);
		const XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, relativeView * proj);
		const XMMATRIX mvpT = XMMatrixTranspose(mvp);

		DirectX::XMFLOAT4X4 matrix;
		XMStoreFloat4x4(&matrix, mvpT);

		return {view, proj, inverseViewProjection, matrix, camPos, elapsedTime};
	}

	void RendererFrameUpdater::Update(const Scene& scene, float elapsedTime,
		const RetroFilmParameters& filmParameters, RendererConstantBuffers& buffers, ID3D12Device* device,
		CommandContext* context) {
		const UINT index = context->GetFrameIndex();
		buffers.EnsureModelCapacity(device, context, scene.GetObjects().size());
		const auto frame = BuildFrameView(scene, elapsedTime);
		m_drawDecisions =
			BuildModelDrawDecisions(scene, frame.view, frame.projection, context->GetRenderHeight());
		DirectX::XMStoreFloat4x4(&m_renderView.view, frame.view);
		DirectX::XMStoreFloat4x4(&m_renderView.projection, frame.projection);
		const auto& lighting = scene.GetEnvironment();
		UpdateOceanConstants(frame, lighting, buffers.GetOceanData(index));
		UpdateFogConstants(frame, lighting, buffers.GetOceanFogData(index));
		UpdateFilmConstants(frame, filmParameters, buffers.GetFilmData(index));
		UpdateModelConstants(frame, lighting, scene, buffers, index);
	}

	void RendererFrameUpdater::UpdateOceanConstants(const FrameViewData& frame,
		const SceneEnvironment& environment, OceanConstantBufferData* oceanData) const {
		oceanData->mvp = frame.oceanMvp;
		// うねり・法線・波模様を同じ時間倍率で動かす
		constexpr float kOceanAnimationSpeed = 2.0f;
		const float oceanTime = frame.time * kOceanAnimationSpeed;
		oceanData->time = oceanTime;
		oceanData->cameraPos = frame.cameraPosition;
		oceanData->oceanOffset[0] = frame.cameraPosition.x;
		oceanData->oceanOffset[1] = 0.0f;
		oceanData->oceanOffset[2] = frame.cameraPosition.z;
		oceanData->sunDirection = environment.sunDirection;
		oceanData->sunIntensity = environment.sunIntensity;
		oceanData->sunColor = environment.sunColor;
		oceanData->ambientIntensity = environment.ambientIntensity;
		oceanData->ambientColor = environment.ambientColor;
		oceanData->oceanSize = m_oceanSettings.size;
		oceanData->oceanWaveLength = m_oceanSettings.waveLength;
		oceanData->oceanWaveSteepness = m_oceanSettings.waveSteepness;
		oceanData->oceanNormalUvScale = m_oceanSettings.normalUvScale;
		oceanData->oceanWavePatternUvScale = m_oceanSettings.wavePatternUvScale;
		oceanData->oceanFogStartDistance = m_oceanSettings.fogStartDistance;
		oceanData->oceanFogEndDistance = m_oceanSettings.fogEndDistance;
		oceanData->oceanEdgeFadeStart = m_oceanSettings.edgeFadeStart;
		oceanData->oceanEdgeFadeWidth = m_oceanSettings.edgeFadeWidth;
		oceanData->oceanWaveHeightOffset = OceanGeometry::CalculateWaveHeight(frame.cameraPosition.x,
			frame.cameraPosition.z, oceanTime, m_oceanSettings.waveLength, m_oceanSettings.waveSteepness);
	}

	void RendererFrameUpdater::UpdateFogConstants(const FrameViewData& frame,
		const SceneEnvironment& environment, OceanFogConstantBufferData* oceanFog) const {
		using namespace DirectX;
		// CPUの行ベクトル用行列をHLSLへの転送時に転置する

		XMStoreFloat4x4(&oceanFog->inverseViewProjection, XMMatrixTranspose(frame.inverseViewProjection));
		oceanFog->cameraPosition = frame.cameraPosition;
		oceanFog->padding0 = 0.0f;
		oceanFog->sunDirection = environment.sunDirection;
		oceanFog->sunIntensity = environment.sunIntensity;
		oceanFog->sunColor = environment.sunColor;
		oceanFog->ambientIntensity = environment.ambientIntensity;
		oceanFog->ambientColor = environment.ambientColor;
		oceanFog->oceanFogBottom = 0.0f;
		oceanFog->oceanFogTop = m_oceanSettings.fogTop;
		oceanFog->oceanFogDensity = m_oceanSettings.fogDensity;
		oceanFog->oceanFogStartDistance = m_oceanSettings.fogStartDistance;
		oceanFog->oceanFogEndDistance = m_oceanSettings.fogEndDistance;
		oceanFog->oceanEdgeFadeStart = m_oceanSettings.edgeFadeStart;
		oceanFog->oceanEdgeFadeWidth = m_oceanSettings.edgeFadeWidth;
		oceanFog->padding1[0] = oceanFog->padding1[1] = 0.0f;
		const auto& clouds = m_cloudSettings;
		oceanFog->cloudLayer = {clouds.bottom, clouds.top, clouds.coverage, clouds.density};
		oceanFog->cloudMotion = {
			1.0f / clouds.scale, clouds.wind.x, clouds.wind.y, clouds.enabled ? 1.0f : 0.0f};
		oceanFog->cloudQuality = {clouds.maxDistance, float(clouds.steps), float(clouds.seed), frame.time};
	}

	void RendererFrameUpdater::UpdateFilmConstants(const FrameViewData& frame,
		const RetroFilmParameters& filmParameters, FilmConstantBufferData* film) const {
		film->time = std::fmod(frame.time, 3600.0f);
		film->padding[0] = film->padding[1] = film->padding[2] = 0.0f;
		film->parameters = filmParameters;
	}

	void RendererFrameUpdater::UpdateModelConstants(const FrameViewData& frame,
		const SceneEnvironment& environment, const Scene& scene, RendererConstantBuffers& constantBuffers,
		UINT frameIndex) const {
		using namespace DirectX;
		const auto& objects = scene.GetObjects();

		for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
			if (!m_drawDecisions[objectIndex].visible) continue;
			const SceneObject& object = objects[objectIndex];
			ModelConstantBufferData* modelData = constantBuffers.GetModelData(frameIndex, objectIndex);
			const XMMATRIX objectWorld = XMLoadFloat4x4(&object.worldTransform);
			XMStoreFloat4x4(&modelData->mvp, XMMatrixTranspose(objectWorld * frame.view * frame.projection));
			XMStoreFloat4x4(&modelData->world, XMMatrixTranspose(objectWorld));
			modelData->time = frame.time;
			modelData->padding[0] = modelData->padding[1] = modelData->padding[2] = 0.0f;
			modelData->cameraPos = frame.cameraPosition;
			modelData->pad2 = 0.0f;
			modelData->sunDirection = environment.sunDirection;
			modelData->sunIntensity = environment.sunIntensity;
			modelData->sunColor = environment.sunColor;
			modelData->ambientIntensity = environment.ambientIntensity;
			modelData->ambientColor = environment.ambientColor;
			modelData->pad3 = 0.0f;
		}
	}

} // namespace Engine
