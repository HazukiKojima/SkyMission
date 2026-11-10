#pragma once
#include "CinematicSettings.h"
#include "CinematicTypes.h"
#include <optional>

namespace Game {
	// 入力デバイスや描画APIに依存せず、時間から演出の状態を決める
	class CinematicController {
	  public:
		void Start(const CinematicSettings& settings, const std::string& id = "opening");
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
		void SetBriefingLine(int line);
		float m_briefingSeconds = 0;
		bool m_briefingStarted = false;
		CinematicSettings m_settings;
		CinematicPresentation m_presentation;
	};
} // namespace Game
