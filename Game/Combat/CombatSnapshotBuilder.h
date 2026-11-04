#pragma once
#include "CombatTypes.h"
#include "Game/Aircraft/AircraftFlightModel.h"
#include "Game/Weapons/WeaponDefinition.h"
namespace Game {
	struct CombatSnapshotInput {
		const AircraftFlightModel& player;
		const GunSettings& gun;
		const CombatSettings& combat;
		bool weaponConfigured;
	};
	// 最終的な対象一覧・捕捉状態からHUD用の飛行情報と照準を組み立てる。
	// シミュレーションやTargetingSystemは変更しない。
	void BuildCombatSnapshot(CombatSnapshot& snapshot, const CombatSnapshotInput& input);
} // namespace Game
