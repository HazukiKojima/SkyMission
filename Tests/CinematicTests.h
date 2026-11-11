#pragma once
#include "Game/Cinematics/CinematicController.h"
#include "Engine/Resources/JsonAssetReader.h"
#include "Game/UI/CinematicPresenter.h"

void TestCinematic(const fs::path& repository, const fs::path& fixture) {
	using namespace Game;
	Engine::JsonAssetReader reader;
	const auto missing = LoadCinematicSettings(reader, fixture / L"no-opening.json");
	Require(missing.durationSeconds == 5, "Missing opening configuration lost defaults");
	const auto partial = fixture / L"opening-partial.json";
	std::ofstream(partial) << "{\"flightSpeedMetersPerSecond\":210}";
	Require(LoadCinematicSettings(reader, partial).flightSpeedMetersPerSecond == 210,
		"Partial opening configuration did not preserve defaults");
	Require(missing.briefing.size() == 3 && missing.formationOffsets.size() == 4 &&
			missing.originOffsetMeters.z == -5000 && missing.audio.bgmVolume == 0.12f,
		"Missing opening configuration changed legacy content");
	const auto configured = fixture / L"opening-data.json";
	std::ofstream(configured) << R"({"briefing":[
		{"lines":["First","Second"],"audio":"../音声/会話.wav","fallbackSeconds":0.5},
		{"lines":["Third","Fourth"],"audio":"","fallbackSeconds":0.25}],
		"formationOffsets":[[0,0,0],[2,1,-3]],"originOffsetMeters":[10,20,-30],
		"radio":[{"speaker":"Commander","text":"Incoming!","side":"friendly","audio":"../音声/無線.wav","startSeconds":0.5,"durationSeconds":2}],
		"audio":{"bgm":"../音声/曲.wav","bgmVolume":0.2,"briefingVolume":0.6,"flightVolumeScale":0.3}})";
	const auto data = LoadCinematicSettings(reader, configured);
	Require(data.briefing.size() == 2 && data.briefing[0].lines[1] == "Second" &&
			data.briefing[0].audio == configured.parent_path() / L"../音声/会話.wav" &&
			data.briefing[1].audio.empty() && data.formationOffsets[1].z == -3 &&
			data.originOffsetMeters.x == 10 && data.audio.bgmVolume == 0.2f &&
			data.audio.bgm == configured.parent_path() / L"../音声/曲.wav" &&
			data.radio.size() == 1 && data.radio[0].audio == configured.parent_path() / L"../音声/無線.wav",
		"Opening content or relative Japanese audio paths were not read");
	CinematicController briefingController;
	briefingController.Start(data);
	briefingController.EnableBriefing();
	Require(briefingController.GetPresentation().briefingLines == data.briefing[0].lines,
		"Briefing presentation did not use its configured subtitles");
	briefingController.UpdateBriefing(CinematicVoiceStatus::Unavailable, 0.1f);
	briefingController.Update(0.25f, false);
	Require(
		briefingController.GetPresentation().elapsedSeconds == 0, "Cinematic clock advanced during briefing");
	for (int i = 0; i < 5; ++i)
		briefingController.UpdateBriefing(CinematicVoiceStatus::Playing, 0.25f);
	briefingController.UpdateBriefing(CinematicVoiceStatus::Unavailable, 0);
	Require(
		briefingController.GetPresentation().briefingLine == 0, "Playback or suspension advanced briefing");
	briefingController.UpdateBriefing(CinematicVoiceStatus::Finished, 0.1f);
	Require(briefingController.GetPresentation().briefingLine == 1 &&
			briefingController.GetPresentation().briefingLines == data.briefing[1].lines,
		"Audio completion did not advance to matching subtitles");
	briefingController.UpdateBriefing(CinematicVoiceStatus::Unavailable, 0.1f);
	briefingController.UpdateBriefing(CinematicVoiceStatus::Unavailable, 0.25f);
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
	briefingController.Start(LoadCinematicSettings(reader, configured));
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
			LoadCinematicSettings(reader, invalid);
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
			LoadCinematicSettings(reader, invalid);
		} catch (const std::exception& error) {
			rejected = std::string(error.what()).find("opening-invalid.json") != std::string::npos;
		}
		Require(rejected, "Opening collection/text limits were not validated with source context");
	}
	const auto settings = LoadCinematicSettings(reader, repository / L"Assets/Cinematics/opening.json");
	Require(settings.radio.size() == 1 && settings.radio[0].caption.speaker == "指揮官" &&
		settings.radio[0].caption.text == "敵機4機、接近！直ちに迎撃せよ！" && fs::exists(settings.radio[0].audio),
		"Opening radio caption or voice configuration was lost");
	for (const auto& text : {settings.radio[0].caption.speaker, settings.radio[0].caption.text, std::string("≪≫")}) {
		std::string_view remaining = text;
		while (!remaining.empty()) Require(Engine::HudFont::Glyph(Engine::HudFont::Next(remaining)) != unsigned('?' - 32),
			"Radio caption uses an unsupported glyph");
	}
	CinematicController radioController;
	radioController.Start(settings);
	Require(radioController.GetPresentation().radioLine == -1, "Radio began during the movie");
	radioController.Update(4.9f, false);
	Require(radioController.GetPresentation().active && radioController.GetPresentation().radioLine == -1,
		"Radio began before the movie ended");
	radioController.Update(0.1f, false);
	Require(!radioController.GetPresentation().active && radioController.GetPresentation().radioLine == 0,
		"Radio did not start when control returned");
	Require(radioController.TakeCompletion() == CinematicCompletion::Completed, "Movie did not emit completion");
	radioController.Update(3.5f, false);
	Require(radioController.GetPresentation().radioLine == 0, "Post-movie radio ended early");
	radioController.Update(0.2f, false);
	Require(radioController.GetPresentation().radioLine == -1, "Post-movie radio did not expire");
	radioController.Start(settings);
	radioController.EnableBriefing();
	Require(radioController.GetPresentation().radioLine == -1, "Radio overlapped the briefing");
	radioController.Start(settings);
	radioController.Update(0, true);
	Require(!radioController.GetPresentation().active && radioController.GetPresentation().radioLine == 0,
		"Skipping the movie did not start the post-movie call");
	radioController.Start(settings);
	Require(radioController.GetPresentation().radioLine == -1, "Replay retained post-movie radio");
	CinematicController controller;
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
		CinematicPresenter{}.Build(controller.GetPresentation(), view, list);
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
		view.width = 800;
		view.height = 600;
		Engine::HudDrawList list;
		list.Reset(view.width, view.height);
		CinematicPresenter{}.Build(presentation, view, list);
		Require(list.GetVertices().size() > 6, "Briefing lost its background or subtitles");
		for (const auto& vertex : list.GetVertices())
			Require(std::abs(vertex.position.x) <= 1.001f && std::abs(vertex.position.y) <= 1.001f,
				"Briefing subtitle escaped the viewport");
	}
	std::string_view briefing = "敵母艦と護衛機を確認。基地を襲った部隊の発進元だ。基地は現在も攻撃を受けてお"
								"り、増援は出せない。君、一機で護衛機を排除し、母艦を撃沈せよ。";
	while (!briefing.empty())
		Require(Engine::HudFont::Glyph(Engine::HudFont::Next(briefing)) != unsigned('?' - 32),
			"Briefing uses an unsupported glyph");
	{
		auto presentation = controller.GetPresentation();
		presentation.radioLine = 0;
		presentation.radio = settings.radio[0].caption;
		Engine::RenderView view;
		view.width = 800; view.height = 600;
		Engine::HudDrawList friendly, enemy;
		friendly.Reset(800, 600); enemy.Reset(800, 600);
		CinematicPresenter{}.Build(presentation, view, friendly);
		presentation.radio.side = "enemy";
		CinematicPresenter{}.Build(presentation, view, enemy);
		Require(friendly.GetVertices().size() == enemy.GetVertices().size(), "Radio side changed layout");
		bool differentColor = false;
		for (size_t i = 0; i < enemy.GetVertices().size(); ++i) {
			const auto& vertex = enemy.GetVertices()[i];
			Require(std::abs(vertex.position.x) <= 1.001f && std::abs(vertex.position.y) <= 1.001f,
				"Radio subtitle escaped a narrow viewport");
			differentColor |= vertex.color.x != friendly.GetVertices()[i].color.x;
		}
		Require(differentColor, "Enemy radio did not use its own color");
	}
	const auto middle = LoadCinematicSettings(reader, repository / L"Assets/Cinematics/midMission.json");
	const auto reinforcements = LoadCinematicSettings(reader, repository / L"Assets/Cinematics/reinforcements.json");
	Require(reinforcements.TotalDuration() == 3 && reinforcements.briefing.empty() &&
		reinforcements.model == "enemy" && reinforcements.formationOffsets.size() == 4,
		"Reinforcement cinematic did not introduce four enemy aircraft for three seconds");
	Require(middle.shots.size() == 1 && middle.briefing.empty() && middle.TotalDuration() == 3 &&
		middle.shots[0].target == "carrier",
		"Three-second carrier cinematic definition was not loaded");
	Require(middle.radio.size() == 1 && middle.radio[0].caption.speaker == "司令官" &&
		middle.radio[0].caption.text == "次は敵母艦だ。爆撃で敵機の発進を阻止せよ！" && fs::exists(middle.radio[0].audio),
		"Carrier command caption or voice asset was not configured");
	for (const auto& text : {middle.radio[0].caption.speaker, middle.radio[0].caption.text}) {
		std::string_view remaining = text;
		while (!remaining.empty()) Require(Engine::HudFont::Glyph(Engine::HudFont::Next(remaining)) != unsigned('?' - 32),
			"Carrier command used an unsupported glyph");
	}
	CinematicController carrierVoice;
	carrierVoice.Start(middle, "midMission");
	carrierVoice.EnableBriefing();
	Require(carrierVoice.GetPresentation().radioLine == 0, "Empty briefing hid the carrier command");
	carrierVoice.Update(3, false);
	Require(!carrierVoice.GetPresentation().active && carrierVoice.GetPresentation().radioLine == 0,
		"Carrier movie did not end at three seconds while preserving its command voice");
	carrierVoice.Update(1.7f, false);
	Require(carrierVoice.GetPresentation().radioLine == -1, "Carrier command did not expire");
	carrierVoice.Start(middle, "midMission");
	carrierVoice.Update(1, false);
	carrierVoice.Update(0, true);
	Require(carrierVoice.GetPresentation().radioLine == -1, "Skipping the carrier movie retained its voice");
	carrierVoice.StartRadio(middle, "midMission");
	Require(!carrierVoice.GetPresentation().active && carrierVoice.GetPresentation().radioLine == 0 &&
		!carrierVoice.TakeCompletion(), "Gameplay-only command started a movie or emitted completion");
	carrierVoice.Update(5, false);
	Require(carrierVoice.GetPresentation().radioLine == -1, "Gameplay-only command did not expire");
	for (const auto* id : {"patrolEncouragement", "patrolComplete", "carrierHit", "carrierComplete"}) {
		const auto command = LoadCinematicSettings(reader, repository / L"Assets/Cinematics" / (std::string(id) + ".json"));
		Require(command.radio.size() == 1 && fs::exists(command.radio[0].audio), "Patrol command voice was missing");
		for (const auto& text : {command.radio[0].caption.speaker, command.radio[0].caption.text}) {
			std::string_view remaining = text;
			while (!remaining.empty()) Require(Engine::HudFont::Glyph(Engine::HudFont::Next(remaining)) != unsigned('?' - 32),
				"Patrol command used an unsupported Japanese glyph");
		}
	}
	CinematicController timeline;
	auto multi = middle;
	multi.shots = {CinematicShotSettings{}, CinematicShotSettings{}};
	for (auto& shot : multi.shots) shot.durationSeconds = 2;
	timeline.Start(multi, "midMission");
	timeline.Update(2.1f, false);
	Require(timeline.GetPresentation().active && timeline.GetPresentation().shotIndex == 1 &&
			timeline.GetPresentation().cinematicId == "midMission" &&
			timeline.GetPresentation().formationShot &&
			std::abs(timeline.GetPose().travelMeters - 18) < 0.01f,
		"Shot crossing did not preserve local elapsed time");
	timeline.Update(2, false);
	Require(!timeline.GetPresentation().active &&
			timeline.TakeCompletion() == CinematicCompletion::Completed && !timeline.TakeCompletion(),
		"Cinematic did not emit exactly one completion");
	timeline.Start(middle, "other");
	timeline.Update(0, true);
	Require(timeline.TakeCompletion() == CinematicCompletion::Skipped && !timeline.TakeCompletion(),
		"Skip completion was missing or repeated");
	timeline.Start(middle);
	timeline.Update(10, false);
	Require(!timeline.GetPresentation().active, "Large frame did not finish the complete shot sequence");
	for (const auto* json : {R"({"radio":[{"speaker":"C","text":"Incoming","side":"invalid"}]})",
		 R"({"radio":[{"speaker":"C","text":"Incoming","durationSeconds":-1}]})",
		 R"({"radio":[{"speaker":"C","text":"Incoming","startSeconds":-1,"durationSeconds":2}]})",
		 R"({"shots":[]})", R"({"shots":[{"target":"unknown"}]})", R"({"shots":[{"durationSeconds":0}]})",
			 R"({"shots":[{"model":""}]})", R"({"shots":[{"formationOffsets":[]}]})",
			 R"({"shots":[{"cameraStartOffsetMeters":[0,100,-50],"cameraEndOffsetMeters":[0,80,-50]}]})"}) {
		std::ofstream(invalid) << json;
		bool rejected = false;
		try {
			LoadCinematicSettings(reader, invalid);
		} catch (const std::exception& error) {
			rejected = std::string(error.what()).find("opening-invalid.json") != std::string::npos;
		}
		Require(rejected, "Invalid shot definition was accepted or lost its path");
	}
	std::cout
		<< "PASS: cinematic defaults, JSON, subtitles, timelines, shot crossings and completion events\n";
}
