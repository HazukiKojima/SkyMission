#include "HudProjection.h"
#include <algorithm>
#include <cmath>

namespace Game {
	HudProjector::HudProjector(const Engine::RenderView& view) : m_width(view.width), m_height(view.height) {
		// 1フレーム中で共有するビュー・射影行列を一度だけ合成する
		DirectX::XMStoreFloat4x4(&m_viewProjection,
			DirectX::XMLoadFloat4x4(&view.view) * DirectX::XMLoadFloat4x4(&view.projection));
	}

	HudProjection HudProjector::Project(const DirectX::XMFLOAT3& point) const {
		using namespace DirectX;
		const auto clip =
			XMVector4Transform(XMVectorSet(point.x, point.y, point.z, 1), XMLoadFloat4x4(&m_viewProjection));
		const float w = XMVectorGetW(clip);
		const float divisor = std::max(std::abs(w), 1e-5f);
		const float x = XMVectorGetX(clip) / divisor;
		const float y = XMVectorGetY(clip) / divisor;
		const float z = XMVectorGetZ(clip) / divisor;
		return {{(x + 1) * m_width * 0.5f, (1 - y) * m_height * 0.5f},
			w > 0 && std::abs(x) <= 1 && std::abs(y) <= 1 && z >= 0 && z <= 1, w <= 0};
	}

	HudProjection ProjectHudPoint(const DirectX::XMFLOAT3& point, const Engine::RenderView& view) {
		return HudProjector(view).Project(point);
	}

	std::optional<HudSegmentProjection> HudProjector::ProjectClippedSegment(
		const DirectX::XMFLOAT3& head, const DirectX::XMFLOAT3& tail) const {
		using namespace DirectX;
		if (m_width == 0 || m_height == 0)
			return std::nullopt;
		const auto matrix = XMLoadFloat4x4(&m_viewProjection);
		const auto headClip = XMVector3Transform(XMLoadFloat3(&head), matrix);
		const auto tailClip = XMVector3Transform(XMLoadFloat3(&tail), matrix);
		XMFLOAT4 a;
		XMFLOAT4 b;
		XMStoreFloat4(&a, headClip);
		XMStoreFloat4(&b, tailClip);
		// 透視除算の前に6面と正のwで切り、背面から伸びる巨大な線を防ぐ
		const float headDistances[] = {
			a.w + a.x, a.w - a.x, a.w + a.y, a.w - a.y, a.z, a.w - a.z, a.w - 1e-5f};
		const float tailDistances[] = {
			b.w + b.x, b.w - b.x, b.w + b.y, b.w - b.y, b.z, b.w - b.z, b.w - 1e-5f};
		float enter = 0;
		float exit = 1;
		for (size_t plane = 0; plane < std::size(headDistances); ++plane) {
			const float from = headDistances[plane];
			const float to = tailDistances[plane];
			if (!std::isfinite(from) || !std::isfinite(to) || (from < 0 && to < 0))
				return std::nullopt;
			if ((from < 0) != (to < 0)) {
				const float fraction = from / (from - to);
				if (from < 0)
					enter = std::max(enter, fraction);
				else
					exit = std::min(exit, fraction);
			}
		}
		if (enter > exit)
			return std::nullopt;
		const auto toPixel = [&](float fraction) {
			const auto clip = XMVectorLerp(headClip, tailClip, fraction);
			const float w = std::max(XMVectorGetW(clip), 1e-5f);
			return XMFLOAT2{std::clamp((XMVectorGetX(clip) / w + 1) * m_width * 0.5f, 0.0f, float(m_width)),
				std::clamp((1 - XMVectorGetY(clip) / w) * m_height * 0.5f, 0.0f, float(m_height))};
		};
		return HudSegmentProjection{toPixel(enter), toPixel(exit)};
	}
} // namespace Game
