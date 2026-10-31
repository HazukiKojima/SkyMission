#include "CombatAudio.h"
#include <algorithm>
#include <cmath>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

namespace Game {
	namespace {
		constexpr DWORD kSampleRate = 48000;
		constexpr float kShotVolume = 0.38f;
		constexpr float kHitVolume = 0.32f;
		constexpr float kShotDurationSeconds = 0.16f;
		constexpr float kHitDurationSeconds = 0.12f;
	} // namespace

	CombatAudio::~CombatAudio() {
		Shutdown();
	}

	void CombatAudio::Initialize() {
		Shutdown();

		// 別方式のCOM初期化済み環境でも音声の初期化を試みる
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		m_comInitialized = SUCCEEDED(comResult);
		if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE)
			return;
		// 初期化できない環境では、リソースを解放して無音で継続する
		if (FAILED(XAudio2Create(m_engine.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR))) {
			Shutdown();
			return;
		}
		if (FAILED(m_engine->CreateMasteringVoice(&m_master))) {
			Shutdown();
			return;
		}

		m_shot = Synthesize(SoundKind::Shot);
		m_hit = Synthesize(SoundKind::Hit);

		WAVEFORMATEX format{};
		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = 1;
		format.nSamplesPerSec = kSampleRate;
		format.wBitsPerSample = 16;
		format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
		for (auto& voice : m_voices) {
			if (FAILED(m_engine->CreateSourceVoice(&voice, &format))) {
				Shutdown();
				return;
			}
		}
	}

	void CombatAudio::Update(unsigned volleyCount, unsigned hitCount) {
		// 同一フレームの多重命中は一音にまとめ、音量の急増を抑える
		if (volleyCount)
			Play(m_shot, kShotVolume);
		if (hitCount)
			Play(m_hit, kHitVolume);
	}

	std::vector<int16_t> CombatAudio::Synthesize(SoundKind kind) {
		const bool isHit = kind == SoundKind::Hit;

		const float durationSeconds = isHit ? kHitDurationSeconds : kShotDurationSeconds;
		std::vector<int16_t> samples(static_cast<size_t>(durationSeconds * static_cast<float>(kSampleRate)));
		uint32_t randomState = isHit ? 719u : 127u;
		float filteredNoise = 0;

		// 固定シードのノイズと減衰する音を重ね、毎回同じ波形を生成する
		for (size_t i = 0; i < samples.size(); ++i) {
			const float timeSeconds = static_cast<float>(i) / static_cast<float>(kSampleRate);
			randomState = randomState * 1664525u + 1013904223u;
			const float noise = static_cast<float>(randomState >> 8) / 8388607.5f - 1;
			filteredNoise += 0.13f * (noise - filteredNoise);
			const float tone = isHit
				? std::sin(6.2831853f * 2100 * timeSeconds) * 0.25f
				: std::sin(6.2831853f * (95 * timeSeconds - 120 * timeSeconds * timeSeconds)) * 0.5f;
			const float envelope =
				std::min(timeSeconds * 3000, 1.0f) * std::exp(-timeSeconds * (isHit ? 42 : 32));
			const float signal = isHit ? noise * 0.65f + tone : filteredNoise + tone;
			samples[i] = static_cast<int16_t>(signal * envelope * 24000);
		}
		return samples;
	}

	void CombatAudio::Play(const std::vector<int16_t>& samples, float volume) {
		for (auto* voice : m_voices) {
			if (!voice)
				continue;
			XAUDIO2_VOICE_STATE state{};
			voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			if (state.BuffersQueued)
				continue;
			XAUDIO2_BUFFER buffer{};
			buffer.AudioBytes = static_cast<UINT32>(samples.size() * sizeof(int16_t));
			buffer.pAudioData = reinterpret_cast<const BYTE*>(samples.data());
			buffer.Flags = XAUDIO2_END_OF_STREAM;
			voice->SetVolume(volume);
			if (SUCCEEDED(voice->SubmitSourceBuffer(&buffer)))
				voice->Start();
			return;
		}
	}

	void CombatAudio::Shutdown() {
		// 参照されるサンプルや出力先より先に、再生用の音声を破棄する
		for (auto*& voice : m_voices) {
			if (voice)
				voice->DestroyVoice();
			voice = nullptr;
		}
		if (m_master)
			m_master->DestroyVoice();
		m_master = nullptr;
		m_engine.Reset();
		if (m_comInitialized)
			CoUninitialize();
		m_comInitialized = false;
	}

} // namespace Game
