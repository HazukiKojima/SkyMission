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
} // namespace Game
