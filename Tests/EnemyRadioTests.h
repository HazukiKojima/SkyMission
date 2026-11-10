#pragma once
#include "Game/Combat/EnemyRadio.h"
#include "Engine/Renderer/Hud/HudFont.h"

void TestEnemyRadio() {
	using namespace Game;
	const std::array<Engine::ObjectId, 1> survivors{42};
	EnemyRadioController radio;
	std::array<bool, EnemyRadioClips.size()> heard{};
	for (int kill = 0; kill < 7; ++kill) {
		radio.NotifyDestroyed(100 + kill, survivors);
		radio.Update(0, survivors, false);
		const auto death = radio.GetPresentation();
		Require(death.clip >= 0 && death.caption.side == "enemy" && death.caption.speaker == "敵機",
			"Enemy death did not create an enemy caption");
		heard[death.clip] = true;
		radio.Update(4, survivors, false);
		const auto reaction = radio.GetPresentation();
		Require(reaction.clip >= 0 && reaction.serial != death.serial && reaction.caption.speaker == "敵僚機",
			"Living wingman did not react after the death call");
		heard[reaction.clip] = true;
		radio.Update(4, survivors, false);
	}
	Require(std::all_of(heard.begin(), heard.end(), [](bool value) { return value; }),
		"Enemy radio did not use all eleven voice clips");
	for (const auto& clip : EnemyRadioClips) {
		std::string_view text = clip.text;
		while (!text.empty()) Require(Engine::HudFont::Glyph(Engine::HudFont::Next(text)) != unsigned('?' - 32),
			"Enemy Japanese subtitle contains an unsupported glyph");
	}
	radio.NotifyDestroyed(1, {});
	radio.Update(0, {}, false);
	Require(radio.GetPresentation().clip >= 0, "Final enemy did not call out");
	radio.Update(4, {}, false);
	Require(radio.GetPresentation().clip == -1, "Dead wingman responded to the final kill");
	radio.NotifyDestroyed(1, survivors);
	radio.Update(0, survivors, false);
	radio.Update(4, {}, false);
	Require(radio.GetPresentation().clip == -1, "Missing wingman remained queued");
	for (int kill = 0; kill < 50; ++kill) radio.NotifyDestroyed(100 + kill, survivors);
	radio.Update(0, survivors, false);
	const auto serial = radio.GetPresentation().serial;
	radio.Update(0.1f, survivors, false);
	Require(radio.GetPresentation().serial == serial, "Radio restarted every frame");
	radio.Update(4, survivors, false);
	radio.Update(4, survivors, false);
	Require(radio.GetPresentation().clip == -1, "Rapid kills created a stale dialogue backlog");
	radio.NotifyDestroyed(1, survivors);
	radio.Update(0, survivors, true);
	Require(radio.GetPresentation().clip == -1, "Enemy radio overlapped command briefing");
	radio = {};
	radio.Update(0, survivors, false);
	Require(radio.GetPresentation().clip == -1, "Retry retained enemy dialogue");
	std::cout << "PASS: enemy death and wingman radio, eleven clips, Japanese glyphs, priority and bounded queue\n";
}
