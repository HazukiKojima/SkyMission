#pragma once
#include <Windows.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstdint>
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")

namespace Game {
    class CombatAudio {
    public:
        CombatAudio() = default;
        CombatAudio(const CombatAudio&) = delete;
        CombatAudio& operator=(const CombatAudio&) = delete;
        ~CombatAudio() { Shutdown(); }
        void Initialize() {
            Shutdown();
            const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            m_comInitialized = SUCCEEDED(com);
            if (FAILED(com) && com != RPC_E_CHANGED_MODE) return;
            if (FAILED(XAudio2Create(m_engine.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR))) { Shutdown(); return; }
            if (FAILED(m_engine->CreateMasteringVoice(&m_master))) { Shutdown(); return; }
            m_shot = Synthesize(false); m_hit = Synthesize(true);
            WAVEFORMATEX format{};
            format.wFormatTag = WAVE_FORMAT_PCM; format.nChannels = 1;
            format.nSamplesPerSec = 48000; format.wBitsPerSample = 16;
            format.nBlockAlign = 2; format.nAvgBytesPerSec = 96000;
            for (auto& voice : m_voices)
                if (FAILED(m_engine->CreateSourceVoice(&voice, &format))) { Shutdown(); return; }
        }
        void Update(unsigned volleys, unsigned hits) {
            // 同一フレームの多重命中は一音にまとめ、音量の急増を抑える
            if (volleys) Play(m_shot, 0.38f);
            if (hits) Play(m_hit, 0.32f);
        }
    private:
        static std::vector<int16_t> Synthesize(bool hit) {
            constexpr float rate = 48000;
            const float duration = hit ? 0.12f : 0.16f;
            std::vector<int16_t> samples(static_cast<size_t>(duration * rate));
            uint32_t random = hit ? 719u : 127u;
            float filtered = 0;
            for (size_t i = 0; i < samples.size(); ++i) {
                const float t = static_cast<float>(i) / rate;
                random = random * 1664525u + 1013904223u;
                const float noise = static_cast<float>(random >> 8) / 8388607.5f - 1;
                filtered += 0.13f * (noise - filtered);
                const float tone = hit ? std::sin(6.2831853f * 2100 * t) * 0.25f
                    : std::sin(6.2831853f * (95 * t - 120 * t * t)) * 0.5f;
                const float envelope = std::min(t * 3000, 1.0f) * std::exp(-t * (hit ? 42 : 32));
                const float signal = hit ? noise * 0.65f + tone : filtered + tone;
                samples[i] = static_cast<int16_t>(signal * envelope * 24000);
            }
            return samples;
        }
        void Play(const std::vector<int16_t>& samples, float volume) {
            for (auto* voice : m_voices) {
                if (!voice) continue;
                XAUDIO2_VOICE_STATE state{};
                voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
                if (state.BuffersQueued) continue;
                XAUDIO2_BUFFER buffer{};
                buffer.AudioBytes = static_cast<UINT32>(samples.size() * sizeof(int16_t));
                buffer.pAudioData = reinterpret_cast<const BYTE*>(samples.data());
                buffer.Flags = XAUDIO2_END_OF_STREAM;
                voice->SetVolume(volume);
                if (SUCCEEDED(voice->SubmitSourceBuffer(&buffer))) voice->Start();
                return;
            }
        }
        void Shutdown() {
            for (auto*& voice : m_voices) { if (voice) voice->DestroyVoice(); voice = nullptr; }
            if (m_master) m_master->DestroyVoice();
            m_master = nullptr; m_engine.Reset();
            if (m_comInitialized) CoUninitialize();
            m_comInitialized = false;
        }
        bool m_comInitialized = false;
        Microsoft::WRL::ComPtr<IXAudio2> m_engine;
        IXAudio2MasteringVoice* m_master = nullptr;
        std::array<IXAudio2SourceVoice*, 12> m_voices{};
        std::vector<int16_t> m_shot, m_hit;
    };
}
