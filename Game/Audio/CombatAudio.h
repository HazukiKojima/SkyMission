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
		CombatAudio(const CombatAudio&) = delete;
		CombatAudio& operator=(const CombatAudio&) = delete;
		~CombatAudio();
		void Initialize(const std::filesystem::path& soundDirectory);
		void Update(const CombatAudioState& state, unsigned hitCount, bool lockWarning = false,
			bool missileWarning = false, float deltaSeconds = 0);
		void SetSuspended(bool suspended);
		void ResetPlayback();
		std::size_t GetLoadedSoundCount() const;

	  private:
		enum class ClipKind { Flight, Gun, Launch, GunHit, MissileHit, Aircraft, Ship, Count };
		enum class SoundKind { Hit, LockWarning, MissileWarning };
		struct Clip {
			std::vector<BYTE> samples;
			WAVEFORMATEX format{};
		};
		static constexpr std::size_t kVoiceCount = 12;
		static bool LoadClip(const std::filesystem::path& path, Clip& clip);
		static std::vector<int16_t> Synthesize(SoundKind kind);
		void Play(const std::vector<int16_t>& samples, float volume, bool warning = false);
		void PlayClip(ClipKind kind, float volume);
		void SetLoop(std::size_t index, ClipKind kind, bool playing, float volume);
		void Shutdown();
		bool m_comInitialized = false, m_mediaInitialized = false, m_suspended = false;
		Microsoft::WRL::ComPtr<IXAudio2> m_engine;
		IXAudio2MasteringVoice* m_master = nullptr;
		std::array<IXAudio2SourceVoice*, kVoiceCount> m_voices{};
		IXAudio2SourceVoice* m_warningVoice = nullptr;
		std::array<IXAudio2SourceVoice*, 2> m_loops{};
		std::array<bool, 2> m_loopPlaying{};
		// Voices reference these buffers until shutdown; do not resize them during playback.
		std::array<Clip, static_cast<std::size_t>(ClipKind::Count)> m_clips;
		std::vector<int16_t> m_hit, m_lockWarning, m_missileWarning;
		float m_warningCountdown = 0;
		int m_warningKind = 0;
	};
} // namespace Game
