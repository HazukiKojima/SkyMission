#pragma once
#include "OpeningCinematicSettings.h"
#include "OpeningCinematicTypes.h"

namespace Game {
	// 入力デバイスや描画APIに依存せず、時間から演出の状態を決める
	class OpeningCinematicController {
	  public:
		void Start(const OpeningCinematicSettings& settings);
		void Update(float deltaSeconds, bool skipRequested);
		void EnableBriefing();
		void UpdateBriefing(OpeningVoiceStatus voice, float deltaSeconds);
		const OpeningCinematicPresentation& GetPresentation() const {
			return m_presentation;
		}
		OpeningCinematicPose GetPose() const;

	  private:
		void Finish();
		void RefreshPresentation();
		void SetBriefingLine(int line);
		float m_briefingSeconds = 0;
		bool m_briefingStarted = false;
		OpeningCinematicSettings m_settings;
		OpeningCinematicPresentation m_presentation;
	};
} // namespace Game
