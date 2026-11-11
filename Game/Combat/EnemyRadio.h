#pragma once
#include "Game/Cinematics/CinematicTypes.h"
#include "Engine/Scene/Scene.h"
#include <algorithm>
#include <array>
#include <deque>
#include <span>

namespace Game {
	struct EnemyRadioClip {
		const wchar_t* file;
		const char* text;
		float seconds;
	};
	inline constexpr std::array<EnemyRadioClip, 11> EnemyRadioClips{{
		{L"It's on fire! Aaah!.wav", "燃えてる！うわああっ！", 2.6f},
		{L"Mayday! Mayday! I'm going down!.wav", "メーデー！メーデー！墜落する！", 3.15f},
		{L"No way! Not by this guy!.wav", "嘘だろ、こいつにやられるなんて！", 2.85f},
		{L"Oh, shi—!.wav", "くそっ、しまっ……！", 1.75f},
		{L"Stay away! GET AWAY FROM ME!.wav", "来るな！こっちに来るなあっ！", 2.85f},
		{L"That's no pilot… That's a monster!.wav", "あいつはパイロットじゃない……化け物だ！", 3.1f},
		{L"Too fast! I can't shake him!.wav", "速すぎる！振り切れない！", 2.8f},
		{L"Aaah! No!.wav", "うわあっ！やめろ！", 1.9f},
		{L"Engine's gone. Ejecting..wav", "エンジンがやられた。脱出する！", 2.6f},
		{L"I don't wanna die! Please—!.wav", "死にたくない！頼む、やめ……！", 2.6f},
		{L"I'm hit! What do I do!.wav", "被弾した！どうすればいい！？", 2.4f}
	}};
	struct EnemyRadioPresentation {
		int clip = -1;
		uint64_t serial = 0;
		RadioCaption caption;
	};
	// One speech channel; keep only the latest waiting exchange during a kill streak.
	class EnemyRadioController {
	  public:
		void NotifyDestroyed(Engine::ObjectId victim, std::span<const Engine::ObjectId> survivors) {
			if (m_current.reaction && m_current.actor == victim) StopCurrent();
			m_pending.clear();
			constexpr std::array deaths{0, 1, 3, 7, 8, 9, 10};
			constexpr std::array reactions{2, 4, 5, 6};
			m_pending.push_back({deaths[m_deaths++ % deaths.size()], victim, false});
			if (!survivors.empty()) {
				const auto actor = survivors[m_reactions % survivors.size()];
				m_pending.push_back({reactions[m_reactions++ % reactions.size()], actor, true});
			}
		}
		void Update(float dt, std::span<const Engine::ObjectId> survivors, bool blocked) {
			if (blocked) { Clear(); return; }
			const auto alive = [&](Engine::ObjectId id) {
				return std::find(survivors.begin(), survivors.end(), id) != survivors.end();
			};
			if (m_presentation.clip >= 0) {
				m_remaining -= dt;
				if (m_remaining > 0 && (!m_current.reaction || alive(m_current.actor))) return;
				StopCurrent();
				m_gap = 0.25f;
			}
			m_gap = std::max(0.0f, m_gap - dt);
			if (m_gap > 0) return;
			while (!m_pending.empty()) {
				m_current = m_pending.front();
				m_pending.pop_front();
				if (m_current.reaction && !alive(m_current.actor)) continue;
				const auto& clip = EnemyRadioClips[m_current.clip];
				m_remaining = clip.seconds;
				m_presentation = {m_current.clip, ++m_serial,
					{m_current.reaction ? "敵僚機" : "敵機", clip.text, "enemy"}};
				break;
			}
		}
		void Clear() { m_pending.clear(); StopCurrent(); m_gap = 0; }
		const EnemyRadioPresentation& GetPresentation() const { return m_presentation; }
	  private:
		struct Message { int clip = -1; Engine::ObjectId actor = Engine::InvalidObjectId; bool reaction = false; };
		void StopCurrent() { m_presentation = {}; m_current = {}; m_remaining = 0; }
		std::deque<Message> m_pending;
		Message m_current;
		EnemyRadioPresentation m_presentation;
		size_t m_deaths = 0, m_reactions = 0;
		uint64_t m_serial = 0;
		float m_remaining = 0, m_gap = 0;
	};
} // namespace Game
