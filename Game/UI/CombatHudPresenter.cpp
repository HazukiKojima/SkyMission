#include "CombatHudPresenter.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;
	void CombatHudPresenter::Build(const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		if (view.width == 0 || view.height == 0) return;
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float line = std::max(1.0f, m_settings.lineWidth * scale), font = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		const HudProjector projector(view);
		const float ox = (view.width - 1920 * scale) * 0.5f, oy = (view.height - 1080 * scale) * 0.5f;
		const auto label = [&](float x, float y, std::string_view text) { list.Text({ ox + x * scale,oy + y * scale }, text, font, color); };
		// The boresight follows the gun axis independently of the predicted bullet path.
		XMFLOAT3 sight;
		XMStoreFloat3(&sight, XMLoadFloat3(&snapshot.muzzle) + XMLoadFloat3(&snapshot.forward) * CombatSnapshot::gunSightDistance);
		const auto cross = projector.Project(sight);
		if (cross.visible) {
			list.Line({ cross.pixel.x - 12 * scale,cross.pixel.y }, { cross.pixel.x + 12 * scale,cross.pixel.y }, line, color);
			list.Line({ cross.pixel.x,cross.pixel.y - 12 * scale }, { cross.pixel.x,cross.pixel.y + 12 * scale }, line, color);
		}
		// Lead marker: aim the boresight at this point to intercept the selected target.
		// aimPoint includes target motion and compensates for inherited aircraft velocity.
		const auto circlePoint = snapshot.lockState == LockState::Locked &&
			snapshot.selected != Engine::InvalidObjectId && snapshot.aim
			? std::optional<XMFLOAT3>{snapshot.aim->aimPoint} : std::nullopt;
		if (circlePoint) {
			const auto impact = projector.Project(*circlePoint);
			if (impact.visible) {
				const auto p = impact.pixel;
				const XMFLOAT4 sightColor{ 0.65f,1.0f,0.65f,color.w };
				list.Circle(p, 34 * scale, 1, line, sightColor);
				list.Circle(p, 25 * scale, 1, line, sightColor);
				list.Circle(p, 5 * scale, 1, line, sightColor);
				for (int i = 0; i < 12; ++i) {
					const float angle = XM_2PI * i / 12;
					const float x = std::cos(angle), y = std::sin(angle);
					list.Line({ p.x + x * 31 * scale,p.y + y * 31 * scale },
						{ p.x + x * (i % 3 == 0 ? 40 : 37) * scale,p.y + y * (i % 3 == 0 ? 40 : 37) * scale }, line, sightColor);
				}
				for (const XMFLOAT2 axis : {XMFLOAT2{ 1,0 }, XMFLOAT2{ -1,0 }, XMFLOAT2{ 0,1 }, XMFLOAT2{ 0,-1 }})
					list.Line({ p.x + axis.x * 10 * scale,p.y + axis.y * 10 * scale },
						{ p.x + axis.x * 23 * scale,p.y + axis.y * 23 * scale }, line, sightColor);
			}
		}
		for (const auto& enemy : snapshot.enemies) {
			const auto projected = projector.Project(enemy.center);
			const bool selected = enemy.id == snapshot.selected;
			if (projected.visible) {
				const auto p = projected.pixel;
				const float radius = (selected ? 32.0f : 22.0f) * scale, corner = radius * 0.4f;
				for (float x : {-1.0f, 1.0f}) for (float y : {-1.0f, 1.0f}) {
					const XMFLOAT2 a{ p.x + x * radius,p.y + y * radius };
					list.Line(a, { a.x - x * corner,a.y }, line, color); list.Line(a, { a.x,a.y - y * corner }, line, color);
				}
				if (selected) {
					const auto distance = int(XMVectorGetX(XMVector3Length(XMLoadFloat3(&enemy.center) - XMLoadFloat3(&snapshot.muzzle))));
					list.Text({ std::clamp(p.x + radius + 8 * scale,0.0f,std::max(0.0f,view.width - 260 * scale)),
						std::clamp(p.y + radius + 8 * scale,0.0f,std::max(0.0f,view.height - 2 * font)) }, enemy.name + "  " + std::to_string(distance), font, color);
					if (snapshot.lockState == LockState::Acquiring) list.Circle(p, radius + 8 * scale, snapshot.acquireProgress, line, color);
					if (snapshot.lockState == LockState::Locked) list.Text({ p.x - radius,std::max(0.0f,p.y - radius - font - 4 * scale) }, "LOCK", font, color);
				}
			}
			else if (selected) {
				const float cx = view.width * 0.5f, cy = view.height * 0.5f;
				float dx = projected.pixel.x - cx, dy = projected.pixel.y - cy;
				if (std::hypot(dx, dy) < 0.001f) dy = 1;
				const float ratio = 1 / std::max(std::abs(dx) / std::max(1.0f, cx - 50 * scale), std::abs(dy) / std::max(1.0f, cy - 50 * scale));
				const XMFLOAT2 tip{ cx + dx * ratio,cy + dy * ratio };
				const float length = std::hypot(dx, dy), ux = dx / length, uy = dy / length;
				list.Line(tip, { tip.x - ux * 20 * scale - uy * 9 * scale,tip.y - uy * 20 * scale + ux * 9 * scale }, line, color);
				list.Line(tip, { tip.x - ux * 20 * scale + uy * 9 * scale,tip.y - uy * 20 * scale - ux * 9 * scale }, line, color);
			}

		}
		if (snapshot.selected == Engine::InvalidObjectId) label(80, 80, "NO TARGET");
		else if (!snapshot.aim) label(80, 80, "OUT OF RANGE");
		if (snapshot.destroyedSeconds > 0) label(800, 720, "DESTROYED");
		else if (snapshot.hitSeconds > 0) label(920, 640, "HIT");
		if (snapshot.allDownSeconds > 0) label(760, 180, "ALL TARGETS DOWN");
	}
}
