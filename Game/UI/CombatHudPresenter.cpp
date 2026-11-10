#include "CombatHudPresenter.h"
#include "RadarMapPresenter.h"
#include "HudStyle.h"
#include "CinematicPresenter.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Game {
	using namespace DirectX;
	void CombatHudPresenter::Build(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		if (view.width == 0 || view.height == 0) return;
		if (snapshot.cinematic.active) {
			CinematicPresenter{}.Build(snapshot.cinematic, view, list);
			return;
		}
		RadarMapPresenter{}.Build(snapshot, view, list);
		DrawCarrierHealth(snapshot, view, list);
		DrawFlightStatus(snapshot, view, list);
		DrawGunSight(snapshot, view, list);
		DrawEnemyMarkers(snapshot, view, list);
		DrawMissionStatus(snapshot, view, list);
		DrawControls(snapshot, view, list);
	}

	void CombatHudPresenter::DrawControls(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float fontSize = 36 * scale;
		const float lineHeight = 48 * scale;
		std::vector<std::string_view> controls{"WASD：操縦", "Q / E：ヨー", "Ctrl / Space：加速 / 減速",
			"Z + マウス：見回し", "左クリック：機銃", "Tab：目標切替"};
		if (snapshot.missionPhase == MissionPhase::CarrierAttack && snapshot.result == CombatResult::Active)
			controls.push_back("右クリック：ミサイル (ロック後)");
		if (snapshot.result != CombatResult::Active) controls.push_back("R：再挑戦");

		float width = Engine::HudDrawList::MeasureTextWidth("操作方法", fontSize);
		for (const auto text : controls)
			width = std::max(width,
				Engine::HudDrawList::MeasureTextWidth("- ", fontSize) +
					Engine::HudDrawList::MeasureTextWidth(text, fontSize));
		// 画面右下から配置し、縦横比が変わっても端からの余白を維持する
		const float x = view.width - 48 * scale - width;
		float y = view.height - 48 * scale - fontSize - lineHeight * float(controls.size());
		list.Text({x, y}, "操作方法", fontSize, m_settings.color);
		for (const auto text : controls) {
			y += lineHeight;
			list.Text({x, y}, "- " + std::string(text), fontSize, m_settings.color);
		}
	}

	void CombatHudPresenter::DrawCarrierHealth(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const auto color = m_settings.color;
		// 母艦のバーは距離やカメラの向きによらず画面左上に固定する。
		if (snapshot.missionEnabled && snapshot.carrierPresent && snapshot.carrierMaxHealth > 0) {
			const float health = std::clamp(snapshot.carrierHealth, 0.0f, snapshot.carrierMaxHealth);
			const float ratio = health / snapshot.carrierMaxHealth;
			const float x = 48 * scale;
			const float y = 66 * scale;
			const float width = 760 * scale, height = 24 * scale;
			const float labelSize = 30 * scale;
			const std::string hp = std::to_string(static_cast<int>(std::ceil(health))) + " / " +
				std::to_string(static_cast<int>(std::ceil(snapshot.carrierMaxHealth)));
			list.Text({x, 26 * scale}, health > 0 ? "敵空母" : "敵空母を撃破", labelSize, color);
			list.Text({x + width - Engine::HudDrawList::MeasureTextWidth(hp, labelSize), 26 * scale}, hp,
				labelSize, color);
			list.FilledRect({x, y}, {width, height}, {0.025f, 0.025f, 0.025f, 0.8f});
			if (ratio > 0)
				list.FilledRect(
					{x, y}, {width * ratio, height}, ratio <= 0.25f ? HudStyle::Alert : HudStyle::Caution);
			list.Line({x, y}, {x + width, y}, lineWidth, color);
			list.Line({x, y + height}, {x + width, y + height}, lineWidth, color);
			list.Line({x, y}, {x, y + height}, lineWidth, color);
			list.Line({x + width, y}, {x + width, y + height}, lineWidth, color);
		}
	}

	void CombatHudPresenter::DrawFlightStatus(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const float fontSize = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		const auto radar = RadarMapPresenter::Layout(view);
		const float offsetX = (view.width - 1920 * scale) * 0.5f;
		const float offsetY = (view.height - 1080 * scale) * 0.5f;
		const auto drawStatus = [&](float y, std::string_view text) {
			list.Text({radar.statusOrigin.x, radar.statusOrigin.y + y * scale}, text, fontSize, color);
		};
		const auto drawLabel = [&](float x, float y, std::string_view text) {
			list.Text({offsetX + x * scale, offsetY + y * scale}, text, fontSize, color);
		};
		const auto drawFlightValue = [&](float x, bool pointsRight, std::string_view label, float value) {
			constexpr float top = 420, width = 240, height = 64, tipWidth = 16;
			const auto point = [&](float dx, float dy) {
				return XMFLOAT2{offsetX + (x + dx) * scale, offsetY + (top + dy) * scale};
			};
			const float left = pointsRight ? 0.0f : tipWidth;
			const float right = pointsRight ? width - tipWidth : width;
			const XMFLOAT2 outline[] = {point(left, 0), point(right, 0),
				point(width, pointsRight ? height * 0.5f : 0), point(right, height), point(left, height),
				point(0, pointsRight ? height : height * 0.5f)};
			for (size_t i = 0; i < std::size(outline); ++i)
				list.Line(outline[i], outline[(i + 1) % std::size(outline)], lineWidth, color);
			drawLabel(x + left, top - m_settings.fontSize - 8, label);
			const auto text = std::to_string(std::lround(value));
			const float textWidth = Engine::HudDrawList::MeasureTextWidth(text, m_settings.fontSize);
			drawLabel(x + right - 12 - textWidth, top + (height - m_settings.fontSize) * 0.5f, text);
		};
		drawFlightValue(100, true, "速度 km/h", snapshot.playerSpeedMetersPerSecond * 3.6f);
		drawFlightValue(1580, false, "高度 m", snapshot.playerAltitudeMeters);
		drawStatus(108,
			"機体 " + std::to_string(static_cast<int>(snapshot.playerHealth)) + "/" +
				std::to_string(static_cast<int>(snapshot.playerMaxHealth)));
		if (snapshot.result == CombatResult::Active && snapshot.enemyFireWarningSeconds > 0) {
			const std::string_view warning =
				snapshot.enemyRearFireWarningSeconds > 0 ? "後方から敵機射撃！" : "敵機射撃！";
			const float textWidth = Engine::HudDrawList::MeasureTextWidth(warning, fontSize);
			list.Text(
				{(view.width - textWidth) * 0.5f, offsetY + 330 * scale}, warning, fontSize, HudStyle::Alert);
		}
		if (snapshot.result != CombatResult::Active &&
			(snapshot.result != CombatResult::Victory || snapshot.allDownSeconds <= 0)) {
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
	}

	void CombatHudPresenter::DrawGunSight(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const auto color = m_settings.color;
		const HudProjector projector(view);
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
	}

	void CombatHudPresenter::DrawEnemyMarkers(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
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
		for (const auto& enemy : snapshot.enemies) {
			const auto projected = projector.Project(enemy.center);
			const bool selected = enemy.id == snapshot.selected;
			if (selected) DrawEnemyDirection(enemy.center, snapshot, view, list);
			if (projected.visible) {
				const auto centerPixel = projected.pixel;
				const float radius = (selected ? 32.0f : 22.0f) * scale;
				const float corner = radius * 0.4f;
				const float frameWidth = selected ? lineWidth * 1.5f : lineWidth;
				// 選択枠だけを毎秒2回明滅させ、暗い瞬間も位置を追える濃さを残す
				const float pulse = 0.5f + 0.5f * std::cos(snapshot.elapsedSeconds * XM_2PI * 2);
				const auto frameColor =
					selected ? HudStyle::Faded(HudStyle::Selected, 0.2f + 0.8f * pulse) : color;
				for (float x : {-1.0f, 1.0f})
					for (float y : {-1.0f, 1.0f}) {
						const XMFLOAT2 a{centerPixel.x + x * radius, centerPixel.y + y * radius};
						list.Line(a, {a.x - x * corner, a.y}, frameWidth, frameColor);
						list.Line(a, {a.x, a.y - y * corner}, frameWidth, frameColor);
					}
				if (selected) {
					const auto distance = int(XMVectorGetX(
						XMVector3Length(XMLoadFloat3(&enemy.center) - XMLoadFloat3(&snapshot.muzzle))));
					list.Text({std::clamp(centerPixel.x + radius + 8 * scale, 0.0f,
								   std::max(0.0f,
									   view.width -
										   Engine::HudDrawList::MeasureTextWidth(
											   enemy.name + "  " + std::to_string(distance), fontSize) -
										   16 * scale)),
								  std::clamp(centerPixel.y + radius + 8 * scale, 0.0f,
									  std::max(0.0f, view.height - 2 * fontSize))},
						enemy.name + "  " + std::to_string(distance), fontSize, color);
					if (snapshot.lockState == LockState::Acquiring)
						list.Circle(
							centerPixel, radius + 8 * scale, snapshot.acquireProgress, lineWidth, color);
					if (snapshot.lockState == LockState::Locked)
						list.Text({centerPixel.x - radius,
									  std::max(0.0f, centerPixel.y - radius - fontSize - 4 * scale)},
							"ロック完了", fontSize, color);
				}
			}
		}
		if (snapshot.selected == Engine::InvalidObjectId) {
			m_arrowTarget = Engine::InvalidObjectId;
			m_arrowSeconds = -1;
			drawLabel(80, 80, "目標なし");
		} else if (!snapshot.aim && snapshot.missionPhase != MissionPhase::CarrierAttack)
			drawLabel(80, 80, "射程外");
		if (snapshot.destroyedSeconds > 0)
			drawLabel(800, 720, "撃墜");
		else if (snapshot.hitSeconds > 0)
			drawLabel(920, 640, "命中");
		if (snapshot.allDownSeconds > 0) drawLabel(760, 180, "全目標撃破");
	}

	void CombatHudPresenter::DrawMissionStatus(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const float fontSize = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		const auto radar = RadarMapPresenter::Layout(view);
		const HudProjector projector(view);
		const float offsetX = (view.width - 1920 * scale) * 0.5f;
		const float offsetY = (view.height - 1080 * scale) * 0.5f;
		const auto drawStatus = [&](float y, std::string_view text) {
			list.Text({radar.statusOrigin.x, radar.statusOrigin.y + y * scale}, text, fontSize, color);
		};
		const auto drawLabel = [&](float x, float y, std::string_view text) {
			list.Text({offsetX + x * scale, offsetY + y * scale}, text, fontSize, color);
		};

		{
			const auto phase = snapshot.missionPhase;
			std::string objective;
			switch (phase) {
			case MissionPhase::Patrol:
				objective = "1/3：哨戒機をすべて撃墜せよ";
				break;
			case MissionPhase::CarrierAttack:
				objective = "2/3：ミサイルで敵空母を撃破せよ";
				break;
			case MissionPhase::FinalFighters:
				objective = "3/3：残りの敵戦闘機をすべて撃墜せよ";
				break;
			case MissionPhase::Commander:
				objective = "3/3：敵編隊長機を撃墜せよ";
				break;
			case MissionPhase::Complete:
				objective = "任務達成";
				break;
			case MissionPhase::Failed:
				objective = "任務失敗";
				break;
			}
			drawLabel(540, 116, objective);
			drawLabel(540, 160, "残りの敵機：" + std::to_string(snapshot.enemies.size()));
			if (snapshot.deckDisabled) drawLabel(540, 204, "敵空母を撃破");
			if (snapshot.missionMessageSeconds > 0 && snapshot.result == CombatResult::Active) {
				if (phase == MissionPhase::CarrierAttack)
					drawLabel(540, 248, "敵空母を照準に捉え、ロックせよ");
				else if (phase == MissionPhase::FinalFighters)
					drawLabel(540, 248, "艦隊の外の交戦区域で、敵戦闘機を撃墜せよ");
				else if (phase == MissionPhase::Commander)
					drawLabel(540, 248, "敵編隊長機が出現。撃墜して任務を完了せよ");
			}
			if (snapshot.result == CombatResult::Active && phase == MissionPhase::CarrierAttack &&
				snapshot.carrierPresent && !snapshot.deckDisabled &&
				snapshot.selected != Engine::InvalidObjectId) {
				DrawEnemyDirection(snapshot.objectivePosition, snapshot, view, list);
				const auto marker = projector.Project(snapshot.objectivePosition);
				if (marker.visible) {
					const float radius = 44 * scale;
					const auto frameColor =
						snapshot.lockState == LockState::Locked ? HudStyle::Alert : HudStyle::Selected;
					// 母艦は六角形で囲み、敵機の四隅枠と区別する
					for (int i = 0; i < 6; ++i) {
						const float angle = XM_2PI * float(i) / 6;
						const float nextAngle = XM_2PI * float(i + 1) / 6;
						list.Line({marker.pixel.x + std::cos(angle) * radius,
									  marker.pixel.y + std::sin(angle) * radius},
							{marker.pixel.x + std::cos(nextAngle) * radius,
								marker.pixel.y + std::sin(nextAngle) * radius},
							lineWidth * 1.5f, frameColor);
					}
					list.Text(
						{marker.pixel.x + radius + 8 * scale, marker.pixel.y}, "敵空母", fontSize, color);
					if (snapshot.lockState == LockState::Acquiring)
						list.Circle(
							marker.pixel, radius + 8 * scale, snapshot.acquireProgress, lineWidth, color);
					if (snapshot.lockState == LockState::Locked)
						list.Text({marker.pixel.x - radius, marker.pixel.y - radius - fontSize - 4 * scale},
							"ロック完了", fontSize, frameColor);
				} else
					DrawOffscreenIndicator(marker, color, view, list);
			}
			if (snapshot.result == CombatResult::Active &&
				(phase == MissionPhase::FinalFighters || phase == MissionPhase::Commander)) {
				const auto marker = projector.Project(snapshot.objectivePosition);
				if (marker.visible) {
					list.Circle(marker.pixel, 26 * scale, 1, lineWidth * 2, color);
					list.Text({marker.pixel.x + 32 * scale, marker.pixel.y}, "交戦区域", fontSize, color);
				} else
					DrawOffscreenIndicator(marker, color, view, list);
			}
			if (phase == MissionPhase::CarrierAttack && snapshot.result == CombatResult::Active) {
				std::string lock = "敵空母：照準を合わせてロック";
				if (snapshot.lockState == LockState::Locked)
					lock = "敵空母：ロック完了";
				else if (snapshot.lockState == LockState::Acquiring)
					lock = "敵空母：ロック中 " + std::to_string(int(snapshot.acquireProgress * 100)) + "%";
				drawStatus(236, lock);
				std::ostringstream bomb;
				bomb << "ミサイル " << snapshot.carrierMissileAmmo << "/2 ";
				if (snapshot.missileCooldownSeconds <= 0)
					bomb << (snapshot.lockState == LockState::Locked ? "[右クリック]" : "ロックなし");
				else
					bomb << "再装填 " << std::fixed << std::setprecision(1) << snapshot.missileCooldownSeconds
						 << "秒";
				drawStatus(188, bomb.str());
				if (snapshot.missileImpactPoint) {
					const auto impact = projector.Project(*snapshot.missileImpactPoint);
					if (impact.visible) {
						const float r = 14 * scale;
						list.Line({impact.pixel.x - r, impact.pixel.y}, {impact.pixel.x + r, impact.pixel.y},
							lineWidth * 2, color);
						list.Line({impact.pixel.x, impact.pixel.y - r}, {impact.pixel.x, impact.pixel.y + r},
							lineWidth * 2, color);
					} else
						DrawOffscreenIndicator(impact, HudStyle::Caution, view, list);
				}
			}
		}
	}

	void CombatHudPresenter::DrawEnemyDirection(const XMFLOAT3& target, const CombatSnapshot& snapshot,
		const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const auto color = m_settings.color;

		const auto worldDirection = XMLoadFloat3(&target) - XMLoadFloat3(&snapshot.muzzle);
		const auto cameraDirection = XMVector3TransformNormal(worldDirection, XMLoadFloat4x4(&view.view));
		const float distance = XMVectorGetX(XMVector3Length(cameraDirection));
		if (!std::isfinite(distance) || distance < 0.001f) return;
		auto direction = cameraDirection / distance;
		XMFLOAT3 d;
		XMStoreFloat3(&d, direction);
		const bool continuing = m_arrowTarget == snapshot.selected && m_arrowSeconds >= 0 &&
			snapshot.elapsedSeconds >= m_arrowSeconds;
		if (!continuing) m_rearTurnSide = d.x < 0 ? -1.0f : 1.0f;
		const bool behind = d.z < 0;
		if (behind) {
			// 後方は方位角を展開して旋回方向を示し、奥行きによる矢印の潰れを防ぐ
			// 真後ろ付近では直前の旋回側を保持し、左右の微小な変化では反転させない
			if (std::abs(d.x) > 0.5f) m_rearTurnSide = d.x < 0 ? -1.0f : 1.0f;
			const float yaw = std::abs(std::atan2(d.x, d.z)) * m_rearTurnSide;
			const float pitch = std::atan2(d.y, std::hypot(d.x, d.z));
			const auto turn = XMVector3Normalize(XMVectorSet(yaw, pitch, -0.35f, 0));
			const float blend = std::clamp(-d.z / 0.35f, 0.0f, 1.0f);
			direction = XMVector3Normalize(XMVectorLerp(direction, turn, blend));
			XMStoreFloat3(&d, direction);
		} else
			m_rearTurnSide = d.x < 0 ? -1.0f : 1.0f;
		// 方向変更時は中央の周囲を連続して回す。左右の座標を直接切り替えない
		const float lateral = std::hypot(d.x, d.y);
		const float desiredAngle = lateral > 0.001f ? std::atan2(d.y, d.x) : m_arrowAngle;
		if (continuing) {
			const float dt = std::clamp(snapshot.elapsedSeconds - m_arrowSeconds, 0.0f, 0.1f);
			const float difference = std::remainder(desiredAngle - m_arrowAngle, XM_2PI);
			m_arrowAngle += std::clamp(difference, -4 * dt, 4 * dt);
		} else
			m_arrowAngle = desiredAngle;
		m_arrowAngle = std::remainder(m_arrowAngle, XM_2PI);
		d.x = std::cos(m_arrowAngle) * lateral;
		d.y = std::sin(m_arrowAngle) * lateral;
		direction = XMLoadFloat3(&d);
		m_arrowTarget = snapshot.selected;
		m_arrowSeconds = snapshot.elapsedSeconds;
		// 正面・真後ろでも基底が潰れないよう、視線に直交する軸を選ぶ。
		const auto reference = std::abs(d.z) < 0.95f ? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
		const auto side = XMVector3Normalize(XMVector3Cross(reference, direction));
		const auto up = XMVector3Cross(direction, side);
		// 敵の方位に合わせて中央の周囲を移動する。視線軸の近くでは
		// 半径を縮め、正面を横切る際に反対側へ突然跳ばないようにする。
		const float radial = 110 * scale / std::max(0.35f, std::hypot(d.x, d.y));
		const XMFLOAT2 anchor{view.width * 0.5f + d.x * radial, view.height * 0.5f - d.y * radial};
		const auto projectArrow = [&](FXMVECTOR point) {
			XMFLOAT3 p;
			XMStoreFloat3(&p, point);
			const float perspective = 420 / (420 + p.z);
			return XMFLOAT2{anchor.x + p.x * perspective * scale, anchor.y - p.y * perspective * scale};
		};
		// 細長い山形の後端を内側へ折り込み、稜線だけで薄い立体感を出す。
		// 四角錐の底面や軸線は描かず、矢印として読める輪郭を優先する。
		const auto tip = projectArrow(direction * 64);
		const auto left = projectArrow(direction * -42 + side * 17);
		const auto right = projectArrow(direction * -42 - side * 17);
		const auto notch = projectArrow(direction * -23);
		const auto ridge = projectArrow(direction * -34 + up * 9);
		const auto arrowColor = color;
		auto ridgeColor = arrowColor;
		ridgeColor.w = 0.65f;
		list.Line(tip, left, lineWidth, arrowColor);
		list.Line(left, notch, lineWidth, arrowColor);
		list.Line(notch, right, lineWidth, arrowColor);
		list.Line(right, tip, lineWidth, arrowColor);
		list.Line(tip, ridge, lineWidth, ridgeColor);
		list.Line(ridge, left, lineWidth, ridgeColor);
		list.Line(ridge, right, lineWidth, ridgeColor);
		if (behind) {
			constexpr std::string_view label = "後方";
			const float labelSize = 24 * scale;
			list.Text({anchor.x - Engine::HudDrawList::MeasureTextWidth(label, labelSize) * 0.5f,
						  anchor.y + 70 * scale},
				label, labelSize, arrowColor);
		}
	}
	void CombatHudPresenter::DrawOffscreenIndicator(const HudProjection& projection, XMFLOAT4 tint,
		const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);

		if (projection.visible || !std::isfinite(projection.pixel.x) || !std::isfinite(projection.pixel.y))
			return;
		const float centerX = view.width * 0.5f;
		const float centerY = view.height * 0.5f;
		float deltaX = projection.pixel.x - centerX;
		float deltaY = projection.pixel.y - centerY;
		if (std::hypot(deltaX, deltaY) < 0.001f) deltaY = 1;
		const float ratio = 1 /
			std::max(std::abs(deltaX) / std::max(1.0f, centerX - 50 * scale),
				std::abs(deltaY) / std::max(1.0f, centerY - 50 * scale));
		const XMFLOAT2 tip{centerX + deltaX * ratio, centerY + deltaY * ratio};
		const float length = std::hypot(deltaX, deltaY);
		const float unitX = deltaX / length;
		const float unitY = deltaY / length;
		list.Line(tip,
			{tip.x - unitX * 20 * scale - unitY * 9 * scale, tip.y - unitY * 20 * scale + unitX * 9 * scale},
			lineWidth, tint);
		list.Line(tip,
			{tip.x - unitX * 20 * scale + unitY * 9 * scale, tip.y - unitY * 20 * scale - unitX * 9 * scale},
			lineWidth, tint);
	}
} // namespace Game
