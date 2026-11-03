#include "CombatSnapshotBuilder.h"
#include "GunAimSolver.h"
#include <algorithm>
namespace Game {
	void BuildCombatSnapshot(CombatSnapshot& snapshot, const CombatSnapshotInput& input) {
		using namespace DirectX;
		snapshot.playerSpeedMetersPerSecond = input.player.GetSpeed();
		snapshot.playerAltitudeMeters = input.player.GetPosition().y;
		snapshot.playerPosition = input.player.GetPosition();
		snapshot.playerHeading = input.player.GetHeading();
		snapshot.aim.reset();
		snapshot.assistReady = false;
		for (const auto& target : snapshot.enemies)
			if (target.id == snapshot.selected && input.weaponConfigured) {
				snapshot.aim = SolveGunAim(snapshot.muzzle, input.player.GetVelocity(), target,
					input.gun.muzzleSpeed, input.gun.lifetime);
				snapshot.assistReady = snapshot.lockState == LockState::Locked && snapshot.aim &&
					XMVectorGetX(XMVector3Length(XMLoadFloat3(&target.center) -
						XMLoadFloat3(&snapshot.muzzle))) <= input.combat.targeting.acquireDistance &&
					IsWithinAngle(
						snapshot.forward, snapshot.aim->direction, input.combat.targeting.assistAngleDegrees);
			}
		snapshot.gunImpactPoint.reset();
		if (input.weaponConfigured) {
			// 一定距離の照準位置は、対象選択や予測補正とは独立して計算する
			const auto direction = XMLoadFloat3(&snapshot.forward);
			const auto aircraftVelocity = input.player.GetVelocity();
			const auto velocity = XMLoadFloat3(&aircraftVelocity) + direction * input.gun.muzzleSpeed;
			constexpr float distance = CombatSnapshot::gunSightDistance;
			const float speed = XMVectorGetX(XMVector3Length(velocity));
			if (speed > 0.001f) {
				float seconds = std::min(distance / speed, input.gun.lifetime);
				const float vy = XMVectorGetY(velocity);
				if (vy < 0)
					seconds = std::min(seconds, std::max(0.0f, -snapshot.muzzle.y / vy));
				XMFLOAT3 point;
				XMStoreFloat3(&point, XMLoadFloat3(&snapshot.muzzle) + velocity * seconds);
				snapshot.gunImpactPoint = point;
			}
		}
	}
} // namespace Game
