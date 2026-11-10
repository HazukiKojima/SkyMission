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
	Require(missing.briefing.size() == 3 && missing.formationOffsets.size() == 4 &&
			missing.originOffsetMeters.z == -5000 && missing.audio.bgmVolume == 0.12f,
		"Missing opening configuration changed legacy content");
	const auto configured = fixture / L"opening-data.json";
	std::ofstream(configured) << R"({"briefing":[
		{"lines":["First","Second"],"audio":"../音声/会話.wav","fallbackSeconds":0.5},
		{"lines":["Third","Fourth"],"audio":"","fallbackSeconds":0.25}],
		"formationOffsets":[[0,0,0],[2,1,-3]],"originOffsetMeters":[10,20,-30],
		"audio":{"bgm":"../音声/曲.wav","bgmVolume":0.2,"briefingVolume":0.6,"flightVolumeScale":0.3}})";
	const auto data = LoadOpeningCinematicSettings(reader, configured);
	Require(data.briefing.size() == 2 && data.briefing[0].lines[1] == "Second" &&
			data.briefing[0].audio == configured.parent_path() / L"../音声/会話.wav" &&
			data.briefing[1].audio.empty() && data.formationOffsets[1].z == -3 &&
			data.originOffsetMeters.x == 10 && data.audio.bgmVolume == 0.2f &&
			data.audio.bgm == configured.parent_path() / L"../音声/曲.wav",
		"Opening content or relative Japanese audio paths were not read");
	OpeningCinematicController briefingController;
	briefingController.Start(data);
	briefingController.EnableBriefing();
	Require(briefingController.GetPresentation().briefingLines == data.briefing[0].lines,
		"Briefing presentation did not use its configured subtitles");
	briefingController.UpdateBriefing(OpeningVoiceStatus::Unavailable, 0.1f);
	briefingController.Update(0.25f, false);
	Require(
		briefingController.GetPresentation().elapsedSeconds == 0, "Cinematic clock advanced during briefing");
	for (int i = 0; i < 5; ++i)
		briefingController.UpdateBriefing(OpeningVoiceStatus::Playing, 0.25f);
	briefingController.UpdateBriefing(OpeningVoiceStatus::Unavailable, 0);
	Require(
		briefingController.GetPresentation().briefingLine == 0, "Playback or suspension advanced briefing");
	briefingController.UpdateBriefing(OpeningVoiceStatus::Finished, 0.1f);
	Require(briefingController.GetPresentation().briefingLine == 1 &&
			briefingController.GetPresentation().briefingLines == data.briefing[1].lines,
		"Audio completion did not advance to matching subtitles");
	briefingController.UpdateBriefing(OpeningVoiceStatus::Unavailable, 0.1f);
	briefingController.UpdateBriefing(OpeningVoiceStatus::Unavailable, 0.25f);
	Require(briefingController.GetPresentation().briefingLine == -1,
		"Silent briefing did not use fallback duration");
	briefingController.Start(data);
	briefingController.EnableBriefing();
	briefingController.Update(0.1f, true);
	Require(!briefingController.GetPresentation().active &&
			briefingController.GetPresentation().briefingLine == -1,
		"Skip retained a briefing");
	briefingController.Start(data);
	briefingController.EnableBriefing();
	Require(
		briefingController.GetPresentation().briefingLine == 0, "Restart did not restore the first briefing");
	std::ofstream(configured) << R"({"briefing":[]})";
	briefingController.Start(LoadOpeningCinematicSettings(reader, configured));
	briefingController.EnableBriefing();
	Require(briefingController.GetPresentation().briefingLine == -1,
		"Empty briefing did not bypass conversation");
	const auto invalid = fixture / L"opening-invalid.json";
	for (const auto json :
		{"{\"cutSeconds\":2}", "{\"verticalFovDegrees\":180}", "{\"cameraStartOffsetMeters\":[0,1]}",
			"{\"title\":7}", "{broken", R"({"formationOffsets":[]})", R"({"formationOffsets":[[0,1]]})",
			R"({"originOffsetMeters":[0,1]})", R"({"audio":7})", R"({"audio":{"bgmVolume":1.1}})",
			R"({"audio":{"briefingVolume":-1}})", R"({"audio":{"flightVolumeScale":"bad"}})",
			R"({"briefing":{}})", R"({"briefing":[{"lines":["a"],"audio":"","fallbackSeconds":1}]})",
			R"({"briefing":[{"lines":["a",7],"audio":"","fallbackSeconds":1}]})",
			R"({"briefing":[{"lines":["","b"],"audio":"","fallbackSeconds":1}]})",
			R"({"briefing":[{"lines":["a","b"],"audio":"","fallbackSeconds":0}]})"}) {
		std::ofstream(invalid) << json;
		bool rejected = false;
		try {
			LoadOpeningCinematicSettings(reader, invalid);
		} catch (const std::exception& error) {
			rejected = std::string(error.what()).find("opening-invalid.json") != std::string::npos;
		}
		Require(rejected, "Invalid opening JSON did not identify its source");
	}
	for (const auto& json : {std::string("{\"formationOffsets\":[") +
				 [] {
					 std::string entries;
					 for (int i = 0; i < 33; ++i)
						 entries += (i ? "," : "") + std::string("[0,0,0]");
					 return entries;
				 }() +
				 "]}",
			 std::string("{\"briefing\":[") +
				 [] {
					 std::string entries;
					 for (int i = 0; i < 65; ++i)
						 entries += (i ? "," : "") + std::string("{}");
					 return entries;
				 }() +
				 "]}",
			 std::string(R"({"briefing":[{"lines":[")") + std::string(193, 'x') +
				 R"(","b"],"audio":"","fallbackSeconds":1}]})"}) {
		std::ofstream(invalid) << json;
		bool rejected = false;
		try {
			LoadOpeningCinematicSettings(reader, invalid);
		} catch (const std::exception& error) {
			rejected = std::string(error.what()).find("opening-invalid.json") != std::string::npos;
		}
		Require(rejected, "Opening collection/text limits were not validated with source context");
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
	for (int line = 0; line < 3; ++line) {
		auto presentation = controller.GetPresentation();
		presentation.briefingLine = line;
		presentation.briefingLines = settings.briefing[line].lines;
		Engine::RenderView view;
		view.width = 800; view.height = 600;
		Engine::HudDrawList list;
		list.Reset(view.width, view.height);
		OpeningCinematicPresenter{}.Build(presentation, view, list);
		Require(list.GetVertices().size() > 6, "Briefing lost its background or subtitles");
		for (const auto& vertex : list.GetVertices())
			Require(std::abs(vertex.position.x) <= 1.001f && std::abs(vertex.position.y) <= 1.001f,
				"Briefing subtitle escaped the viewport");
	}
	std::string_view briefing = "敵母艦と護衛機を確認。基地を襲った部隊の発進元だ。基地は現在も攻撃を受けており、増援は出せない。君、一機で護衛機を排除し、母艦を撃沈せよ。";
	while (!briefing.empty())
		Require(Engine::HudFont::Glyph(Engine::HudFont::Next(briefing)) != unsigned('?' - 32),
			"Briefing uses an unsupported glyph");
	std::cout << "PASS: opening defaults, partial/invalid JSON, timing, fades, skip, viewport and glyphs\n";
}
