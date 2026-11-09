#pragma once
#include "Game/Cinematics/OpeningCinematicController.h"
#include "Engine/Resources/JsonAssetReader.h"
#include "Game/UI/OpeningCinematicPresenter.h"

void TestOpeningCinematic(const fs::path& repository, const fs::path& fixture) {
	using namespace Game;
	Engine::JsonAssetReader reader;
	const auto missing = LoadOpeningCinematicSettings(reader, fixture / L"no-opening.json");
	Require(missing.durationSeconds == 5, "Missing opening configuration lost defaults");
	const auto partial = fixture / L"opening-partial.json";
	std::ofstream(partial) << "{\"flightSpeedMetersPerSecond\":210}";
	Require(LoadOpeningCinematicSettings(reader, partial).flightSpeedMetersPerSecond == 210,
		"Partial opening configuration did not preserve defaults");
	const auto invalid = fixture / L"opening-invalid.json";
	for (const auto json : {"{\"cutSeconds\":2}", "{\"verticalFovDegrees\":180}",
			 "{\"cameraStartOffsetMeters\":[0,1]}", "{\"title\":7}", "{broken"}) {
		std::ofstream(invalid) << json;
		bool rejected = false;
		try {
			LoadOpeningCinematicSettings(reader, invalid);
		} catch (const std::exception& error) {
			rejected = std::string(error.what()).find("opening-invalid.json") != std::string::npos;
		}
		Require(rejected, "Invalid opening JSON did not identify its source");
	}
	const auto settings =
		LoadOpeningCinematicSettings(reader, repository / L"Assets/Cinematics/opening.json");
	OpeningCinematicController controller;
	controller.Start(settings);
	controller.Update(3.6f, false);
	Require(controller.GetPresentation().formationShot && controller.GetPresentation().fadeOpacity == 0,
		"Opening fade began before its configured time");
	controller.Update(0.2f, false);
	Require(!controller.GetPresentation().formationShot && controller.GetPresentation().fadeOpacity > 0.999f,
		"Opening did not switch cameras under full black");
	controller.Update(0.2f, false);
	Require(controller.GetPresentation().fadeOpacity > 0.999f, "Opening black hold was not preserved");
	controller.Update(0.7f, false);
	Require(controller.GetPresentation().active && controller.GetPresentation().fadeOpacity < 0.001f,
		"Opening did not reveal the normal camera");
	controller.Update(0.3f, false);
	Require(!controller.GetPresentation().active, "Opening did not finish at five seconds");
	controller.Start(settings);
	controller.Update(1, true);
	controller.Update(1, true);
	Require(!controller.GetPresentation().active && controller.GetPresentation().fadeOpacity == 0,
		"Repeated opening skip restarted or retained a fade");
	controller.Start(settings);
	controller.Update(std::numeric_limits<float>::quiet_NaN(), false);
	Require(controller.GetPresentation().elapsedSeconds == 0, "Invalid delta advanced opening time");
	for (const auto size :
		{DirectX::XMUINT2{1920, 1080}, DirectX::XMUINT2{2560, 1080}, DirectX::XMUINT2{800, 600}}) {
		Engine::RenderView view;
		view.width = size.x;
		view.height = size.y;
		Engine::HudDrawList list;
		list.Reset(size.x, size.y);
		OpeningCinematicPresenter{}.Build(controller.GetPresentation(), view, list);
		Require(!list.GetVertices().empty(), "Opening overlay was empty");
		for (const auto& vertex : list.GetVertices())
			Require(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) &&
					std::abs(vertex.position.x) <= 1.001f && std::abs(vertex.position.y) <= 1.001f,
				"Opening overlay escaped the viewport");
		for (const auto& text : {settings.title, settings.subtitle, std::string("Enter：スキップ")}) {
			std::string_view remaining = text;
			while (!remaining.empty())
				Require(Engine::HudFont::Glyph(Engine::HudFont::Next(remaining)) != unsigned('?' - 32),
					"Opening label uses an unsupported glyph");
		}
	}
	std::cout << "PASS: opening defaults, partial/invalid JSON, timing, fades, skip, viewport and glyphs\n";
}
