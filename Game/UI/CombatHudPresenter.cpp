#include "CombatHudPresenter.h"
#include "RadarMapPresenter.h"
#include "HudStyle.h"
#include "CinematicPresenter.h"
#include "RadioCaptionPresenter.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace Game {
	using namespace DirectX;

	void CombatHudPresenter::Build(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list, bool usingGamepad) const {
		if (view.width == 0 || view.height == 0) return;

		// 映像演出中は通常HUDを専用の表示へ切り替える
		if (snapshot.cinematic.active) {
			CinematicPresenter{m_tuning.ink}.Build(snapshot.cinematic, view, list, usingGamepad);

			if (snapshot.enemyRadio.clip >= 0 && snapshot.cinematic.radioLine < 0 && snapshot.cinematic.briefingLine < 0)
				RadioCaptionPresenter{}.Build(snapshot.enemyRadio.caption, view, list,
					view.height * snapshot.cinematic.text.barHeightFraction);
			return;
		}

		// 通常戦闘の情報を描画順に重ねる
		RadarMapPresenter{}.Build(snapshot, view, list, m_tuning);
		DrawFlightStatus(snapshot, view, list);
		DrawGunSight(snapshot, view, list);
		DrawEnemyMarkers(snapshot, view, list);
		DrawMissionStatus(snapshot, view, list, usingGamepad);
		DrawControls(snapshot, view, list, usingGamepad);
		DrawCombatGuide(snapshot, view, list, usingGamepad);
		DrawDebugCollisions(snapshot, view, list);

		// 最後に現在の無線字幕を重ねる
		if (snapshot.cinematic.radioLine >= 0)
			RadioCaptionPresenter{}.Build(snapshot.cinematic.radio, view, list);
		else if (snapshot.enemyRadio.clip >= 0)
			RadioCaptionPresenter{}.Build(snapshot.enemyRadio.caption, view, list);
	}

	void CombatHudPresenter::DrawCombatGuide(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list, bool usingGamepad) const {
		constexpr float kPatrolGuideDurationSeconds = 5.0f;
		constexpr float kGuideScreenHorizontalMargin = 80.0f;
		constexpr float kGuideTitleFontSize = 50.0f;
		constexpr float kGuideDetailFontSize = 34.0f;
		constexpr float kGuideBoxHorizontalPadding = 48.0f;
		constexpr float kGuideBoxVerticalPadding = 50.0f;
		constexpr float kGuideCenterHeightFraction = 0.76f;
		constexpr float kGuideShadowOffset = 2.0f;
		constexpr float kGuideTitleTopOffset = 18.0f;
		constexpr float kGuideDetailTopOffset = 32.0f;
		if (snapshot.result != CombatResult::Active) return;

		// 任務の段階に合った操作案内を選ぶ
		std::string title, detail;
		bool locked = false;

		if (snapshot.missionPhase == MissionPhase::Patrol &&
			snapshot.elapsedSeconds < kPatrolGuideDurationSeconds && !snapshot.enemies.empty()) {
			title = m_text.Text(usingGamepad ? "×で射撃" : "左クリックで射撃");
			detail = m_text.Text("敵機に照準を合わせて撃墜せよ");
		} else if (snapshot.missionPhase == MissionPhase::CarrierAttack && snapshot.carrierPresent &&
			!snapshot.deckDisabled && snapshot.selected != Engine::InvalidObjectId) {
			locked = snapshot.lockState == LockState::Locked;
			title = locked ? m_text.Text(usingGamepad ? "○で射撃" : "右クリックで射撃")
						   : m_text.Text("母艦に照準を合わせてロックオン");
			detail = locked ? m_text.Text("ロックオン完了")
							: m_text.Text(usingGamepad ? "ロックオン後、○で射撃" : "ロックオン後、右クリックで射撃");

		} else return;

		const float scale = HudStyle::FitScale(view.width, view.height);

		// 案内文が画面幅に収まる文字サイズを求める
		const float available = view.width - kGuideScreenHorizontalMargin * scale;
		const float titleSize = std::min(kGuideTitleFontSize * scale,
			available / std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(title, 1)));
		const float detailSize = std::min(kGuideDetailFontSize * scale,
			available / std::max(1.0f, Engine::HudDrawList::MeasureTextWidth(detail, 1)));
		const float titleWidth = Engine::HudDrawList::MeasureTextWidth(title, titleSize);
		const float detailWidth = Engine::HudDrawList::MeasureTextWidth(detail, detailSize);
		const float width = std::max(titleWidth, detailWidth) + kGuideBoxHorizontalPadding * scale;
		const float height = titleSize + detailSize + kGuideBoxVerticalPadding * scale;
		const float top = view.height * kGuideCenterHeightFraction - height * 0.5f;
		list.FilledRect({(view.width - width) * 0.5f, top}, {width, height}, {0.015f, 0.025f, 0.04f, 0.78f});
		const auto draw = [&](const std::string& text, float y, float size, float textWidth, XMFLOAT4 color) {
			const float x = (view.width - textWidth) * 0.5f;
			list.Text({x + kGuideShadowOffset * scale, y + kGuideShadowOffset * scale}, text, size, {0, 0, 0, 1});
			list.Text({x, y}, text, size, color);
		};

		draw(title, top + kGuideTitleTopOffset * scale, titleSize, titleWidth,
			locked ? m_tuning.selected : XMFLOAT4{1, 1, 1, 1});
		draw(detail, top + kGuideDetailTopOffset * scale + titleSize, detailSize, detailWidth, m_settings.color);
	}

	void CombatHudPresenter::DrawDebugCollisions(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
#if defined(_DEBUG)
		if (!snapshot.debugCollisionsEnabled) return;

		const HudProjector projector(view);

		for (const auto& box : snapshot.debugCollisions) {
			XMFLOAT3 corners[8];
			const auto& bounds = box.target.bounds;

			for (int corner = 0; corner < 8; ++corner)
				XMStoreFloat3(&corners[corner], XMVector3TransformCoord(XMVectorSet(
					corner & 1 ? bounds.maximum.x : bounds.minimum.x,
					corner & 2 ? bounds.maximum.y : bounds.minimum.y,
					corner & 4 ? bounds.maximum.z : bounds.minimum.z, 1), XMLoadFloat4x4(&box.target.world)));

			// 8頂点の各軸方向を一度ずつ結び、OBBの12辺を描く
			for (int corner = 0; corner < 8; ++corner)
				for (int axis : {1, 2, 4})
					if (!(corner & axis))
						if (const auto line = projector.ProjectClippedSegment(corners[corner], corners[corner | axis]))
							list.Line(line->head, line->tail, 2, box.color);
		}

		list.Text({24, 20}, "F3 COLLISION | PLAYER: GREEN  ENEMY: RED  CARRIER DECK: YELLOW", 18,
			{1, 1, 1, 1});
#else
		(void)snapshot;
		(void)view;
		(void)list;
#endif
	}

	void CombatHudPresenter::DrawControls(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list, bool usingGamepad) const {
		const float scale = HudStyle::FitScale(view.width, view.height);
		const float fontSize = m_tuning.layout.controlsFontSize * scale;
		const float lineHeight = m_tuning.layout.controlsLineHeight * scale;
		std::vector<std::string_view> controls{
			m_text.Text(usingGamepad ? "左スティック：操縦" : "WASD：操縦"),
			m_text.Text(usingGamepad ? "L1 / R1：ヨー" : "Q / E：ヨー"),
			m_text.Text(usingGamepad ? "R2 / L2：加速 / 減速" : "Ctrl / Space：加速 / 減速"),
			m_text.Text(usingGamepad ? "右スティック：見回し" : "ホイール押込 + マウス：見回し"),
			m_text.Text(usingGamepad ? "△ / △長押し：目標切替 / 目標を注視" : "Z長押し：選択中の敵機を注視"),
			m_text.Text(usingGamepad ? "×：機銃" : "左クリック：機銃")};
		if (!usingGamepad) controls.push_back(m_text.Text("Tab：目標切替"));
		if (snapshot.missionPhase == MissionPhase::CarrierAttack && snapshot.result == CombatResult::Active)
			controls.push_back(m_text.Text(usingGamepad ? "○：ミサイル (ロック後)" : "右クリック：ミサイル (ロック後)"));

		float width = Engine::HudDrawList::MeasureTextWidth(m_text.Text("操作方法"), fontSize);

		for (const auto text : controls)
			width = std::max(width,
				Engine::HudDrawList::MeasureTextWidth("- ", fontSize) +
					Engine::HudDrawList::MeasureTextWidth(text, fontSize));

		// 画面右下から配置し、縦横比が変わっても端からの余白を維持する
		const float x = view.width - m_tuning.layout.controlsRightMargin * scale - width;
		float y = view.height - m_tuning.layout.controlsBottomMargin * scale - fontSize -
			lineHeight * float(controls.size());
		list.Text({x, y}, m_text.Text("操作方法"), fontSize, m_settings.color);

		for (const auto text : controls) {
			y += lineHeight;
			list.Text({x, y}, "- " + std::string(text), fontSize, m_settings.color);
		}
	}

	void CombatHudPresenter::DrawFlightStatus(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		const float scale = HudStyle::FitScale(view.width, view.height);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const float fontSize = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		const auto radar = RadarMapPresenter::Layout(view, m_tuning);
		const float offsetX = (view.width - HudStyle::ReferenceWidth * scale) * 0.5f;
		const float offsetY = (view.height - HudStyle::ReferenceHeight * scale) * 0.5f;
		const auto drawStatus = [&](float y, std::string_view text) {
			list.Text({radar.statusOrigin.x, radar.statusOrigin.y + y * scale}, text, fontSize, color);
		};

		const auto drawLabel = [&](float x, float y, std::string_view text) {
			list.Text({offsetX + x * scale, offsetY + y * scale}, text, fontSize, color);
		};

		// 計器の枠を作り、数値を枠の内側へ右寄せする
		const auto drawFlightValue = [&](float x, bool pointsRight, std::string_view label, float value) {
			const float top = m_tuning.layout.flightPanelTop, width = m_tuning.layout.flightPanelWidth,
						height = m_tuning.layout.flightPanelHeight,
						tipWidth = m_tuning.layout.flightPanelTipWidth;
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
			drawLabel(x + left, top - m_settings.fontSize - m_tuning.layout.flightLabelGap, label);
			const auto text = std::to_string(std::lround(value));
			const float textWidth = Engine::HudDrawList::MeasureTextWidth(text, m_settings.fontSize);
			drawLabel(x + right - m_tuning.layout.flightValuePadding - textWidth,
				top + (height - m_settings.fontSize) * 0.5f, text);
		};

		// 速度の表示倍率はHUDの数値にだけ適用する
		constexpr float speedDisplayScale = 0.1f;
		constexpr float kMetersPerSecondToKilometersPerHour = 3.6f;
		drawFlightValue(m_tuning.layout.speedPanelX, true, m_text.Text("速度 km/h"),
			snapshot.playerSpeedMetersPerSecond * kMetersPerSecondToKilometersPerHour * speedDisplayScale);
		drawFlightValue(
			m_tuning.layout.altitudePanelX, false, m_text.Text("高度 m"), snapshot.playerAltitudeMeters);
		drawStatus(m_tuning.layout.healthStatusY,
			m_text.Text("機体 ") + std::to_string(static_cast<int>(snapshot.playerHealth)) + "/" +
				std::to_string(static_cast<int>(snapshot.playerMaxHealth)));
		if (snapshot.result == CombatResult::Active && snapshot.enemyFireWarningSeconds > 0) {
			const std::string_view warning = snapshot.enemyRearFireWarningSeconds > 0
				? m_text.Text("後方から敵機射撃！")
				: m_text.Text("敵機射撃！");
			const float textWidth = Engine::HudDrawList::MeasureTextWidth(warning, fontSize);
			list.Text({(view.width - textWidth) * 0.5f, offsetY + m_tuning.layout.fireWarningY * scale},
				warning, fontSize, m_tuning.alert);
		}

		if (snapshot.result == CombatResult::Victory && snapshot.allDownSeconds <= 0) {
			drawLabel(m_tuning.layout.resultPosition[0], m_tuning.layout.resultPosition[1],
				m_text.Text("任務達成"));
		}

		// 自機の前方を基準に被弾方向の矢印を表示する
		if (snapshot.playerHitSeconds > 0) {
			constexpr float kHitArrowRadius = 160.0f;
			constexpr float kHitArrowLength = 25.0f;
			constexpr float kHitArrowHalfWidth = 12.0f;
			constexpr float kHitArrowStrokeScale = 2.0f;
			const auto& d = snapshot.playerHitDirection;
			const auto& f = snapshot.forward;
			const float bearing = std::atan2(-f.z * d.x + f.x * d.z, f.x * d.x + f.z * d.z);
			const float x = std::sin(bearing), y = -std::cos(bearing);
			const XMFLOAT2 center{view.width * 0.5f, view.height * 0.5f};
			const auto warning = m_tuning.alert;
			const XMFLOAT2 tip{center.x + x * kHitArrowRadius * scale, center.y + y * kHitArrowRadius * scale};
			list.Line(tip, {tip.x - x * kHitArrowLength * scale - y * kHitArrowHalfWidth * scale,
					tip.y - y * kHitArrowLength * scale + x * kHitArrowHalfWidth * scale},
				lineWidth * kHitArrowStrokeScale, warning);
			list.Line(tip, {tip.x - x * kHitArrowLength * scale + y * kHitArrowHalfWidth * scale,
					tip.y - y * kHitArrowLength * scale - x * kHitArrowHalfWidth * scale},
				lineWidth * kHitArrowStrokeScale, warning);
			drawLabel(m_tuning.layout.playerHitPosition[0], m_tuning.layout.playerHitPosition[1],
				m_text.Text("被弾"));
		}
	}

	void CombatHudPresenter::DrawGunSight(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		constexpr float kCrosshairHalfLength = 12.0f;
		constexpr float kLeadRingRadii[] = {34.0f, 25.0f, 5.0f};
		constexpr float kLeadTickStartRadius = 31.0f;
		constexpr float kLeadMajorTickEndRadius = 40.0f;
		constexpr float kLeadMinorTickEndRadius = 37.0f;
		constexpr float kLeadCrossStartRadius = 10.0f;
		constexpr float kLeadCrossEndRadius = 23.0f;
		const float scale =
			HudStyle::FitScale(view.width, view.height) * m_tuning.layout.gunSightScale;
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
			const float reach = kCrosshairHalfLength * scale;
			list.Line({sightProjection.pixel.x - reach, sightProjection.pixel.y},
				{sightProjection.pixel.x + reach, sightProjection.pixel.y}, lineWidth, color);
			list.Line({sightProjection.pixel.x, sightProjection.pixel.y - reach},
				{sightProjection.pixel.x, sightProjection.pixel.y + reach}, lineWidth, color);
		}

		// 選択対象の迎撃位置に予測マーカーを表示する
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
				for (float radius : kLeadRingRadii)
					list.Circle(centerPixel, radius * scale, 1, lineWidth, sightColor);

				constexpr int kSightTickCount = 12;
				constexpr int kSightMajorTickInterval = 3;
				for (int i = 0; i < kSightTickCount; ++i) {
					const float angle = XM_2PI * i / kSightTickCount;
					const float x = std::cos(angle);
					const float y = std::sin(angle);
					const float endRadius = i % kSightMajorTickInterval == 0
						? kLeadMajorTickEndRadius : kLeadMinorTickEndRadius;
					list.Line({centerPixel.x + x * kLeadTickStartRadius * scale,
							centerPixel.y + y * kLeadTickStartRadius * scale},
						{centerPixel.x + x * endRadius * scale, centerPixel.y + y * endRadius * scale},
						lineWidth, sightColor);
				}

				for (const XMFLOAT2 axis : {XMFLOAT2{1, 0}, XMFLOAT2{-1, 0}, XMFLOAT2{0, 1}, XMFLOAT2{0, -1}})
					list.Line({centerPixel.x + axis.x * kLeadCrossStartRadius * scale,
							centerPixel.y + axis.y * kLeadCrossStartRadius * scale},
						{centerPixel.x + axis.x * kLeadCrossEndRadius * scale,
							centerPixel.y + axis.y * kLeadCrossEndRadius * scale}, lineWidth, sightColor);
			}
		}
	}

	void CombatHudPresenter::DrawEnemyMarkers(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list) const {
		constexpr float kMarkerCornerRadiusFraction = 0.4f;
		constexpr float kSelectedMarkerStrokeScale = 1.5f;
		constexpr float kSelectedMarkerPulseHz = 2.0f;
		constexpr float kSelectedMarkerMinOpacity = 0.2f;
		constexpr float kSelectedMarkerOpacityRange = 0.8f;
		const float scale = HudStyle::FitScale(view.width, view.height);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const float fontSize = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		const HudProjector projector(view);
		const float offsetX = (view.width - HudStyle::ReferenceWidth * scale) * 0.5f;
		const float offsetY = (view.height - HudStyle::ReferenceHeight * scale) * 0.5f;
		const auto drawLabel = [&](float x, float y, std::string_view text) {
			list.Text({offsetX + x * scale, offsetY + y * scale}, text, fontSize, color);
		};

		for (const auto& enemy : snapshot.enemies) {
			const auto projected = projector.Project(enemy.center);
			const bool selected = enemy.id == snapshot.selected;

			if (selected) DrawEnemyDirection(enemy.center, snapshot, view, list);
			if (projected.visible) {
				const auto centerPixel = projected.pixel;
				const float radius =
					(selected ? m_tuning.layout.selectedEnemyRadius : m_tuning.layout.enemyRadius) * scale;
				const float corner = radius * kMarkerCornerRadiusFraction;
				const float frameWidth = selected ? lineWidth * kSelectedMarkerStrokeScale : lineWidth;

				// 選択枠だけを毎秒2回明滅させ、暗い瞬間も位置を追える濃さを残す
				const float pulse = 0.5f + 0.5f * std::cos(snapshot.elapsedSeconds * XM_2PI * kSelectedMarkerPulseHz);
				const auto frameColor =
					selected ? HudStyle::Faded(m_tuning.selected,
						kSelectedMarkerMinOpacity + kSelectedMarkerOpacityRange * pulse) : color;
				for (float x : {-1.0f, 1.0f})
					for (float y : {-1.0f, 1.0f}) {
						const XMFLOAT2 a{centerPixel.x + x * radius, centerPixel.y + y * radius};
						list.Line(a, {a.x - x * corner, a.y}, frameWidth, frameColor);
						list.Line(a, {a.x, a.y - y * corner}, frameWidth, frameColor);
					}

				if (selected) {
					const auto distance = int(XMVectorGetX(
						XMVector3Length(XMLoadFloat3(&enemy.center) - XMLoadFloat3(&snapshot.muzzle))));
					list.Text(
						{std::clamp(centerPixel.x + radius + m_tuning.layout.markerTextGap * scale, 0.0f,
							 std::max(0.0f,
								 view.width -
									 Engine::HudDrawList::MeasureTextWidth(
										 enemy.name + "  " + std::to_string(distance), fontSize) -
									 m_tuning.layout.screenTextMargin * scale)),
							std::clamp(centerPixel.y + radius + m_tuning.layout.markerTextGap * scale, 0.0f,
								std::max(0.0f, view.height - 2 * fontSize))},
						enemy.name + "  " + std::to_string(distance), fontSize, color);
					if (snapshot.lockState == LockState::Acquiring)
						list.Circle(centerPixel, radius + m_tuning.layout.markerTextGap * scale,
							snapshot.acquireProgress, lineWidth, color);
					if (snapshot.lockState == LockState::Locked)
						list.Text({centerPixel.x - radius,
									  std::max(0.0f,
										  centerPixel.y - radius - fontSize -
											  m_tuning.layout.markerLabelGap * scale)},
							m_text.Text("ロック完了"), fontSize, color);
				}
			}
		}

		if (snapshot.selected == Engine::InvalidObjectId) {
			m_arrowTarget = Engine::InvalidObjectId;
			m_arrowSeconds = -1;
			drawLabel(m_tuning.layout.targetStatusPosition[0], m_tuning.layout.targetStatusPosition[1],
				m_text.Text("目標なし"));
		} else if (!snapshot.aim && snapshot.missionPhase != MissionPhase::CarrierAttack)
			drawLabel(m_tuning.layout.targetStatusPosition[0], m_tuning.layout.targetStatusPosition[1],
				m_text.Text("射程外"));
		if (snapshot.destroyedSeconds > 0)
			drawLabel(m_tuning.layout.destroyedPosition[0], m_tuning.layout.destroyedPosition[1],
				m_text.Text("撃墜"));
		else if (snapshot.hitSeconds > 0)
			drawLabel(m_tuning.layout.hitPosition[0], m_tuning.layout.hitPosition[1], m_text.Text("命中"));
		if (snapshot.allDownSeconds > 0)
			drawLabel(m_tuning.layout.allDownPosition[0], m_tuning.layout.allDownPosition[1],
				m_text.Text("全目標撃破"));
	}

	void CombatHudPresenter::DrawMissionStatus(
		const CombatSnapshot& snapshot, const Engine::RenderView& view, Engine::HudDrawList& list, bool usingGamepad) const {
		constexpr int kCarrierMarkerSides = 6;
		constexpr float kCarrierMarkerStrokeScale = 1.5f;
		constexpr float kCombatAreaLabelGap = 6.0f;
		constexpr float kObjectiveMarkerStrokeScale = 2.0f;
		constexpr float kPercentPerUnitProgress = 100.0f;
		const float scale = HudStyle::FitScale(view.width, view.height);
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const float fontSize = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		const auto radar = RadarMapPresenter::Layout(view, m_tuning);
		const HudProjector projector(view);
		const auto drawStatus = [&](float y, std::string_view text) {
			list.Text({radar.statusOrigin.x, radar.statusOrigin.y + y * scale}, text, fontSize, color);
		};

		const auto drawLabel = [&](float rightEdge, float y, std::string_view text) {
			const float right = view.width - (HudStyle::ReferenceWidth - rightEdge) * scale;
			const float width = Engine::HudDrawList::MeasureTextWidth(text, fontSize);
			list.Text({right - width, y * scale}, text, fontSize, color);
		};
		const auto drawMissionLine = [&](unsigned line, std::string_view text) {
			drawLabel(m_tuning.layout.missionX, m_tuning.layout.missionY + line * m_tuning.layout.missionLineHeight,
				text);
		};

		{
			// 任務の段階と残存目標数を画面右上へ表示する
			const auto phase = snapshot.missionPhase;
			std::string objective;

			switch (phase) {
			case MissionPhase::Patrol:
				objective = m_text.Text("1/3：哨戒機をすべて撃墜せよ");
				break;
			case MissionPhase::CarrierAttack:
				objective = m_text.Text("2/3：ミサイルで敵空母を撃破せよ");
				break;
			case MissionPhase::FinalFighters:
				objective = m_text.Text("3/3：残りの敵戦闘機をすべて撃墜せよ");
				break;
			case MissionPhase::Commander:
				objective = m_text.Text("3/3：敵編隊長機を撃墜せよ");
				break;
			case MissionPhase::Complete:
				objective = m_text.Text("任務達成");
				break;
			case MissionPhase::Failed:
				objective = m_text.Text("任務失敗");
				break;
			}

			drawMissionLine(0, objective);
			drawMissionLine(1,
				m_text.Text("残りの敵機：") + std::to_string(snapshot.enemies.size()));
			if (snapshot.deckDisabled)
				drawMissionLine(2, m_text.Text("敵空母を撃破"));
			if (snapshot.missionMessageSeconds > 0 && snapshot.result == CombatResult::Active) {
				constexpr unsigned kMissionMessageLine = 3;
				if (phase == MissionPhase::FinalFighters)
					drawMissionLine(kMissionMessageLine,
						m_text.Text("艦隊の外の交戦区域で、敵戦闘機を撃墜せよ"));
				else if (phase == MissionPhase::Commander)
					drawMissionLine(kMissionMessageLine,
						m_text.Text("敵編隊長機が出現。撃墜して任務を完了せよ"));
			}

			if (snapshot.result == CombatResult::Active && phase == MissionPhase::CarrierAttack &&
				snapshot.carrierPresent && !snapshot.deckDisabled &&
				snapshot.selected != Engine::InvalidObjectId) {
				DrawEnemyDirection(snapshot.objectivePosition, snapshot, view, list);
				const auto marker = projector.Project(snapshot.objectivePosition);

				if (marker.visible) {
					const float radius = m_tuning.layout.carrierMarkerRadius * scale;
					const auto frameColor =
						snapshot.lockState == LockState::Locked ? m_tuning.alert : m_tuning.selected;

					// 母艦は六角形で囲み、敵機の四隅枠と区別する
					for (int i = 0; i < kCarrierMarkerSides; ++i) {
						const float angle = XM_2PI * float(i) / kCarrierMarkerSides;
						const float nextAngle = XM_2PI * float(i + 1) / kCarrierMarkerSides;
						list.Line({marker.pixel.x + std::cos(angle) * radius,
									  marker.pixel.y + std::sin(angle) * radius},
							{marker.pixel.x + std::cos(nextAngle) * radius,
								marker.pixel.y + std::sin(nextAngle) * radius},
							lineWidth * kCarrierMarkerStrokeScale, frameColor);
					}

					list.Text(
						{marker.pixel.x + radius + m_tuning.layout.markerTextGap * scale, marker.pixel.y},
						m_text.Text("敵空母"), fontSize, color);
					if (snapshot.lockState == LockState::Acquiring)
						list.Circle(marker.pixel, radius + m_tuning.layout.markerTextGap * scale,
							snapshot.acquireProgress, lineWidth, color);
					if (snapshot.lockState == LockState::Locked)
						list.Text(
							{marker.pixel.x - radius,
								marker.pixel.y - radius - fontSize - m_tuning.layout.markerLabelGap * scale},
							m_text.Text("ロック完了"), fontSize, frameColor);
				} else
					DrawOffscreenIndicator(marker, color, view, list);
			}

			if (snapshot.result == CombatResult::Active &&
				(phase == MissionPhase::FinalFighters || phase == MissionPhase::Commander)) {
				const auto marker = projector.Project(snapshot.objectivePosition);

				if (marker.visible) {
					list.Circle(marker.pixel, m_tuning.layout.combatAreaMarkerRadius * scale, 1,
						lineWidth * kObjectiveMarkerStrokeScale, color);
					list.Text({marker.pixel.x + (m_tuning.layout.combatAreaMarkerRadius + kCombatAreaLabelGap) * scale,
								  marker.pixel.y},
						m_text.Text("交戦区域"), fontSize, color);
				} else
					DrawOffscreenIndicator(marker, color, view, list);
			}

			// 捕捉状況・残弾・再装填と予想着弾点をまとめて表示する
			if (phase == MissionPhase::CarrierAttack && snapshot.result == CombatResult::Active &&
				snapshot.selected != Engine::InvalidObjectId) {
				std::string lock = m_text.Text("敵空母：照準を合わせてロック");

				if (snapshot.lockState == LockState::Locked)
					lock = m_text.Text("敵空母：ロック完了");
				else if (snapshot.lockState == LockState::Acquiring)
					lock = m_text.Text("敵空母：ロック中 ") +
						std::to_string(int(snapshot.acquireProgress * kPercentPerUnitProgress)) + "%";
				drawStatus(m_tuning.layout.lockStatusY, lock);
				std::ostringstream bomb;
				bomb << m_text.Text("ミサイル ") << snapshot.carrierMissileAmmo << "/"
					 << snapshot.carrierMissileCapacity << " ";
				if (snapshot.missileCooldownSeconds <= 0)
					bomb << (snapshot.lockState == LockState::Locked ? m_text.Text(usingGamepad ? "[○]" : "[右クリック]")
																	 : m_text.Text("ロックなし"));
				else
					bomb << m_text.Text("再装填 ") << std::fixed << std::setprecision(1)
						 << snapshot.missileCooldownSeconds << m_text.Text("秒");
				drawStatus(m_tuning.layout.missileStatusY, bomb.str());

				if (snapshot.missileImpactPoint) {
					const auto impact = projector.Project(*snapshot.missileImpactPoint);

					if (impact.visible) {
						const float r = m_tuning.layout.impactMarkerRadius * scale;
						list.Line({impact.pixel.x - r, impact.pixel.y}, {impact.pixel.x + r, impact.pixel.y},
							lineWidth * kObjectiveMarkerStrokeScale, color);
						list.Line({impact.pixel.x, impact.pixel.y - r}, {impact.pixel.x, impact.pixel.y + r},
							lineWidth * kObjectiveMarkerStrokeScale, color);
					} else
						DrawOffscreenIndicator(impact, m_tuning.caution, view, list);
				}
			}
		}
	}

	void CombatHudPresenter::DrawEnemyDirection(const XMFLOAT3& target, const CombatSnapshot& snapshot,
		const Engine::RenderView& view, Engine::HudDrawList& list) const {
		constexpr float kMinimumTargetDistance = 0.001f;
		constexpr float kRearSideSwitchThreshold = 0.5f;
		constexpr float kRearDirectionBlendDepth = 0.35f;
		constexpr float kMinimumProjectedDirection = 0.001f;
		constexpr float kMaximumArrowDeltaSeconds = 0.1f;
		constexpr float kMaximumArrowAngularSpeed = 4.0f;
		constexpr float kMaximumForwardDotForReference = 0.95f;
		constexpr float kDirectionArrowOrbitRadius = 110.0f;
		constexpr float kMinimumOrbitDirectionMagnitude = 0.35f;
		constexpr float kArrowProjectionDistance = 420.0f;
		constexpr float kArrowTipLength = 64.0f;
		constexpr float kArrowTailLength = 42.0f;
		constexpr float kArrowTailHalfWidth = 17.0f;
		constexpr float kArrowNotchDepth = 23.0f;
		constexpr float kArrowRidgeDepth = 34.0f;
		constexpr float kArrowRidgeHeight = 9.0f;
		constexpr float kArrowRidgeOpacity = 0.65f;
		constexpr float kRearLabelFontSize = 24.0f;
		constexpr float kRearLabelOffset = 70.0f;
		const float scale = HudStyle::FitScale(view.width, view.height) *
			m_tuning.layout.directionArrowScale;
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);
		const auto color = m_settings.color;

		const auto worldDirection = XMLoadFloat3(&target) - XMLoadFloat3(&snapshot.muzzle);
		const auto cameraDirection = XMVector3TransformNormal(worldDirection, XMLoadFloat4x4(&view.view));
		const float distance = XMVectorGetX(XMVector3Length(cameraDirection));

		if (!std::isfinite(distance) || distance < kMinimumTargetDistance) return;

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
			if (std::abs(d.x) > kRearSideSwitchThreshold) m_rearTurnSide = d.x < 0 ? -1.0f : 1.0f;
			const float yaw = std::abs(std::atan2(d.x, d.z)) * m_rearTurnSide;
			const float pitch = std::atan2(d.y, std::hypot(d.x, d.z));
			const auto turn = XMVector3Normalize(XMVectorSet(yaw, pitch, -kRearDirectionBlendDepth, 0));
			const float blend = std::clamp(-d.z / kRearDirectionBlendDepth, 0.0f, 1.0f);
			direction = XMVector3Normalize(XMVectorLerp(direction, turn, blend));
			XMStoreFloat3(&d, direction);
		} else
			m_rearTurnSide = d.x < 0 ? -1.0f : 1.0f;

		// 方向変更時は中央の周囲を連続して回す。左右の座標を直接切り替えない
		const float lateral = std::hypot(d.x, d.y);
		const float desiredAngle = lateral > kMinimumProjectedDirection ? std::atan2(d.y, d.x) : m_arrowAngle;

		if (continuing) {
			const float dt = std::clamp(snapshot.elapsedSeconds - m_arrowSeconds, 0.0f, kMaximumArrowDeltaSeconds);
			const float difference = std::remainder(desiredAngle - m_arrowAngle, XM_2PI);
			const float maxTurn = kMaximumArrowAngularSpeed * dt;
			m_arrowAngle += std::clamp(difference, -maxTurn, maxTurn);
		} else
			m_arrowAngle = desiredAngle;
		m_arrowAngle = std::remainder(m_arrowAngle, XM_2PI);
		d.x = std::cos(m_arrowAngle) * lateral;
		d.y = std::sin(m_arrowAngle) * lateral;
		direction = XMLoadFloat3(&d);
		m_arrowTarget = snapshot.selected;
		m_arrowSeconds = snapshot.elapsedSeconds;

		// 正面・真後ろでも基底が潰れないよう、視線に直交する軸を選ぶ
		const auto reference = std::abs(d.z) < kMaximumForwardDotForReference
			? XMVectorSet(0, 0, 1, 0) : XMVectorSet(0, 1, 0, 0);
		const auto side = XMVector3Normalize(XMVector3Cross(reference, direction));
		const auto up = XMVector3Cross(direction, side);

		// 敵の方位に合わせて中央の周囲を移動する。視線軸の近くでは
		// 半径を縮め、正面を横切る際に反対側へ突然跳ばないようにする
		const float radial = kDirectionArrowOrbitRadius * scale /
			std::max(kMinimumOrbitDirectionMagnitude, std::hypot(d.x, d.y));
		const XMFLOAT2 anchor{view.width * 0.5f + d.x * radial, view.height * 0.5f - d.y * radial};
		const auto projectArrow = [&](FXMVECTOR point) {
			XMFLOAT3 p;
			XMStoreFloat3(&p, point);
			const float perspective = kArrowProjectionDistance / (kArrowProjectionDistance + p.z);
			return XMFLOAT2{anchor.x + p.x * perspective * scale, anchor.y - p.y * perspective * scale};
		};

		// 細長い山形の後端を内側へ折り込み、稜線だけで薄い立体感を出す
		// 四角錐の底面や軸線は描かず、矢印として読める輪郭を優先する
		const auto tip = projectArrow(direction * kArrowTipLength);
		const auto left = projectArrow(direction * -kArrowTailLength + side * kArrowTailHalfWidth);
		const auto right = projectArrow(direction * -kArrowTailLength - side * kArrowTailHalfWidth);
		const auto notch = projectArrow(direction * -kArrowNotchDepth);
		const auto ridge = projectArrow(direction * -kArrowRidgeDepth + up * kArrowRidgeHeight);
		const auto arrowColor = color;
		auto ridgeColor = arrowColor;
		ridgeColor.w = kArrowRidgeOpacity;
		list.Line(tip, left, lineWidth, arrowColor);
		list.Line(left, notch, lineWidth, arrowColor);
		list.Line(notch, right, lineWidth, arrowColor);
		list.Line(right, tip, lineWidth, arrowColor);
		list.Line(tip, ridge, lineWidth, ridgeColor);
		list.Line(ridge, left, lineWidth, ridgeColor);
		list.Line(ridge, right, lineWidth, ridgeColor);

		if (behind) {
			const std::string_view label = m_text.Text("後方");
			const float labelSize = kRearLabelFontSize * scale;
			list.Text({anchor.x - Engine::HudDrawList::MeasureTextWidth(label, labelSize) * 0.5f,
						  anchor.y + kRearLabelOffset * scale},
				label, labelSize, arrowColor);
		}
	}

	void CombatHudPresenter::DrawOffscreenIndicator(const HudProjection& projection, XMFLOAT4 tint,
		const Engine::RenderView& view, Engine::HudDrawList& list) const {
		constexpr float kOffscreenMarkerInset = 50.0f;
		constexpr float kOffscreenArrowLength = 20.0f;
		constexpr float kOffscreenArrowHalfWidth = 9.0f;
		const float scale = HudStyle::FitScale(view.width, view.height) *
			m_tuning.layout.offscreenIndicatorScale;
		const float lineWidth = std::max(1.0f, m_settings.lineWidth * scale);

		if (projection.visible || !std::isfinite(projection.pixel.x) || !std::isfinite(projection.pixel.y))
			return;

		const float centerX = view.width * 0.5f;
		const float centerY = view.height * 0.5f;
		float deltaX = projection.pixel.x - centerX;
		float deltaY = projection.pixel.y - centerY;

		constexpr float kMinimumOffscreenDirectionLength = 0.001f;
		if (std::hypot(deltaX, deltaY) < kMinimumOffscreenDirectionLength) deltaY = 1;

		// 対象の方向を保ったまま矢印を画面の内縁へ収める
		const float ratio = 1 /
			std::max(std::abs(deltaX) / std::max(1.0f, centerX - kOffscreenMarkerInset * scale),
				std::abs(deltaY) / std::max(1.0f, centerY - kOffscreenMarkerInset * scale));
		const XMFLOAT2 tip{centerX + deltaX * ratio, centerY + deltaY * ratio};
		const float length = std::hypot(deltaX, deltaY);

		// 対象の方向を基準に矢印の両翼を描く
		const float unitX = deltaX / length;
		const float unitY = deltaY / length;
		list.Line(tip,
			{tip.x - unitX * kOffscreenArrowLength * scale - unitY * kOffscreenArrowHalfWidth * scale,
				tip.y - unitY * kOffscreenArrowLength * scale + unitX * kOffscreenArrowHalfWidth * scale},
			lineWidth, tint);
		list.Line(tip,
			{tip.x - unitX * kOffscreenArrowLength * scale + unitY * kOffscreenArrowHalfWidth * scale,
				tip.y - unitY * kOffscreenArrowLength * scale - unitX * kOffscreenArrowHalfWidth * scale},
			lineWidth, tint);
	}
} // namespace Game
