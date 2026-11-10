#pragma once
#include <Windows.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>
#include "Game/Combat/CombatTypes.h"

namespace Game {
	class CombatAudio {
	  public:
		CombatAudio() = default;
		~CombatAudio();
		CombatAudio(const CombatAudio&) = delete;
		CombatAudio& operator=(const CombatAudio&) = delete;

		// 初期化・再生状態の制御
		void Initialize(const std::filesystem::path& soundDirectory);
		void Update(const CombatAudioState& state, unsigned hitCount, bool lockWarning = false,
			bool missileWarning = false, float deltaSeconds = 0);
		int UpdateOpening(const OpeningCinematicPresentation& opening, float deltaSeconds);
		void SetSuspended(bool suspended);
		void ResetPlayback();

		std::size_t GetLoadedSoundCount() const;

	  private:
		enum class ClipKind { Flight, Gun, Launch, GunHit, MissileHit, Aircraft, Ship, BattleBgm, AirRaid, Briefing1, Briefing2, Briefing3, Count };
		enum class SoundKind { Hit, LockWarning, MissileWarning };
		struct Clip {
			std::vector<BYTE> samples;
			WAVEFORMATEX format{};
		};

		static constexpr std::size_t kVoiceCount = 12;

		static bool LoadClip(const std::filesystem::path& path, Clip& clip, UINT channels = 1);
		static std::vector<int16_t> Synthesize(SoundKind kind);
		void Play(const std::vector<int16_t>& samples, float volume, bool warning = false);
		void PlayClip(ClipKind kind, float volume);
		void SetLoop(std::size_t index, ClipKind kind, bool playing, float volume);
		void UpdateWarning(bool flying, bool lockWarning, bool missileWarning, float deltaSeconds);
		void PlayEvents(const CombatAudioState& state, unsigned hitCount);
		void Shutdown();

		// COMと音声エンジンの状態
		bool m_comInitialized = false;
		bool m_mediaInitialized = false;
		bool m_suspended = false;

		Microsoft::WRL::ComPtr<IXAudio2> m_engine;
		IXAudio2MasteringVoice* m_master = nullptr;

		// 単発音・警告音・ループ音のボイス
		std::array<IXAudio2SourceVoice*, kVoiceCount> m_voices{};
		IXAudio2SourceVoice* m_briefingVoice = nullptr;
		int m_briefingLine = -1;
		float m_briefingFallbackSeconds = 0;
		bool m_openingActive = false;
		IXAudio2SourceVoice* m_warningVoice = nullptr;
		std::array<IXAudio2SourceVoice*, 4> m_loops{};
		std::array<bool, 4> m_loopPlaying{};

		// 終了までボイスが参照するため、再生中にバッファをリサイズしない
		std::array<Clip, static_cast<std::size_t>(ClipKind::Count)> m_clips;
		std::vector<int16_t> m_hit;
		std::vector<int16_t> m_lockWarning;
		std::vector<int16_t> m_missileWarning;

		float m_warningCountdownSeconds = 0;
		int m_warningKind = 0;
	};
} // namespace Game
