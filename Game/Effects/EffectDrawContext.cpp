#include "EffectDrawContext.h"
#include <algorithm>
#include <cmath>

namespace Game {
	EffectRandom::EffectRandom(const DirectX::XMFLOAT3& position)
		: m_seed(position.x * 0.73f + position.y * 1.37f + position.z * 0.19f) {}

	float EffectRandom::Sample(int index) const {
		// 毎フレーム同じ乱数を使い、火花の方向や煙の模様のちらつきを防ぐ
		const float value = std::sin(m_seed + index * 127.1f) * 43758.5453f;
		return value - std::floor(value);
	}

	EffectDrawContext::EffectDrawContext(const Engine::RenderView& view, Engine::HudDrawList& list)
		: m_view(view), m_list(list), m_projector(view) {
		const auto cameraWorld = DirectX::XMMatrixInverse(nullptr, DirectX::XMLoadFloat4x4(&view.view));
		DirectX::XMStoreFloat3(&m_cameraRight, cameraWorld.r[0]);
	}

	bool EffectDrawContext::HasVertexBudget(size_t requiredVertices) const {
		return m_list.GetVertices().size() + requiredVertices <=
			Engine::HudDrawList::MaxVertices - EffectDrawSettings::hudReservedVertices;
	}

	float EffectDrawContext::GetScale() const {
		return std::min(float(m_view.width) / EffectDrawSettings::referenceWidth,
			float(m_view.height) / EffectDrawSettings::referenceHeight);
	}

	std::optional<EffectDrawContext::ProjectedBillboard> EffectDrawContext::ProjectBillboard(
		DirectX::FXMVECTOR position, float radius, EffectClipMode clipMode) const {
		using namespace DirectX;
		XMFLOAT3 center;
		XMFLOAT3 edge;
		XMStoreFloat3(&center, position);
		XMStoreFloat3(&edge, position + XMLoadFloat3(&m_cameraRight) * radius);
		const auto projectedCenter = m_projector.Project(center);
		const auto projectedEdge = m_projector.Project(edge);
		if (projectedCenter.behind || projectedEdge.behind)
			return std::nullopt;
		const float pixels = std::hypot(
			projectedEdge.pixel.x - projectedCenter.pixel.x, projectedEdge.pixel.y - projectedCenter.pixel.y);
		const auto point = projectedCenter.pixel;
		if (clipMode == EffectClipMode::CenterVisible) {
			if (!projectedCenter.visible)
				return std::nullopt;
			return ProjectedBillboard{point, std::min(pixels, float(m_view.height))};
		}
		// 爆発の中心が画面外でも、火球や煙の一部が見える間は描画する
		if (point.x + pixels < 0 || point.y + pixels < 0 || point.x - pixels > m_view.width ||
			point.y - pixels > m_view.height)
			return std::nullopt;
		return ProjectedBillboard{point, std::min(pixels, float(std::max(m_view.width, m_view.height)) * 2)};
	}

	void EffectDrawContext::DrawCloud(DirectX::FXMVECTOR position, float radius, float rotation,
		unsigned seed, DirectX::XMFLOAT4 color, EffectClipMode clipMode) {
		if (const auto billboard = ProjectBillboard(position, radius, clipMode))
			m_list.CloudPuff(billboard->center, billboard->radius, rotation, seed, color);
	}

	void EffectDrawContext::DrawGlow(
		DirectX::FXMVECTOR position, float radius, DirectX::XMFLOAT4 color, EffectClipMode clipMode) {
		if (const auto billboard = ProjectBillboard(position, radius, clipMode))
			m_list.SoftDisc(billboard->center, billboard->radius, color);
	}

	std::optional<ProjectedEffectSegment> EffectDrawContext::ProjectSegment(
		DirectX::FXMVECTOR head, DirectX::FXMVECTOR tail) const {
		DirectX::XMFLOAT3 headPosition;
		DirectX::XMFLOAT3 tailPosition;
		DirectX::XMStoreFloat3(&headPosition, head);
		DirectX::XMStoreFloat3(&tailPosition, tail);
		const auto headProjection = m_projector.Project(headPosition);
		const auto tailProjection = m_projector.Project(tailPosition);
		if (!headProjection.visible || !tailProjection.visible)
			return std::nullopt;
		return ProjectedEffectSegment{headProjection.pixel, tailProjection.pixel};
	}

	void EffectDrawContext::DrawStreak(const ProjectedEffectSegment& segment, float outerWidth,
		DirectX::XMFLOAT4 outerColor, float innerWidth, DirectX::XMFLOAT4 innerColor) {
		m_list.Line(segment.head, segment.tail, outerWidth, outerColor);
		m_list.Line(segment.head, segment.tail, innerWidth, innerColor);
	}
} // namespace Game
