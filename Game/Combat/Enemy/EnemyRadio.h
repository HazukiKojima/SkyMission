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
		{L"Voice/Enemy/It's on fire! Aaah!.wav", "燃えてる！うわああっ！", 2.6f},
		{L"Voice/Enemy/Mayday! Mayday! I'm going down!.wav", "メーデー！メーデー！墜落する！", 3.15f},
		{L"Voice/Enemy/No way! Not by this guy!.wav", "嘘だろ、こいつにやられるなんて！", 2.85f},
		{L"Voice/Enemy/Oh, shi—!.wav", "くそっ、しまっ……！", 1.75f},
		{L"Voice/Enemy/Stay away! GET AWAY FROM ME!.wav", "来るな！こっちに来るなあっ！", 2.85f},
		{L"Voice/Enemy/That's no pilot… That's a monster!.wav", "あいつはパイロットじゃない……化け物だ！", 3.1f},
		{L"Voice/Enemy/Too fast! I can't shake him!.wav", "速すぎる！振り切れない！", 2.8f},
		{L"Voice/Enemy/Aaah! No!.wav", "うわあっ！やめろ！", 1.9f},
		{L"Voice/Enemy/Engine's gone. Ejecting..wav", "エンジンがやられた。脱出する！", 2.6f},
		{L"Voice/Enemy/I don't wanna die! Please—!.wav", "死にたくない！頼む、やめ……！", 2.6f},
		{L"Voice/Enemy/I'm hit! What do I do!.wav", "被弾した！どうすればいい！？", 2.4f}
	}};
	struct EnemyRadioPresentation {
		int clip = -1;
		uint64_t serial = 0;
		RadioCaption caption;
	};

	// 音声チャンネルは1つ。連続撃墜中は、待機中の最新の会話だけを残す
	class EnemyRadioController {
	  public:
		void NotifyDestroyed(Engine::ObjectId victim, std::span<const Engine::ObjectId> survivors);
		void NotifyCrashImpact(Engine::ObjectId victim);

		void Update(float dt, std::span<const Engine::ObjectId> survivors, bool blocked);

		bool IsBusy() const { return m_presentation.clip >= 0 || !m_pending.empty(); }
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
