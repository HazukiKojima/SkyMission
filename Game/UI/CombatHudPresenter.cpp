#include "CombatHudPresenter.h"
#include "RadarMapPresenter.h"
#include "HudStyle.h"
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
		HudStyle::DrawFrame(view, list, color);
		RadarMapPresenter{}.Build(snapshot, view, list);
		const auto radar = RadarMapPresenter::Layout(view);
		const auto drawStatus = [&](float y, std::string_view text, float size) {
			list.Text({radar.statusOrigin.x, radar.statusOrigin.y + y * scale}, text, size * scale, color);
		};
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
		const auto drawFlightValue = [&](float x, std::string_view label, float value) {
			constexpr float top = 370, width = 240, height = 60;
			const XMFLOAT2 corners[] = {{offsetX + x * scale, offsetY + top * scale},
				{offsetX + (x + width) * scale, offsetY + top * scale},
				{offsetX + (x + width) * scale, offsetY + (top + height) * scale},
				{offsetX + x * scale, offsetY + (top + height) * scale}};
			for (size_t i = 0; i < std::size(corners); ++i)
				list.Line(corners[i], corners[(i + 1) % std::size(corners)], lineWidth, color);
			list.Text(
				{offsetX + (x + 8) * scale, offsetY + (top - 60) * scale}, label, fontSize * 1.1f, color);
			const auto text = std::to_string(std::lround(value));
			const float numberSize = m_settings.fontSize * 1.2f;
			const float textWidth = Engine::HudDrawList::MeasureTextWidth(text, numberSize);
			list.Text({offsetX + (x + width - 18 - textWidth) * scale,
						  offsetY + (top + (height - numberSize) * 0.5f) * scale},
				text, numberSize * scale, color);
		};
		drawFlightValue(110, "速度", snapshot.playerSpeedMetersPerSecond * 3.6f);
		drawFlightValue(1560, "高度", snapshot.playerAltitudeMeters);
		drawStatus(124,
			"機体 " + std::to_string(static_cast<int>(snapshot.playerHealth)) + "/" +
				std::to_string(static_cast<int>(snapshot.playerMaxHealth)),
			m_settings.fontSize);
		list.Line({radar.statusOrigin.x, radar.statusOrigin.y + 172 * scale},
			{radar.statusOrigin.x + 300 * scale, radar.statusOrigin.y + 172 * scale}, lineWidth, color);
		if (snapshot.result != CombatResult::Active) {
			drawLabel(740, 380, snapshot.result == CombatResult::Victory ? "任務達成" : "自機撃墜");
			drawLabel(740, 440, "R：再挑戦");
		}
		if (snapshot.playerHitSeconds > 0) {
			const auto& d = snapshot.playerHitDirection;
			const auto& f = snapshot.forward;
			const float bearing = std::atan2(-f.z * d.x + f.x * d.z, f.x * d.x + f.z * d.z);
			const float x = std::sin(bearing), y = -std::cos(bearing);
			const XMFLOAT2 center{view.width * 0.5f, view.height * 0.5f};
			const auto warning = HudStyle::Alert;
			const XMFLOAT2 tip{center.x + x * 160 * scale, center.y + y * 160 * scale};
			list.Line(tip, {tip.x - x * 25 * scale - y * 12 * scale, tip.y - y * 25 * scale + x * 12 * scale},
				lineWidth * 2, warning);
			list.Line(tip, {tip.x - x * 25 * scale + y * 12 * scale, tip.y - y * 25 * scale - x * 12 * scale},
				lineWidth * 2, warning);
			drawLabel(820, 800, "被弾");
		}
		if (snapshot.threatWarning != ThreatWarning::None) {
			const bool missile = snapshot.threatWarning == ThreatWarning::Missile;
			const auto warning = color;
			const std::string_view label = missile ? "ミサイル接近" : "敵機接近";
			const float titleSize = m_settings.fontSize * 1.8f;
			const float width = Engine::HudDrawList::MeasureTextWidth(label, titleSize);
			const float titleX = (1920 - width) * 0.5f;
			list.Text({offsetX + titleX * scale, offsetY + 56 * scale}, label, titleSize * scale, warning);
			const float barY = offsetY + 100 * scale;
			list.Line({offsetX + (titleX - 170) * scale, barY}, {offsetX + (titleX - 50) * scale, barY},
				lineWidth, color);
			list.Line({offsetX + (titleX + width + 50) * scale, barY},
				{offsetX + (titleX + width + 170) * scale, barY}, lineWidth, color);
			if (!missile) {
				list.Circle({view.width * 0.5f, offsetY + 180 * scale}, 20 * scale,
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
				const auto sightColor = color;
				list.Circle(centerPixel, 28 * scale, 1, lineWidth, sightColor);
				list.Circle(centerPixel, 19 * scale, 1, lineWidth, sightColor);
				list.Circle(centerPixel, 3 * scale, 1, lineWidth, sightColor);
				for (int i = 0; i < 4; ++i) {
					const float angle = XM_2PI * i / 4;
					const float x = std::cos(angle);
					const float y = std::sin(angle);
					list.Line({centerPixel.x + x * 31 * scale, centerPixel.y + y * 31 * scale},
						{centerPixel.x + x * 40 * scale, centerPixel.y + y * 40 * scale}, lineWidth,
						sightColor);
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
					const auto name =
						enemy.name.starts_with("ENEMY ") ? "敵機 " + enemy.name.substr(6) : enemy.name;
					const auto label = name + "  " + std::to_string(distance);
					const float labelWidth = Engine::HudDrawList::MeasureTextWidth(label, fontSize);
					list.Text({std::clamp(centerPixel.x + radius + 8 * scale, 0.0f,
								   std::max(0.0f, view.width - labelWidth - 16 * scale)),
								  std::clamp(centerPixel.y + radius + 8 * scale, 0.0f,
									  std::max(0.0f, view.height - 2 * fontSize))},
						label, fontSize, color);
					if (snapshot.lockState == LockState::Acquiring)
						list.Circle(
							centerPixel, radius + 8 * scale, snapshot.acquireProgress, lineWidth, color);
					if (snapshot.lockState == LockState::Locked)
						list.Text({centerPixel.x - radius,
									  std::max(0.0f, centerPixel.y - radius - fontSize - 4 * scale)},
							"照準", fontSize, color);
				}
			} else if (selected) {
				drawOffscreenIndicator(projected, color);
			}
		}
		if (snapshot.selected == Engine::InvalidObjectId)
			drawLabel(80, 80, "目標なし");
		else if (!snapshot.aim)
			drawLabel(80, 80, "射程外");
		if (snapshot.destroyedSeconds > 0)
			drawLabel(800, 720, "撃墜");
		else if (snapshot.hitSeconds > 0)
			drawLabel(920, 640, "命中");
		if (snapshot.rocket.configured) {
			for (size_t port = 0; port < 2; ++port) {
				std::ostringstream text;
				text << (snapshot.rocket.nextMuzzle == port ? "▶ " : "   ") << "ロケット "
					 << (port == 0 ? "L1 " : "R1 ");
				if (snapshot.rocket.cooldowns[port] <= 1e-6f)
					text << "使用可";
				else
					text << std::fixed << std::setprecision(1)
						 << std::ceil(snapshot.rocket.cooldowns[port] * 10) / 10 << "秒";
				drawStatus(210 + float(port) * 56, text.str(), m_settings.fontSize * 0.85f);
			}
		}
		if (snapshot.allDownSeconds > 0)
			drawLabel(760, 240, "全目標撃墜");
		// ロック中の敵と飛来ミサイルは、同じ画面端配置を色で区別する。
		for (const auto& position : snapshot.lockingEnemyPositions)
			drawOffscreenIndicator(projector.Project(position), HudStyle::Caution);
		if (snapshot.threatWarning == ThreatWarning::Missile)
			for (const auto& threat : snapshot.incomingMissiles) {
				const auto projected = projector.Project(threat.position);
				if (projected.visible)
					list.Circle(
						projected.pixel, std::max(10.0f, 22 * scale), 1, lineWidth * 2, HudStyle::Alert);
				else
					drawOffscreenIndicator(projected, HudStyle::Alert);
			}
	}
} // namespace Game
