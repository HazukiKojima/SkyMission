#pragma once
#include <Windows.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Game {
	// 射撃音と命中音を合成し、空いている音声で再生する
	class CombatAudio {
	  public:
		CombatAudio() = default;
		CombatAudio(const CombatAudio&) = delete;
		CombatAudio& operator=(const CombatAudio&) = delete;
		~CombatAudio();

		void Initialize();
		void Update(unsigned volleyCount, unsigned hitCount, bool lockWarning = false,
			bool missileWarning = false, float deltaSeconds = 0);

	  private:
		enum class SoundKind { Shot, Hit, LockWarning, MissileWarning };
		static constexpr std::size_t kVoiceCount = 12;

		static std::vector<int16_t> Synthesize(SoundKind kind);
		void Play(const std::vector<int16_t>& samples, float volume);
		void Shutdown();

		// このインスタンスで成功したCOM初期化だけを終了時に解除する
		bool m_comInitialized = false;
		Microsoft::WRL::ComPtr<IXAudio2> m_engine;
		IXAudio2MasteringVoice* m_master = nullptr;
		std::array<IXAudio2SourceVoice*, kVoiceCount> m_voices{};
		// XAudio2はサンプルを参照するため、音声の破棄までメモリーを保持する
		std::vector<int16_t> m_shot;
		std::vector<int16_t> m_hit;
		std::vector<int16_t> m_lockWarning, m_missileWarning;
		float m_warningCountdown = 0;
		int m_warningKind = 0;
	};
} // namespace Game
