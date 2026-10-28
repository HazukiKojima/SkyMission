#include "RendererFrameUpdater.h"
#include "OceanGeometry.h"
#include "CommandContext.h"
#include "../../Camera/Camera.h"
#include <cmath>

namespace Engine {

	void RendererFrameUpdater::Configure(const OceanSettings& settings) {
		m_oceanSettings = settings;
	}

	void RendererFrameUpdater::Update(const Scene& scene, float elapsedTime, const RetroFilmParameters& filmParameters,
									  RendererConstantBuffers& constantBuffers, ID3D12Device* device,
									  CommandContext* context) {
		const UINT frameIndex = context->GetFrameIndex();
		using namespace DirectX;
		const Camera& camera = scene.GetCamera();
		const SceneEnvironment& environment = scene.GetEnvironment();
		const auto camPos = camera.GetPosition();
		const float cameraHeight = (camPos.y < 0.0f) ? -camPos.y : camPos.y;
		const float oceanHalfDiagonal = m_oceanSettings.size / std::sqrt(2.0f);
		const float oceanCornerDistance =
			std::sqrt(oceanHalfDiagonal * oceanHalfDiagonal + cameraHeight * cameraHeight);

		const XMMATRIX world = XMMatrixIdentity();
		const XMMATRIX view = camera.GetView();
		const XMMATRIX proj = camera.GetProjection(oceanCornerDistance * 1.1f);
		const XMMATRIX mvp = world * view * proj;
		const XMMATRIX inverseViewProjection = XMMatrixInverse(nullptr, view * proj);
		const XMMATRIX mvpT = XMMatrixTranspose(mvp);

		DirectX::XMFLOAT4X4 matrix;
		XMStoreFloat4x4(&matrix, mvpT);

		OceanConstantBufferData* oceanData = constantBuffers.GetOceanData(frameIndex);
		oceanData->mvp = matrix;
		oceanData->time = elapsedTime;
		oceanData->cameraPos = camPos;
		oceanData->oceanOffset[0] = camPos.x;
		oceanData->oceanOffset[1] = 0.0f;
		oceanData->oceanOffset[2] = camPos.z;
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
		oceanData->oceanWaveHeightOffset = OceanGeometry::CalculateWaveHeight(
			camPos.x, camPos.z, elapsedTime, m_oceanSettings.waveLength, m_oceanSettings.waveSteepness);

		OceanFogConstantBufferData* oceanFog = constantBuffers.GetOceanFogData(frameIndex);
		XMStoreFloat4x4(&oceanFog->inverseViewProjection, XMMatrixTranspose(inverseViewProjection));
		oceanFog->cameraPosition = camPos;
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

		const auto& objects = scene.GetObjects();
		constantBuffers.EnsureModelCapacity(device, context, objects.size());

		FilmConstantBufferData* film = constantBuffers.GetFilmData(frameIndex);
		film->time = std::fmod(elapsedTime, 3600.0f);
		film->padding[0] = film->padding[1] = film->padding[2] = 0.0f;
		film->parameters = filmParameters;

		for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
			const SceneObject& object = objects[objectIndex];
			ModelConstantBufferData* modelData = constantBuffers.GetModelData(frameIndex, objectIndex);
			const XMMATRIX objectWorld = XMLoadFloat4x4(&object.worldTransform);
			XMStoreFloat4x4(&modelData->mvp, XMMatrixTranspose(objectWorld * view * proj));
			XMStoreFloat4x4(&modelData->world, XMMatrixTranspose(objectWorld));
			modelData->time = elapsedTime;
			modelData->padding[0] = modelData->padding[1] = modelData->padding[2] = 0.0f;
			modelData->cameraPos = camPos;
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
