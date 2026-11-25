#include "EnemyRadio.h"

namespace Game {
	void EnemyRadioController::NotifyDestroyed(
		Engine::ObjectId victim, std::span<const Engine::ObjectId> survivors) {
		// 最新の撃墜に合わせて会話を予約する
		if (m_current.reaction && m_current.actor == victim) StopCurrent();
		m_pending.clear();
		constexpr std::array deaths{0, 1, 3, 7, 8, 9, 10};
		constexpr std::array reactions{2, 4, 5, 6};
		m_pending.push_back({deaths[m_deaths++ % deaths.size()], victim, false});

		// 生存している僚機の反応を予約する
		if (!survivors.empty()) {
			const auto actor = survivors[m_reactions % survivors.size()];
			m_pending.push_back({reactions[m_reactions++ % reactions.size()], actor, true});
		}
	}

	void EnemyRadioController::NotifyCrashImpact(Engine::ObjectId victim) {
		// 爆発時点で再生中の台詞は流し切り、未再生の台詞だけを取り消す
		std::erase_if(m_pending, [victim](const Message& message) {
			return !message.reaction && message.actor == victim;
		});
	}

	void EnemyRadioController::Update(float dt, std::span<const Engine::ObjectId> survivors, bool blocked) {
		if (blocked) {
			StopCurrent();
			return;
		}
		const auto alive = [&](Engine::ObjectId id) {
			return std::find(survivors.begin(), survivors.end(), id) != survivors.end();
		};

		// 台詞の終了条件を確認する
		if (m_presentation.clip >= 0) {
			m_remaining -= dt;

			if (m_remaining > 0 && (!m_current.reaction || alive(m_current.actor))) return;

			StopCurrent();
			m_gap = 0.25f;
		}

		m_gap = std::max(0.0f, m_gap - dt);

		if (m_gap > 0) return;

		// 有効な次の台詞を開始する
		while (!m_pending.empty()) {
			m_current = m_pending.front();
			m_pending.pop_front();

			if (m_current.reaction && !alive(m_current.actor)) continue;

			const auto& clip = EnemyRadioClips[m_current.clip];
			m_remaining = clip.seconds;
			m_presentation = {
				m_current.clip, ++m_serial, {m_current.reaction ? "敵僚機" : "敵機", clip.text, "enemy"}};
			break;
		}
	}
} // namespace Game
