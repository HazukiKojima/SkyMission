#pragma once
#include "OpeningCinematicSettings.h"
#include "OpeningCinematicTypes.h"

namespace Game {
	// 入力デバイスや描画APIに依存せず、時間から演出の状態を決める
	class OpeningCinematicController {
	  public:
		void Start(const OpeningCinematicSettings& settings);
		void Update(float deltaSeconds, bool skipRequested);
		const OpeningCinematicPresentation& GetPresentation() const {
			return m_presentation;
		}
		OpeningCinematicPose GetPose() const;

	  private:
		void Finish();
		void RefreshPresentation();
		OpeningCinematicSettings m_settings;
		OpeningCinematicPresentation m_presentation;
	};
} // namespace Game
