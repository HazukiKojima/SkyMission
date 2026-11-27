#include "AircraftGun.h"
#include "Game/Settings/SimulationTiming.h"
#include "Engine/Resources/Mesh/Model.h"
#include <algorithm>

namespace Game {
	void AircraftGun::Configure(const GunSettings& settings, const Engine::Model& model) {
		ValidateGunSettings(settings);

		// 各銃口が一意の有効なノードか確認する
		std::vector<DirectX::XMFLOAT3> muzzles(settings.muzzleNames.size());

		for (size_t i = 0; i < muzzles.size(); ++i) {
			if (model.CountNodes(settings.muzzleNames[i]) != 1)
				throw std::invalid_argument(
					"muzzleNames: expected exactly one node named '" + settings.muzzleNames[i] + "'");
			muzzles[i] = *model.FindNodePosition(settings.muzzleNames[i]);

			if (!std::isfinite(muzzles[i].x) || !std::isfinite(muzzles[i].y) || !std::isfinite(muzzles[i].z))
				throw std::invalid_argument(
					"muzzleNames: nonfinite node position for '" + settings.muzzleNames[i] + "'");
		}

		// 全銃口の検証後に設定を確定する
		m_muzzles = std::move(muzzles);
		m_settings = settings;
		m_cooldown = 0;
	}

	std::vector<float> AircraftGun::Update(WeaponInput input, float dt) {
		std::vector<float> ages;

		if (!std::isfinite(dt) || dt <= 0) return ages;

		const double seconds = std::min(dt, kMaximumSimulationDeltaSeconds);

		if (!input.fire) {
			m_cooldown = std::max(0.0, m_cooldown - seconds);
			return ages;
		}

		// 半開区間[0, seconds)で発射。境界の発射は次フレームに一度だけ行う
		const double interval = m_settings.fireInterval;

		// floatのフレーム時間の累積誤差で、境界直前に余分な一斉射を生成しない
		const double epsilon = std::min(1e-7, interval * 1e-5);

		if (m_cooldown < seconds - epsilon && !m_muzzles.empty()) {
			const double count = std::ceil((seconds - m_cooldown - epsilon) / interval);
			const auto generated =
				static_cast<size_t>(std::min(count, static_cast<double>(m_settings.maxProjectiles / m_muzzles.size())));
			ages.reserve(generated);

			for (size_t i = 0; i < generated; ++i)
				ages.push_back(static_cast<float>(seconds - (m_cooldown + i * interval)));

			// 容量を超える要求は持ち越さず、発射時刻だけ進める
			m_cooldown += count * interval;
		}

		m_cooldown = std::max(0.0, m_cooldown - seconds);
		return ages;
	}
} // namespace Game
