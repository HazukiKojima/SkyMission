#include "OpeningCinematicController.h"
#include <algorithm>
#include <cmath>

namespace Game {
	namespace {
		float SmoothProgress(float seconds, float startSeconds, float durationSeconds) {
			const float progress = std::clamp((seconds - startSeconds) / durationSeconds, 0.0f, 1.0f);
			return progress * progress * (3 - 2 * progress);
		}
	} // namespace
	void OpeningCinematicController::Start(const OpeningCinematicSettings& settings) {
		settings.Validate();
		m_settings = settings;
		m_presentation = {true, true, 0, 0, settings.title, settings.subtitle};
		m_briefingSeconds = 0;
		m_briefingStarted = false;
	}
	void OpeningCinematicController::SetBriefingLine(int line) {
		m_presentation.briefingLine = line;
		m_presentation.briefingLines =
			line >= 0 ? m_settings.briefing.at(line).lines : std::array<std::string, 2>{};
		m_briefingSeconds = 0;
		m_briefingStarted = false;
	}
	void OpeningCinematicController::EnableBriefing() {
		if (m_presentation.active) SetBriefingLine(m_settings.briefing.empty() ? -1 : 0);
	}
	void OpeningCinematicController::UpdateBriefing(OpeningVoiceStatus voice, float deltaSeconds) {
		if (!m_presentation.active || m_presentation.briefingLine < 0 || !std::isfinite(deltaSeconds) ||
			deltaSeconds <= 0)
			return;
		if (!m_briefingStarted) {
			m_briefingStarted = true;
			return;
		}
		if (voice == OpeningVoiceStatus::Playing) return;
		m_briefingSeconds += std::min(deltaSeconds, 0.25f);
		const auto index = static_cast<size_t>(m_presentation.briefingLine);
		if (voice == OpeningVoiceStatus::Finished ||
			m_briefingSeconds >= m_settings.briefing[index].fallbackSeconds)
			SetBriefingLine(index + 1 < m_settings.briefing.size() ? static_cast<int>(index + 1) : -1);
	}
	void OpeningCinematicController::Update(float deltaSeconds, bool skipRequested) {
		if (!m_presentation.active || !std::isfinite(deltaSeconds) || deltaSeconds < 0) return;
		if (skipRequested) {
			Finish();
			return;
		}
		if (m_presentation.briefingLine >= 0) return;
		m_presentation.elapsedSeconds =
			std::min(m_settings.durationSeconds, m_presentation.elapsedSeconds + deltaSeconds);
		if (m_presentation.elapsedSeconds >= m_settings.durationSeconds)
			Finish();
		else
			RefreshPresentation();
	}
	void OpeningCinematicController::Finish() {
		SetBriefingLine(-1);
		m_presentation.active = false;
		m_presentation.formationShot = false;
		m_presentation.fadeOpacity = 0;
	}
	void OpeningCinematicController::RefreshPresentation() {
		const float seconds = m_presentation.elapsedSeconds;
		m_presentation.formationShot = seconds < m_settings.cutSeconds;
		m_presentation.fadeOpacity = seconds < m_settings.cutSeconds
			? SmoothProgress(seconds, m_settings.fadeOutStartSeconds,
				  m_settings.cutSeconds - m_settings.fadeOutStartSeconds)
			: 1 - SmoothProgress(seconds, m_settings.fadeInStartSeconds, m_settings.fadeInSeconds);
	}
	OpeningCinematicPose OpeningCinematicController::GetPose() const {
		const float seconds = m_presentation.elapsedSeconds;
		const float blend = SmoothProgress(seconds, 0, m_settings.fadeOutStartSeconds);
		DirectX::XMFLOAT3 camera;
		DirectX::XMStoreFloat3(&camera,
			DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&m_settings.cameraStartOffsetMeters),
				DirectX::XMLoadFloat3(&m_settings.cameraEndOffsetMeters), blend));
		return {camera, seconds * m_settings.flightSpeedMetersPerSecond,
			std::fmod(seconds * m_settings.propellerRadiansPerSecond, DirectX::XM_2PI)};
	}
} // namespace Game
