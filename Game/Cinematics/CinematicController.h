#pragma once
#include "CinematicSettings.h"
#include "CinematicTypes.h"
#include <optional>

namespace Game {
	// 入力デバイスや描画APIに依存せず、時間から演出の状態を決める
	class CinematicController {
	  public:
		void Start(const CinematicSettings& settings, const std::string& id = "opening");
		// Start timed dialogue during gameplay without switching cameras or pausing combat.
		void StartRadio(const CinematicSettings& settings, const std::string& id);
		void StopRadio();
		void Update(float deltaSeconds, bool skipRequested);
		void EnableBriefing();
		void UpdateBriefing(CinematicVoiceStatus voice, float deltaSeconds);
		const CinematicPresentation& GetPresentation() const {
			return m_presentation;
		}
		CinematicPose GetPose() const;
		const CinematicShotSettings& GetCurrentShot() const;
		std::optional<CinematicCompletion> TakeCompletion();

	  private:
		void Finish(CinematicCompletion reason);
		float ShotSeconds() const;
		std::optional<CinematicCompletion> m_completion;
		uint64_t m_playbackSerial = 0;
		void RefreshPresentation();
		void RefreshRadio();
		float m_radioSeconds = 0;
		bool m_radioRunning = false;
		void SetBriefingLine(int line);
		float m_briefingSeconds = 0;
		bool m_briefingStarted = false;
		CinematicSettings m_settings;
		CinematicPresentation m_presentation;
	};
} // namespace Game
