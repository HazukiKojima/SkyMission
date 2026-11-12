#include "CombatAudio.h"
#include "Engine/Log.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace Game {
	namespace {
		constexpr DWORD kSampleRate = 48000;
		constexpr DWORD kAudioStream = static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM);
		constexpr DWORD kAllStreams = static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS);
		constexpr int kNoWarning = 0;
		constexpr int kLockWarning = 1;
		constexpr int kMissileWarning = 2;
		WAVEFORMATEX PcmFormat() {
			WAVEFORMATEX format{};
			format.wFormatTag = WAVE_FORMAT_PCM;
			format.nChannels = 1;
			format.nSamplesPerSec = kSampleRate;
			format.wBitsPerSample = 16;
			format.nBlockAlign = 2;
			format.nAvgBytesPerSec = kSampleRate * 2;
			return format;
		}
		void StopVoice(IXAudio2SourceVoice* voice) {
			if (voice) {
				voice->Stop();
				voice->FlushSourceBuffers();
			}
		}
	} // namespace

	CombatAudio::~CombatAudio() {
		Shutdown();
	}

	bool CombatAudio::LoadClip(const std::filesystem::path& path, Clip& clip, UINT channels) {
		using Microsoft::WRL::ComPtr;
		ComPtr<IMFSourceReader> reader;
		if (FAILED(MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader))) return false;
		ComPtr<IMFMediaType> requested;
		if (FAILED(MFCreateMediaType(&requested))) return false;
		if (FAILED(requested->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio)) ||
			FAILED(requested->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM)) ||
			FAILED(requested->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, channels)) ||
			FAILED(requested->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kSampleRate)) ||
			FAILED(requested->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16)) ||
			FAILED(requested->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, channels * 2)) ||
			FAILED(requested->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, kSampleRate * channels * 2)) ||
			FAILED(reader->SetStreamSelection(kAllStreams, FALSE)) ||
			FAILED(reader->SetStreamSelection(kAudioStream, TRUE)) ||
			FAILED(reader->SetCurrentMediaType(kAudioStream, nullptr, requested.Get()))) {
			return false;
		}
		ComPtr<IMFMediaType> actual;
		if (FAILED(reader->GetCurrentMediaType(kAudioStream, &actual))) return false;
		WAVEFORMATEX* wave = nullptr;
		UINT32 waveBytes = 0;
		if (FAILED(MFCreateWaveFormatExFromMFMediaType(actual.Get(), &wave, &waveBytes))) return false;
		const auto format = *wave;
		CoTaskMemFree(wave);
		if (format.wFormatTag != WAVE_FORMAT_PCM || format.nChannels != channels ||
			format.nSamplesPerSec != kSampleRate || format.wBitsPerSample != 16 ||
			format.nBlockAlign != channels * 2) {
			return false;
		}
		Clip decoded;
		decoded.format = format;
		for (;;) {
			DWORD flags = 0;
			ComPtr<IMFSample> sample;
			if (FAILED(reader->ReadSample(kAudioStream, 0, nullptr, &flags, nullptr, &sample)) ||
				(flags & (MF_SOURCE_READERF_ERROR | MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))) {
				return false;
			}
			if (sample) {
				ComPtr<IMFMediaBuffer> buffer;
				if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) return false;
				BYTE* data = nullptr;
				DWORD length = 0;
				if (FAILED(buffer->Lock(&data, nullptr, &length))) return false;
				// 破損ファイルでも起動時のメモリ使用量を制限する
				if (decoded.samples.size() + length > 64 * 1024 * 1024 * channels) {
					buffer->Unlock();
					return false;
				}
				try {
					decoded.samples.insert(decoded.samples.end(), data, data + length);
				} catch (...) {
					buffer->Unlock();
					throw;
				}
				buffer->Unlock();
			}
			if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
				break;
			}
		}
		if (decoded.samples.empty() || decoded.samples.size() % format.nBlockAlign) return false;
		clip = std::move(decoded);
		return true;
	}

	void CombatAudio::Initialize(const std::filesystem::path& soundDirectory) {
		// 単独利用時も従来のSoundディレクトリ基準の既定値を維持する。
		CinematicSettings opening;
		opening.audio.bgm = soundDirectory / L"Opening/空襲BGM.wav";
		for (auto& entry : opening.briefing)
			entry.audio = soundDirectory / L"Opening" / entry.audio.filename();
		Initialize(soundDirectory, opening);
	}
	void CombatAudio::Initialize(
		const std::filesystem::path& soundDirectory, const CinematicSettings& opening) {
		Initialize(soundDirectory, std::map<std::string, CinematicSettings>{{"opening", opening}});
	}
	void CombatAudio::Initialize(const std::filesystem::path& soundDirectory,
		const std::map<std::string, CinematicSettings>& cinematics) {
		for (const auto& [id, settings] : cinematics)
			settings.Validate();
		Shutdown();
		const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		m_comInitialized = SUCCEEDED(comResult);
		if (FAILED(comResult) && comResult != RPC_E_CHANGED_MODE) return;
		m_mediaInitialized = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
		m_settings = {};
		m_settings.Load(Engine::JsonAssetReader{}, soundDirectory / L"audio.json");
		const std::string names[] = {m_settings.flightFile, m_settings.gunFile, m_settings.launchFile,
			m_settings.gunHitFile, m_settings.missileHitFile, m_settings.aircraftFile, m_settings.shipFile,
			m_settings.battleBgmFile};
		for (std::size_t i = 0; i < m_clips.size(); ++i) {
			const auto path = soundDirectory /
				std::filesystem::path(
					std::u8string(reinterpret_cast<const char8_t*>(names[i].data()), names[i].size()));
			const UINT channels = i == static_cast<std::size_t>(ClipKind::BattleBgm) ? 2 : 1;
			if (!m_mediaInitialized || !LoadClip(path, m_clips[i], channels)) {
				Engine::Log(L"CombatAudio: could not load {}\n", path.wstring());
			}
		}
		const auto loadCinematicClip = [&](const std::filesystem::path& path, Clip& clip, UINT channels) {
			if (!path.empty() && (!m_mediaInitialized || !LoadClip(path, clip, channels)))
				Engine::Log(L"CombatAudio: could not load {}\n", path.wstring());
		};
		for (size_t i = 0; i < EnemyRadioClips.size(); ++i)
			loadCinematicClip(soundDirectory / EnemyRadioClips[i].file, m_enemyRadioClips[i], 1);
		for (const auto& [id, settings] : cinematics) {
			auto& bank = m_cinematicBanks[id];
			bank.settings = settings.audio;
			loadCinematicClip(settings.audio.bgm, bank.bgm, 2);
			bank.briefingCount = settings.briefing.size();
			bank.briefing.resize(settings.briefing.size() + settings.radio.size());
			for (size_t i = 0; i < settings.briefing.size(); ++i)
				loadCinematicClip(settings.briefing[i].audio, bank.briefing[i], 1);
			for (size_t i = 0; i < settings.radio.size(); ++i)
				loadCinematicClip(settings.radio[i].audio, bank.briefing[bank.briefingCount + i], 1);
		}
		if (FAILED(XAudio2Create(m_engine.GetAddressOf(), 0, XAUDIO2_DEFAULT_PROCESSOR)) ||
			FAILED(m_engine->CreateMasteringVoice(&m_master))) {
			Engine::Log("CombatAudio: audio output unavailable\n");
			return;
		}
		m_hit = Synthesize(SoundKind::Hit);
		m_lockWarning = Synthesize(SoundKind::LockWarning);
		m_missileWarning = Synthesize(SoundKind::MissileWarning);
		const auto format = PcmFormat();
		for (auto& voice : m_voices) {
			if (FAILED(m_engine->CreateSourceVoice(&voice, &format))) {
				Shutdown();
				return;
			}
		}
		for (std::size_t i = 0; i < m_loops.size(); ++i) {
			const auto& bgm = m_clips[static_cast<std::size_t>(ClipKind::BattleBgm)];
			auto loopFormat = i == 2 && !bgm.samples.empty() ? bgm.format : format;
			if (i == 3) {
				loopFormat.nChannels = 2;
				loopFormat.nBlockAlign = 4;
				loopFormat.nAvgBytesPerSec = kSampleRate * 4;
			}
			if (FAILED(m_engine->CreateSourceVoice(&m_loops[i], &loopFormat))) {
				Shutdown();
				return;
			}
		}
		if (FAILED(m_engine->CreateSourceVoice(&m_enemyRadioVoice, &format))) {
			Shutdown();
			return;
		}
		if (FAILED(m_engine->CreateSourceVoice(&m_briefingVoice, &format))) {
			Shutdown();
			return;
		}
		if (FAILED(m_engine->CreateSourceVoice(&m_warningVoice, &format))) {
			Shutdown();
		}
	}

	std::size_t CombatAudio::GetLoadedSoundCount() const {
		const auto loaded = [](const Clip& clip) {
			return !clip.samples.empty();
		};
		size_t count = static_cast<size_t>(std::count_if(m_clips.begin(), m_clips.end(), loaded));
		for (const auto& [id, bank] : m_cinematicBanks)
			count += static_cast<size_t>(std::count_if(bank.briefing.begin(), bank.briefing.end(), loaded)) +
				(loaded(bank.bgm) ? 1 : 0);
		count += static_cast<size_t>(std::count_if(m_enemyRadioClips.begin(), m_enemyRadioClips.end(), loaded));
		return count;
	}

	void CombatAudio::SetSuspended(bool suspended) {
		if (m_suspended == suspended) return;
		m_suspended = suspended;
		if (!m_engine) return;
		if (suspended) {
			m_engine->StopEngine();
		} else {
			m_engine->StartEngine();
		}
	}

	void CombatAudio::ResetPlayback() {
		for (auto* voice : m_voices) {
			StopVoice(voice);
		}
		for (auto* voice : m_loops) {
			StopVoice(voice);
		}
		StopVoice(m_warningVoice);
		StopVoice(m_enemyRadioVoice);
		m_enemyRadioSerial = 0;
		StopVoice(m_briefingVoice);
		m_briefingLine = -1;
		m_briefingStatus = CinematicVoiceStatus::Unavailable;
		m_loopPlaying = {};
		m_activeCinematicId.clear();
		m_activeCinematicAudio = nullptr;
		m_warningCountdownSeconds = 0;
		m_warningKind = 0;
	}

	void CombatAudio::SetLoop(std::size_t index, ClipKind kind, bool playing, float volume) {
		SetLoopClip(index, m_clips[static_cast<std::size_t>(kind)], playing, volume);
	}
	void CombatAudio::SetLoopClip(std::size_t index, const Clip& clip, bool playing, float volume) {
		auto* voice = m_loops[index];
		if (!voice || clip.samples.empty()) return;
		voice->SetVolume(volume);
		if (playing == m_loopPlaying[index]) return;
		StopVoice(voice);
		m_loopPlaying[index] = false;
		if (!playing) return;
		XAUDIO2_BUFFER buffer{};
		buffer.AudioBytes = static_cast<UINT32>(clip.samples.size());
		buffer.pAudioData = clip.samples.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
		voice->SetVolume(volume);
		if (SUCCEEDED(voice->SubmitSourceBuffer(&buffer)) && SUCCEEDED(voice->Start())) {
			m_loopPlaying[index] = true;
		}
	}

	void CombatAudio::PlayClip(ClipKind kind, float volume) {
		const auto& clip = m_clips[static_cast<std::size_t>(kind)];
		if (clip.samples.empty()) return;
		for (auto* voice : m_voices) {
			if (!voice) continue;
			XAUDIO2_VOICE_STATE state{};
			voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			if (state.BuffersQueued) continue;
			XAUDIO2_BUFFER buffer{};
			buffer.AudioBytes = static_cast<UINT32>(clip.samples.size());
			buffer.pAudioData = clip.samples.data();
			buffer.Flags = XAUDIO2_END_OF_STREAM;
			voice->SetVolume(volume);
			if (SUCCEEDED(voice->SubmitSourceBuffer(&buffer))) {
				voice->Start();
			}
			return;
		}
	}

	void CombatAudio::Update(const CombatAudioState& state, unsigned hitCount, bool lockWarning,
		bool missileWarning, float deltaSeconds) {
		if (state.restarted && !m_cinematicActive) {
			ResetPlayback();
		}
		SetLoop(0, ClipKind::Flight, state.flying,
			m_settings.flightVolume *
				(m_cinematicActive && m_activeCinematicAudio
						? m_activeCinematicAudio->settings.flightVolumeScale
						: 1.0f));
		SetLoop(1, ClipKind::Gun, state.flying && state.gunFiring, m_settings.gunVolume);
		SetLoop(2, ClipKind::BattleBgm, !m_cinematicActive, m_settings.battleBgmVolume);
		if (m_activeCinematicAudio)
			SetLoopClip(3, m_activeCinematicAudio->bgm, m_cinematicActive,
				m_activeCinematicAudio->settings.bgmVolume);
		UpdateWarning(state.flying, lockWarning, missileWarning, deltaSeconds);
		PlayEvents(state, hitCount);
	}
	CinematicVoiceStatus CombatAudio::UpdateCinematic(const CinematicPresentation& opening) {
		if (m_activeCinematicId != opening.cinematicId || m_activePlaybackSerial != opening.playbackSerial) {
			StopVoice(m_briefingVoice);
			StopVoice(m_loops[3]);
			m_loopPlaying[3] = false;
			m_briefingLine = -1;
			m_briefingStatus = CinematicVoiceStatus::Unavailable;
			m_activeCinematicId = opening.cinematicId;
			m_activePlaybackSerial = opening.playbackSerial;
			const auto bank = m_cinematicBanks.find(m_activeCinematicId);
			m_activeCinematicAudio = bank == m_cinematicBanks.end() ? nullptr : &bank->second;
		}
		m_cinematicActive = opening.active;
		// Radio clips share the speech voice, but do not stop the cinematic clock.
		const int voiceLine = opening.briefingLine >= 0 ? opening.briefingLine
			: opening.radioLine >= 0 && m_activeCinematicAudio
				? static_cast<int>(m_activeCinematicAudio->briefingCount) + opening.radioLine : -1;
		if (voiceLine < 0 || (!opening.active && opening.radioLine < 0)) {
			StopVoice(m_briefingVoice);
			m_briefingLine = -1;
			m_briefingStatus = CinematicVoiceStatus::Unavailable;
			return m_briefingStatus;
		}
		if (!m_activeCinematicAudio ||
			static_cast<size_t>(voiceLine) >= m_activeCinematicAudio->briefing.size())
			return CinematicVoiceStatus::Unavailable;
		// Command speech owns the radio channel even if an enemy caption is stale this frame.
		StopVoice(m_enemyRadioVoice);
		m_enemyRadioSerial = 0;
		if (voiceLine != m_briefingLine) {
			StopVoice(m_briefingVoice);
			m_briefingLine = voiceLine;
			m_briefingStatus = CinematicVoiceStatus::Unavailable;
			const auto& clip = m_activeCinematicAudio->briefing[m_briefingLine];
			if (m_briefingVoice && !clip.samples.empty()) {
				XAUDIO2_BUFFER buffer{};
				buffer.AudioBytes = static_cast<UINT32>(clip.samples.size());
				buffer.pAudioData = clip.samples.data();
				buffer.Flags = XAUDIO2_END_OF_STREAM;
				m_briefingVoice->SetVolume(m_activeCinematicAudio->settings.briefingVolume);
				if (SUCCEEDED(m_briefingVoice->SubmitSourceBuffer(&buffer)) &&
					SUCCEEDED(m_briefingVoice->Start()))
					m_briefingStatus = CinematicVoiceStatus::Playing;
				else
					StopVoice(m_briefingVoice);
			}
			return m_briefingStatus;
		}
		if (m_suspended) return m_briefingStatus;
		XAUDIO2_VOICE_STATE state{};
		if (m_briefingVoice) m_briefingVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
		if (m_briefingStatus == CinematicVoiceStatus::Playing && !state.BuffersQueued)
			m_briefingStatus = CinematicVoiceStatus::Finished;
		return m_briefingStatus;
	}
	void CombatAudio::UpdateEnemyRadio(const EnemyRadioPresentation& radio) {
		if (m_briefingLine >= 0 || radio.clip < 0 || static_cast<size_t>(radio.clip) >= m_enemyRadioClips.size()) {
			StopVoice(m_enemyRadioVoice);
			m_enemyRadioSerial = 0;
			return;
		}
		if (radio.serial == m_enemyRadioSerial) return;
		StopVoice(m_enemyRadioVoice);
		m_enemyRadioSerial = radio.serial;
		const auto& clip = m_enemyRadioClips[radio.clip];
		if (!m_enemyRadioVoice || clip.samples.empty()) return;
		XAUDIO2_BUFFER buffer{};
		buffer.AudioBytes = static_cast<UINT32>(clip.samples.size());
		buffer.pAudioData = clip.samples.data();
		buffer.Flags = XAUDIO2_END_OF_STREAM;
		m_enemyRadioVoice->SetVolume(0.5f);
		if (FAILED(m_enemyRadioVoice->SubmitSourceBuffer(&buffer)) || FAILED(m_enemyRadioVoice->Start()))
			StopVoice(m_enemyRadioVoice);
	}
	void CombatAudio::UpdateWarning(bool flying, bool lockWarning, bool missileWarning, float deltaSeconds) {
		int warning = kNoWarning;
		if (flying) {
			if (missileWarning) {
				warning = kMissileWarning;
			} else if (lockWarning) {
				warning = kLockWarning;
			}
		}
		if (warning != m_warningKind) {
			StopVoice(m_warningVoice);
			m_warningKind = warning;
			m_warningCountdownSeconds = 0;
		}
		if (std::isfinite(deltaSeconds) && deltaSeconds > 0) {
			m_warningCountdownSeconds -= std::min(deltaSeconds, 0.25f);
		}
		if (warning != kNoWarning && m_warningCountdownSeconds <= 0) {
			Play(missileWarning ? m_missileWarning : m_lockWarning, m_settings.warningVolume, true);
			m_warningCountdownSeconds = missileWarning ? m_settings.missileWarningIntervalSeconds
													   : m_settings.lockWarningIntervalSeconds;
		}
	}
	void CombatAudio::PlayEvents(const CombatAudioState& state, unsigned hitCount) {
		// 同じフレームの同種イベントをまとめ、急な音量増加を防ぐ
		if (state.missileLaunches) {
			PlayClip(ClipKind::Launch, m_settings.launchVolume);
		}
		if (state.gunHits) {
			PlayClip(ClipKind::GunHit, m_settings.gunHitVolume);
		}
		if (state.missileHits) {
			PlayClip(ClipKind::MissileHit, m_settings.missileHitVolume);
		}
		if (state.aircraftExplosions) {
			PlayClip(ClipKind::Aircraft, m_settings.aircraftVolume);
		}
		if (state.shipExplosions) {
			PlayClip(ClipKind::Ship, m_settings.shipVolume);
		}
		if (hitCount) {
			Play(m_hit, m_settings.hitVolume);
		}
	}

	std::vector<int16_t> CombatAudio::Synthesize(SoundKind kind) {
		const bool warning = kind != SoundKind::Hit;
		const float duration = warning ? 0.1f : 0.12f;
		std::vector<int16_t> samples(static_cast<std::size_t>(kSampleRate * duration));
		uint32_t randomState = 719;
		for (std::size_t i = 0; i < samples.size(); ++i) {
			const float time = static_cast<float>(i) / kSampleRate;
			if (warning) {
				const float frequency = kind == SoundKind::MissileWarning ? 1600.0f : 900.0f;
				const float envelope = std::min(time * 200, 1.0f) * std::min((duration - time) * 200, 1.0f);
				samples[i] = static_cast<int16_t>(std::sin(6.2831853f * frequency * time) * envelope * 16000);
			} else {
				randomState = randomState * 1664525u + 1013904223u;
				const float noise = static_cast<float>(randomState >> 8) / 8388607.5f - 1;
				const float tone = std::sin(6.2831853f * 2100 * time) * 0.25f;
				const float envelope = std::min(time * 3000, 1.0f) * std::exp(-time * 42);
				samples[i] = static_cast<int16_t>((noise * 0.65f + tone) * envelope * 24000);
			}
		}
		return samples;
	}

	void CombatAudio::Play(const std::vector<int16_t>& samples, float volume, bool warning) {
		if (samples.empty()) return;
		const auto submit = [&](IXAudio2SourceVoice* voice) {
			if (!voice) return false;
			XAUDIO2_VOICE_STATE state{};
			voice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
			if (state.BuffersQueued) return false;
			XAUDIO2_BUFFER buffer{};
			buffer.AudioBytes = static_cast<UINT32>(samples.size() * sizeof(int16_t));
			buffer.pAudioData = reinterpret_cast<const BYTE*>(samples.data());
			buffer.Flags = XAUDIO2_END_OF_STREAM;
			voice->SetVolume(volume);
			if (SUCCEEDED(voice->SubmitSourceBuffer(&buffer))) {
				voice->Start();
			}
			return true;
		};
		if (warning) {
			submit(m_warningVoice);
		} else {
			for (auto* voice : m_voices) {
				if (submit(voice)) {
					break;
				}
			}
		}
	}

	void CombatAudio::Shutdown() {
		// XAudio2はPCMバッファを参照するため、先にすべてのボイスを破棄する
		for (auto*& voice : m_voices) {
			if (voice) {
				voice->DestroyVoice();
			}
			voice = nullptr;
		}
		for (auto*& voice : m_loops) {
			if (voice) {
				voice->DestroyVoice();
			}
			voice = nullptr;
		}
		if (m_warningVoice) {
			m_warningVoice->DestroyVoice();
		}
		m_warningVoice = nullptr;
		if (m_enemyRadioVoice) m_enemyRadioVoice->DestroyVoice();
		m_enemyRadioVoice = nullptr;
		m_enemyRadioSerial = 0;
		for (auto& clip : m_enemyRadioClips) clip = {};
		if (m_briefingVoice) m_briefingVoice->DestroyVoice();
		m_briefingVoice = nullptr;
		m_briefingLine = -1;
		m_briefingStatus = CinematicVoiceStatus::Unavailable;
		m_cinematicActive = false;
		if (m_master) {
			m_master->DestroyVoice();
		}
		m_master = nullptr;
		m_engine.Reset();
		m_activeCinematicAudio = nullptr;
		m_activeCinematicId.clear();
		m_cinematicBanks.clear();
		for (auto& clip : m_clips) {
			clip = {};
		}
		if (m_mediaInitialized) {
			MFShutdown();
		}
		m_mediaInitialized = false;
		if (m_comInitialized) {
			CoUninitialize();
		}
		m_comInitialized = false;
		m_suspended = false;
		m_loopPlaying = {};
		m_warningCountdownSeconds = 0;
		m_warningKind = 0;
	}
} // namespace Game
