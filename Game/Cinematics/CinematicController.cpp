#include "CinematicController.h"
#include <algorithm>
#include <cmath>

namespace Game {
	namespace {
		float SmoothProgress(float seconds, float startSeconds, float durationSeconds) {
			const float progress = std::clamp((seconds - startSeconds) / durationSeconds, 0.0f, 1.0f);
			return progress * progress * (3 - 2 * progress);
		}
	} // namespace
	void CinematicController::Start(const CinematicSettings& settings, const std::string& id) {
		settings.Validate();
		m_settings = settings;
		m_presentation = {true, true, 0, 0, settings.title, settings.subtitle};
		m_presentation.cinematicId = id;
		m_presentation.playbackSerial = ++m_playbackSerial;
		m_completion.reset();
		RefreshPresentation();
		m_briefingSeconds = 0;
		m_briefingStarted = false;
	}
	void CinematicController::SetBriefingLine(int line) {
		m_presentation.briefingLine = line;
		m_presentation.briefingLines =
			line >= 0 ? m_settings.briefing.at(line).lines : std::array<std::string, 2>{};
		m_briefingSeconds = 0;
		m_briefingStarted = false;
	}
	void CinematicController::EnableBriefing() {
		if (m_presentation.active) SetBriefingLine(m_settings.briefing.empty() ? -1 : 0);
	}
	void CinematicController::UpdateBriefing(CinematicVoiceStatus voice, float deltaSeconds) {
		if (!m_presentation.active || m_presentation.briefingLine < 0 || !std::isfinite(deltaSeconds) ||
			deltaSeconds <= 0)
			return;
		if (!m_briefingStarted) {
			m_briefingStarted = true;
			return;
		}
		if (voice == CinematicVoiceStatus::Playing) return;
		m_briefingSeconds += std::min(deltaSeconds, 0.25f);
		const auto index = static_cast<size_t>(m_presentation.briefingLine);
		if (voice == CinematicVoiceStatus::Finished ||
			m_briefingSeconds >= m_settings.briefing[index].fallbackSeconds)
			SetBriefingLine(index + 1 < m_settings.briefing.size() ? static_cast<int>(index + 1) : -1);
	}
	void CinematicController::Update(float deltaSeconds, bool skipRequested) {
		if (!m_presentation.active || !std::isfinite(deltaSeconds) || deltaSeconds < 0) return;
		if (skipRequested) {
			Finish(CinematicCompletion::Skipped);
			return;
		}
		if (m_presentation.briefingLine >= 0) return;
		m_presentation.elapsedSeconds =
			std::min(m_settings.TotalDuration(), m_presentation.elapsedSeconds + deltaSeconds);
		if (m_presentation.elapsedSeconds >= m_settings.TotalDuration())
			Finish(CinematicCompletion::Completed);
		else
			RefreshPresentation();
	}
	void CinematicController::Finish(CinematicCompletion reason) {
		m_completion = reason;
		SetBriefingLine(-1);
		m_presentation.active = false;
		m_presentation.formationShot = false;
		m_presentation.fadeOpacity = 0;
	}
	void CinematicController::RefreshPresentation() {
		m_presentation.text = m_settings.text;
		const float seconds = m_presentation.elapsedSeconds;
		if (!m_settings.shots.empty()) {
			float end = m_settings.shots.front().durationSeconds;
			m_presentation.shotIndex = 0;
			while (seconds >= end && m_presentation.shotIndex + 1 < m_settings.shots.size())
				end += m_settings.shots[++m_presentation.shotIndex].durationSeconds;
			const auto& shot = GetCurrentShot();
			m_presentation.title = shot.title;
			m_presentation.subtitle = shot.subtitle;
			m_presentation.formationShot = true;
			m_presentation.fadeOpacity = 0;
			return;
		}
		m_presentation.formationShot = seconds < m_settings.cutSeconds;
		m_presentation.fadeOpacity = seconds < m_settings.cutSeconds
			? SmoothProgress(seconds, m_settings.fadeOutStartSeconds,
				  m_settings.cutSeconds - m_settings.fadeOutStartSeconds)
			: 1 - SmoothProgress(seconds, m_settings.fadeInStartSeconds, m_settings.fadeInSeconds);
	}
	CinematicPose CinematicController::GetPose() const {
		const auto& shot = GetCurrentShot();
		const float seconds = ShotSeconds();
		const float blend = SmoothProgress(
			seconds, 0, m_settings.shots.empty() ? m_settings.fadeOutStartSeconds : shot.durationSeconds);
		DirectX::XMFLOAT3 camera;
		DirectX::XMStoreFloat3(&camera,
			DirectX::XMVectorLerp(DirectX::XMLoadFloat3(&shot.cameraStartOffsetMeters),
				DirectX::XMLoadFloat3(&shot.cameraEndOffsetMeters), blend));
		return {camera, seconds * shot.flightSpeedMetersPerSecond,
			std::fmod(seconds * shot.propellerRadiansPerSecond, DirectX::XM_2PI)};
	}
	const CinematicShotSettings& CinematicController::GetCurrentShot() const {
		return m_settings.shots.empty() ? static_cast<const CinematicShotSettings&>(m_settings)
										: m_settings.shots.at(m_presentation.shotIndex);
	}
	float CinematicController::ShotSeconds() const {
		float seconds = m_presentation.elapsedSeconds;
		for (size_t i = 0; i < m_presentation.shotIndex; ++i)
			seconds -= m_settings.shots[i].durationSeconds;
		return seconds;
	}
	std::optional<CinematicCompletion> CinematicController::TakeCompletion() {
		auto result = m_completion;
		m_completion.reset();
		return result;
	}
} // namespace Game
