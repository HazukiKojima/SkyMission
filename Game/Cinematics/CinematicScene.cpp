#include "CinematicScene.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Game {
	using namespace DirectX;
	void CinematicScene::Initialize(const CinematicShotSettings& settings,
		std::shared_ptr<Engine::Model> model, float visualScale, const Engine::SceneEnvironment& environment,
		const XMFLOAT3& origin, UINT width, UINT height, float farZ) {
		settings.Validate();
		if (!model || !model->GetLocalBounds() || width == 0 || height == 0 || !std::isfinite(visualScale) ||
			visualScale <= 0 || !std::isfinite(farZ) || farZ <= 1)
			throw std::invalid_argument(
				"Opening cinematic requires a bounded model and valid viewport/scale");
		m_settings = settings;
		m_scene = {};
		m_scene.SetEnvironment(environment);
		m_origin = origin;
		m_visualScale = visualScale * settings.modelScale;
		m_farZ = farZ;
		const auto& bounds = *model->GetLocalBounds();
		m_modelCenter = bounds.Center();
		// 既定構図は幅約200mのゲーム内機体を基準とし、モデル変更時も距離を追従させる
		constexpr float kReferenceAircraftExtentMeters = 200;
		m_framingScale = std::max({bounds.maximum.x - bounds.minimum.x, bounds.maximum.y - bounds.minimum.y,
							 bounds.maximum.z - bounds.minimum.z}) *
			m_visualScale / kReferenceAircraftExtentMeters;
		m_framingScale = std::max(m_framingScale, 0.01f);
		m_aircraft.resize(settings.formationOffsets.size());
		for (auto& id : m_aircraft)
			id = m_scene.AddObject(model);
		OnResize(width, height);
	}
	void CinematicScene::Update(const CinematicPose& pose) {
		XMFLOAT3 lead = m_origin;
		const float heading = XMConvertToRadians(m_settings.headingDegrees);
		if (m_settings.headingDegrees == 180)
			lead.z += pose.travelMeters;
		else {
			lead.x -= std::sin(heading) * pose.travelMeters;
			lead.z -= std::cos(heading) * pose.travelMeters;
		}
		for (size_t index = 0; index < m_aircraft.size(); ++index) {
			const auto& offset = m_settings.formationOffsets[index];
			const float x = lead.x + offset.x * m_settings.formationSpacingMeters * m_framingScale;
			const float y = lead.y + offset.y * m_settings.formationSpacingMeters * m_framingScale;
			const float z = lead.z + offset.z * m_settings.formationDepthMeters * m_framingScale;
			auto* aircraft = m_scene.FindObject(m_aircraft[index]);
			XMStoreFloat4x4(&aircraft->worldTransform,
				XMMatrixTranslation(-m_modelCenter.x, -m_modelCenter.y, -m_modelCenter.z) *
					XMMatrixScaling(m_visualScale, m_visualScale, m_visualScale) *
					XMMatrixRotationY(heading) * XMMatrixTranslation(x, y, z));
			aircraft->nodeRotationRadians = pose.propellerRadians;
		}
		XMFLOAT3 eye;
		XMFLOAT3 target;
		XMStoreFloat3(&eye, XMLoadFloat3(&lead) + XMLoadFloat3(&pose.cameraOffsetMeters) * m_framingScale);
		XMStoreFloat3(
			&target, XMLoadFloat3(&lead) + XMLoadFloat3(&m_settings.lookAtOffsetMeters) * m_framingScale);
		m_camera.LookAt(eye, target);
		m_scene.SetCamera(m_camera);
	}
	void CinematicScene::OnResize(UINT width, UINT height) {
		if (width == 0 || height == 0) return;
		// 4:3などの狭い画面では縦視野角を広げ、左右の僚機が画面外へ切れないようにする
		constexpr float kReferenceAspect = 16.0f / 9;
		const float aspect = float(width) / height;
		const float halfFov = XMConvertToRadians(m_settings.verticalFovDegrees) * 0.5f;
		const float verticalFov =
			2 * std::atan(std::tan(halfFov) * std::max(1.0f, kReferenceAspect / aspect));
		m_camera.Initialize(verticalFov, aspect, 1, m_farZ);
		m_scene.SetCamera(m_camera);
	}
} // namespace Game
