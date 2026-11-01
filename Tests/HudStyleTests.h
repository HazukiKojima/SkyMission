#pragma once
#include "Engine/Renderer/Hud/HudFont.h"
#include "Game/UI/HudStyle.h"

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
	Game::HudStyle::DrawFrame(view, list, Game::HudStyle::Ink);
	const auto frame = list.GetVertices();
	list.Reset(view.width, view.height);
	Game::HudStyle::DrawFrame(view, list, Game::HudStyle::Ink);
	Require(frame.size() == list.GetVertices().size() && frame.size() < 1500,
		"Weathered border has an unstable or excessive vertex count");
	for (size_t i = 0; i < frame.size(); ++i)
		Require(frame[i].position.x == list.GetVertices()[i].position.x &&
				frame[i].position.y == list.GetVertices()[i].position.y &&
				std::isfinite(frame[i].position.x) && std::isfinite(frame[i].position.y),
			"Weathered border changed between otherwise identical frames");
	std::cout << "PASS: Japanese HUD typography, width, UTF-8 fallback, solid UV and deterministic border\n";
}
