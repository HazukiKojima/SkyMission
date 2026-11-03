#pragma once
#include "Engine/Renderer/Hud/HudFont.h"
#include "Game/UI/HudStyle.h"
#include "Game/UI/CombatHudPresenter.h"

void TestHudTypography() {
	Engine::HudDrawList list;
	list.Reset(1920, 1080);
	list.Text({100, 100}, "速度 1122", 48, Game::HudStyle::Ink);
	Require(list.GetVertices().size() == 42, "UTF-8 HUD text was rendered per byte");
	float right = 0;
	for (const auto& vertex : list.GetVertices())
		right = std::max(right, (vertex.position.x + 1) * 960);
	Require(std::abs(right - 100 - Engine::HudDrawList::MeasureTextWidth("速度 1122", 48)) < 0.001f,
		"Japanese/Latin text measurement differs from the rendered bounds");
	Require(Engine::HudDrawList::MeasureTextWidth("", 48) == 0, "Empty text has a width");
	Require(Engine::HudDrawList::MeasureTextWidth("\xf0\x9f\x98\x80", 48) ==
			Engine::HudDrawList::MeasureTextWidth("?", 48),
		"Unsupported Unicode was replaced per byte");
	for (std::string_view text :
		{std::string_view{"\xed\xa0\x80"}, std::string_view{"\xe9\x80"}, std::string_view{"\xff"}}) {
		list.Reset(1920, 1080);
		list.Text({}, text, 48, Game::HudStyle::Ink);
		Require(!list.GetVertices().empty() && list.GetVertices().size() <= text.size() * 6,
			"Invalid UTF-8 did not produce bounded replacement glyphs");
		for (const auto& vertex : list.GetVertices())
			Require(vertex.uv.x >= 0 && vertex.uv.x <= 1 && vertex.uv.y >= 0 && vertex.uv.y <= 1,
				"Invalid UTF-8 escaped the atlas");
	}
	std::string_view labels = "敵機接近照準速度高度北機体ロケット使用可秒任務達成自機撃墜R：再挑戦被弾ミサイ"
							  "ル接近目標なし射程外命中全目標撃墜▶";
	while (!labels.empty()) {
		const auto character = Engine::HudFont::Next(labels);
		Require(character < 128 || Engine::HudFont::Glyph(character) >= Engine::HudFont::AsciiCount,
			"A Japanese UI label is missing from the font atlas repertoire");
	}
	list.Reset(1920, 1080);
	list.FilledRect({10, 20}, {100, 50}, Game::HudStyle::Ink);
	Require(list.GetVertices().size() == 6, "Filled rectangle did not produce one quad");
	for (const auto& vertex : list.GetVertices())
		Require(vertex.uv.x == Engine::HudFont::SolidUv.x && vertex.uv.y == Engine::HudFont::SolidUv.y,
			"HUD shape UV no longer points to the shared solid texel");
	Engine::RenderView view;
	view.width = 1920;
	view.height = 1080;
	list.Reset(view.width, view.height);

	Game::CombatSnapshot snapshot;
	snapshot.playerSpeedMetersPerSecond = 100;
	snapshot.playerAltitudeMeters = 1500;
	DirectX::XMStoreFloat4x4(&view.view, DirectX::XMMatrixIdentity());
	DirectX::XMStoreFloat4x4(
		&view.projection, DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4, 1920.0f / 1080, 1, 10000));
	Game::CombatHudPresenter{}.Build(snapshot, view, list);
	for (const auto& vertex : list.GetVertices()) {
		const float x = (vertex.position.x + 1) * 960, y = (1 - vertex.position.y) * 540;
		Require(
			x > 16 && x < 1904 && y > 16 && y < 1064, "Flight HUD retained the weathered screen-edge frame");
	}
	snapshot.forward = {0, 0, 1};
	snapshot.enemies = {{1, {0, 0, 1000}, {}, "ENEMY 01", 100, {0, 0, 1}}};
	snapshot.selected = 1;
	snapshot.rocket = {true, {0, 2.4f}, 0};
	snapshot.hitSeconds = snapshot.playerHitSeconds = 0.1f;
	snapshot.playerHitDirection = {1, 0, 0};
	snapshot.destroyedSeconds = snapshot.allDownSeconds = 1;
	for (auto result : {Game::CombatResult::Active, Game::CombatResult::Victory, Game::CombatResult::Defeat})
		for (auto warning :
			{Game::ThreatWarning::None, Game::ThreatWarning::LockOn, Game::ThreatWarning::Missile}) {
			snapshot.result = result;
			snapshot.threatWarning = warning;
			snapshot.lockState = Game::LockState::Locked;
			list.Reset(view.width, view.height);
			Game::CombatHudPresenter{}.Build(snapshot, view, list);
			for (size_t i = 0; i < list.GetVertices().size(); i += 6) {
				const auto uv = list.GetVertices()[i].uv;
				if (uv.x == Engine::HudFont::SolidUv.x && uv.y == Engine::HudFont::SolidUv.y)
					continue;
				const auto column =
					unsigned(std::lround(uv.x * Engine::HudFont::Width)) / Engine::HudFont::Cell;
				const auto row =
					unsigned(std::lround(uv.y * Engine::HudFont::Height)) / Engine::HudFont::Cell;
				Require(row * Engine::HudFont::Columns + column < Engine::HudFont::AsciiCount,
					"A built-in flight HUD or radar label is not English/ASCII");
			}
		}
	const auto ink = Engine::HudSettings{}.color;
	Require(ink.x == Game::HudStyle::Ink.x && ink.y == Game::HudStyle::Ink.y &&
			ink.z == Game::HudStyle::Ink.z && ink.w == Game::HudStyle::Ink.w,
		"English HUD changed the current parchment palette");
	std::cout
		<< "PASS: HUD typography, UTF-8 fallback, English labels, preserved palette and undecorated frame\n";
}
