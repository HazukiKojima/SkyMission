#include "CombatHudPresenter.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Game {
	using namespace DirectX;
	void CombatHudPresenter::Build(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		if (view.width == 0 || view.height == 0)
			return;
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const float fontSize = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		const HudProjector projector(view);
		const float offsetX = (view.width - 1920 * scale) * 0.5f;
		const float offsetY = (view.height - 1080 * scale) * 0.5f;
		const auto drawLabel = [&](float x, float y, std::string_view text) {
			list.Text({offsetX + x * scale, offsetY + y * scale}, text, fontSize, color);
		};
		// 敵機とミサイルで同じ画面端配置・記号を共有する。
		const auto drawOffscreenIndicator = [&](const HudProjection& projection, XMFLOAT4 tint) {
			if (projection.visible || !std::isfinite(projection.pixel.x) ||
				!std::isfinite(projection.pixel.y))
				return;
			const float centerX = view.width * 0.5f;
			const float centerY = view.height * 0.5f;
			float deltaX = projection.pixel.x - centerX;
			float deltaY = projection.pixel.y - centerY;
			if (std::hypot(deltaX, deltaY) < 0.001f)
				deltaY = 1;
			const float ratio = 1 /
				std::max(std::abs(deltaX) / std::max(1.0f, centerX - 50 * scale),
					std::abs(deltaY) / std::max(1.0f, centerY - 50 * scale));
			const XMFLOAT2 tip{centerX + deltaX * ratio, centerY + deltaY * ratio};
			const float length = std::hypot(deltaX, deltaY);
			const float unitX = deltaX / length;
			const float unitY = deltaY / length;
			list.Line(tip,
				{tip.x - unitX * 20 * scale - unitY * 9 * scale,
					tip.y - unitY * 20 * scale + unitX * 9 * scale},
				lineWidth, tint);
			list.Line(tip,
				{tip.x - unitX * 20 * scale + unitY * 9 * scale,
					tip.y - unitY * 20 * scale - unitX * 9 * scale},
				lineWidth, tint);
		};
		drawLabel(80, 820,
			"HULL " + std::to_string(static_cast<int>(snapshot.playerHealth)) + "/" +
				std::to_string(static_cast<int>(snapshot.playerMaxHealth)));
		if (snapshot.result != CombatResult::Active) {
			drawLabel(740, 380, snapshot.result == CombatResult::Victory ? "VICTORY" : "SHOT DOWN");
			drawLabel(740, 440, "R: RETRY");
		}
		if (snapshot.playerHitSeconds > 0) {
			const auto& d = snapshot.playerHitDirection;
			const auto& f = snapshot.forward;
			const float bearing = std::atan2(-f.z * d.x + f.x * d.z, f.x * d.x + f.z * d.z);
			const float x = std::sin(bearing), y = -std::cos(bearing);
			const XMFLOAT2 center{view.width * 0.5f, view.height * 0.5f};
			const XMFLOAT4 warning{1, 0.2f, 0.15f, 1};
			const XMFLOAT2 tip{center.x + x * 160 * scale, center.y + y * 160 * scale};
			list.Line(tip, {tip.x - x * 25 * scale - y * 12 * scale, tip.y - y * 25 * scale + x * 12 * scale},
				lineWidth * 2, warning);
			list.Line(tip, {tip.x - x * 25 * scale + y * 12 * scale, tip.y - y * 25 * scale - x * 12 * scale},
				lineWidth * 2, warning);
			drawLabel(820, 800, "TAKING FIRE");
		}
		if (snapshot.threatWarning != ThreatWarning::None) {
			const bool missile = snapshot.threatWarning == ThreatWarning::Missile;
			const XMFLOAT4 warning = missile ? XMFLOAT4{1, 0.15f, 0.12f, 1} : XMFLOAT4{1, 0.65f, 0.15f, 1};
			const std::string label = missile
				? "MISSILE ALERT  " + std::to_string(snapshot.incomingMissiles.size())
				: "LOCK WARNING  " + std::to_string(snapshot.lockingEnemyCount);
			list.Text({offsetX + 650 * scale, offsetY + 160 * scale}, label, fontSize * 1.35f, warning);
			if (!missile) {
				list.Circle({view.width * 0.5f, offsetY + 240 * scale}, 20 * scale,
					snapshot.enemyLockProgress, lineWidth * 2, warning);
			}
		}
		// 照準は弾道予測とは独立し、機関砲の軸に追従する
		XMFLOAT3 sightWorldPosition;
		XMStoreFloat3(&sightWorldPosition,
			XMLoadFloat3(&snapshot.muzzle) +
				XMLoadFloat3(&snapshot.forward) * CombatSnapshot::gunSightDistance);
		const auto sightProjection = projector.Project(sightWorldPosition);
		if (sightProjection.visible) {
			list.Line({sightProjection.pixel.x - 12 * scale, sightProjection.pixel.y},
				{sightProjection.pixel.x + 12 * scale, sightProjection.pixel.y}, lineWidth, color);
			list.Line({sightProjection.pixel.x, sightProjection.pixel.y - 12 * scale},
				{sightProjection.pixel.x, sightProjection.pixel.y + 12 * scale}, lineWidth, color);
		}
		// 選択した敵に命中するよう、照準を予測位置へ合わせる
		// 予測位置には敵の移動と、自機から引き継ぐ弾速の補正を含む
		const auto leadPoint = snapshot.lockState == LockState::Locked &&
				snapshot.selected != Engine::InvalidObjectId && snapshot.aim
			? std::optional<XMFLOAT3>{snapshot.aim->aimPoint}
			: std::nullopt;
		if (leadPoint) {
			const auto impact = projector.Project(*leadPoint);
			if (impact.visible) {
				const auto centerPixel = impact.pixel;
				const XMFLOAT4 sightColor{0.65f, 1.0f, 0.65f, color.w};
				list.Circle(centerPixel, 34 * scale, 1, lineWidth, sightColor);
				list.Circle(centerPixel, 25 * scale, 1, lineWidth, sightColor);
				list.Circle(centerPixel, 5 * scale, 1, lineWidth, sightColor);
				for (int i = 0; i < 12; ++i) {
					const float angle = XM_2PI * i / 12;
					const float x = std::cos(angle);
					const float y = std::sin(angle);
					list.Line({centerPixel.x + x * 31 * scale, centerPixel.y + y * 31 * scale},
						{centerPixel.x + x * (i % 3 == 0 ? 40 : 37) * scale,
							centerPixel.y + y * (i % 3 == 0 ? 40 : 37) * scale},
						lineWidth, sightColor);
				}
				for (const XMFLOAT2 axis : {XMFLOAT2{1, 0}, XMFLOAT2{-1, 0}, XMFLOAT2{0, 1}, XMFLOAT2{0, -1}})
					list.Line({centerPixel.x + axis.x * 10 * scale, centerPixel.y + axis.y * 10 * scale},
						{centerPixel.x + axis.x * 23 * scale, centerPixel.y + axis.y * 23 * scale}, lineWidth,
						sightColor);
			}
		}
		for (const auto& enemy : snapshot.enemies) {
			const auto projected = projector.Project(enemy.center);
			const bool selected = enemy.id == snapshot.selected;
			if (projected.visible) {
				const auto centerPixel = projected.pixel;
				const float radius = (selected ? 32.0f : 22.0f) * scale;
				const float corner = radius * 0.4f;
				for (float x : {-1.0f, 1.0f})
					for (float y : {-1.0f, 1.0f}) {
						const XMFLOAT2 a{centerPixel.x + x * radius, centerPixel.y + y * radius};
						list.Line(a, {a.x - x * corner, a.y}, lineWidth, color);
						list.Line(a, {a.x, a.y - y * corner}, lineWidth, color);
					}
				if (selected) {
					const auto distance = int(XMVectorGetX(
						XMVector3Length(XMLoadFloat3(&enemy.center) - XMLoadFloat3(&snapshot.muzzle))));
					list.Text({std::clamp(centerPixel.x + radius + 8 * scale, 0.0f,
								   std::max(0.0f, view.width - 260 * scale)),
								  std::clamp(centerPixel.y + radius + 8 * scale, 0.0f,
									  std::max(0.0f, view.height - 2 * fontSize))},
						enemy.name + "  " + std::to_string(distance), fontSize, color);
					if (snapshot.lockState == LockState::Acquiring)
						list.Circle(
							centerPixel, radius + 8 * scale, snapshot.acquireProgress, lineWidth, color);
					if (snapshot.lockState == LockState::Locked)
						list.Text({centerPixel.x - radius,
									  std::max(0.0f, centerPixel.y - radius - fontSize - 4 * scale)},
							"LOCK", fontSize, color);
				}
			} else if (selected) {
				drawOffscreenIndicator(projected, color);
			}
		}
		if (snapshot.selected == Engine::InvalidObjectId)
			drawLabel(80, 80, "NO TARGET");
		else if (!snapshot.aim)
			drawLabel(80, 80, "OUT OF RANGE");
		if (snapshot.destroyedSeconds > 0)
			drawLabel(800, 720, "DESTROYED");
		else if (snapshot.hitSeconds > 0)
			drawLabel(920, 640, "HIT");
		if (snapshot.rocket.configured) {
			for (size_t port = 0; port < 2; ++port) {
				std::ostringstream text;
				text << (snapshot.rocket.nextMuzzle == port ? "> " : "  ") << "RKT "
					 << (port == 0 ? "L1 " : "R1 ");
				if (snapshot.rocket.cooldowns[port] <= 1e-6f)
					text << "READY";
				else
					text << std::fixed << std::setprecision(1)
						 << std::ceil(snapshot.rocket.cooldowns[port] * 10) / 10 << "s";
				drawLabel(80, 900 + float(port) * 48, text.str());
			}
		}
		if (snapshot.allDownSeconds > 0)
			drawLabel(760, 180, "ALL TARGETS DOWN");
		// ロック中の敵と飛来ミサイルは、同じ画面端配置を色で区別する。
		for (const auto& position : snapshot.lockingEnemyPositions)
			drawOffscreenIndicator(projector.Project(position), {1, 0.65f, 0.15f, 1});
		if (snapshot.threatWarning == ThreatWarning::Missile)
			for (const auto& threat : snapshot.incomingMissiles) {
				const auto projected = projector.Project(threat.position);
				if (projected.visible)
					list.Circle(
						projected.pixel, std::max(10.0f, 22 * scale), 1, lineWidth * 2, {1, 0.15f, 0.12f, 1});
				else
					drawOffscreenIndicator(projected, {1, 0.15f, 0.12f, 1});
			}
	}
} // namespace Game
