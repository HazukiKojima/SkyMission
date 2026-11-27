#include "FlightScene.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game {
	namespace {
		constexpr float kWingCutFraction = 0.27f;
		constexpr float kFuselageCutFraction = 0.57f;
		constexpr float kDetachedPartPivotFraction = 0.4f;
		constexpr float kDetachedPartSpeedRetention = 0.68f;
		constexpr float kDetachedPartGravity = 36.0f;
		constexpr DirectX::XMFLOAT3 kDetachedPartSpinRates{3.0f, 1.8f, 4.5f};
		constexpr DirectX::XMFLOAT3 kWingSeparationVelocity{48.0f, 8.0f, -8.0f};
		constexpr DirectX::XMFLOAT3 kFuselageSeparationVelocity{30.0f, 14.0f, 54.0f};
		constexpr float kWingBreakBlastScale = 0.8f;
		constexpr float kFuselageBreakBlastScale = 1.6f;
		constexpr float kPartBreakFlashDurationScale = 1.5f;
		constexpr float kWingImpactBlastScale = 0.85f;
		constexpr float kFuselageImpactBlastScale = 1.7f;
		constexpr float kEnginePositionFraction = 0.20f;
		constexpr float kEngineBurstBaseBlastScale = 1.5f;
		constexpr float kEngineBurstFlashDurationScale = 1.65f;
		constexpr float kFuelFireBaseWingFraction = 0.24f;
		constexpr float kFuelFireWingSpreadFraction = 0.50f;
		constexpr float kFuelFireInitialSize = 0.8f;
		constexpr float kFuelFireSizeIncrease = 1.35f;
		constexpr float kFuelFireBlastScale = 1.6f;
		constexpr float kFuelFireFlashDurationScale = 2.0f;
		constexpr float kFuselageBreakProgress = 0.53f;
		constexpr float kWingPartExplosionProgress = 0.66f;
		constexpr float kFuselagePartExplosionProgress = 0.86f;
		constexpr float kSpinPitchRadians = -0.85f;
		constexpr float kSpinPitchOscillationRadians = 0.16f;
		constexpr float kSpinPitchOscillationRate = 5.0f;
		constexpr float kShallowestDivePitchRadians = -0.50f;
		constexpr float kCrashAttitudeBlendRate = 18.0f;

		std::optional<float> GetLowestAircraftY(const Engine::SceneObject& aircraft) {
			using namespace DirectX;
			if (!aircraft.model) return std::nullopt;
			auto bounds = aircraft.model->GetLocalBounds();
			if (!bounds) return std::nullopt;
			const auto& plane = aircraft.localClipPlane;
			if (plane.x > 0) bounds->minimum.x = -plane.w;
			if (plane.x < 0) bounds->maximum.x = plane.w;
			if (plane.z > 0) bounds->minimum.z = -plane.w;
			if (plane.z < 0) bounds->maximum.z = plane.w;

			float lowestY = std::numeric_limits<float>::infinity();
			const auto world = XMLoadFloat4x4(&aircraft.worldTransform);
			for (int corner = 0; corner < 8; ++corner) {
				const auto point = XMVector3TransformCoord(XMVectorSet(
					corner & 1 ? bounds->maximum.x : bounds->minimum.x,
					corner & 2 ? bounds->maximum.y : bounds->minimum.y,
					corner & 4 ? bounds->maximum.z : bounds->minimum.z, 1), world);
				lowestY = std::min(lowestY, XMVectorGetY(point));
			}
			return lowestY;
		}

	} // namespace

	void FlightScene::BeginEnemyCrash(EnemyAircraft& enemy, const HitEvent& hit) {
		using namespace DirectX;
		enemy.crashVariant = m_crashSequence++;
		enemy.crashEffect = static_cast<CrashEffect>(
			enemy.crashVariant % static_cast<unsigned>(CrashEffect::Count));
		++m_combat.enemyAircraftDestroyed;
		enemy.crashVelocity = enemy.flightModel.GetVelocity();
		const auto& settings = m_effects.GetSettings().aircraftCrash;
		enemy.crashVelocity.x *= settings.horizontalSpeedRetention;
		enemy.crashVelocity.z *= settings.horizontalSpeedRetention;
		enemy.crashVelocity.y = std::min(enemy.crashVelocity.y, 0.0f) - settings.initialDownwardSpeed;
		if (const auto* object = m_scene.FindObject(enemy.objectId)) {
			const auto inverse = XMMatrixInverse(nullptr, XMLoadFloat4x4(&object->worldTransform));
			XMStoreFloat3(&enemy.crashSmokeOrigin,
				XMVector3TransformCoord(XMLoadFloat3(&hit.position), inverse));
		}
		// 爆煙は被弾地点に残し、燃えている機体だけがそこから落ちていく
		m_effects.SpawnExplosion(hit.position, {}, settings.initialExplosionRadiusMultiplier,
			settings.initialFlashDurationMultiplier, ExplosionStyle::BacklitSmoke);
		m_effects.SpawnAircraftSmoke({hit.position, 0});
		if (enemy.crashEffect == CrashEffect::WingBreak) DetachAircraftPart(enemy, true);
		if (hit.kind == ProjectileKind::Gun) ++m_combat.audio.gunHits;
		else ++m_combat.audio.missileHits;
	}

	void FlightScene::DetachAircraftPart(EnemyAircraft& enemy, bool wing) {
		using namespace DirectX;
		auto* object = m_scene.FindObject(enemy.objectId);
		if (!object || !object->model) return;
		const auto bounds = object->model->GetLocalBounds();
		if (!bounds) return;

		const auto center = bounds->Center();
		const float side = enemy.crashSmokeOrigin.x < center.x ? -1.0f : 1.0f;
		const float cut = wing
			? std::lerp(center.x, side < 0 ? bounds->minimum.x : bounds->maximum.x, kWingCutFraction)
			: std::lerp(bounds->minimum.z, bounds->maximum.z, kFuselageCutFraction);
		const XMFLOAT4 plane = wing ? XMFLOAT4{side, 0, 0, -side * cut} : XMFLOAT4{0, 0, 1, -cut};
		object->localClipPlane = {-plane.x, -plane.y, -plane.z, -plane.w};
		// 火元を機体側に残し、切り離した側の空間に炎が浮かないようにする
		if (wing && side * (enemy.crashSmokeOrigin.x - cut) > 0) {
			enemy.crashSmokeOrigin.x = cut;
		} else if (!wing) {
			enemy.crashSmokeOrigin.z = std::min(enemy.crashSmokeOrigin.z, cut);
		}
		auto part = *object;
		part.localClipPlane = plane;
		enemy.detachedPartPivot = wing
			? XMFLOAT3{std::lerp(cut, side < 0 ? bounds->minimum.x : bounds->maximum.x, kDetachedPartPivotFraction), center.y, center.z}
			: XMFLOAT3{center.x, center.y, std::lerp(cut, bounds->maximum.z, kDetachedPartPivotFraction)};

		const auto world = XMLoadFloat4x4(&part.worldTransform);
		const auto separation = wing ? kWingSeparationVelocity : kFuselageSeparationVelocity;
		const auto localKick = XMVectorSet(side * separation.x, separation.y, separation.z, 0);
		const auto kick = XMVector3TransformNormal(localKick, world) / enemy.visualScale;
		XMStoreFloat3(&enemy.detachedPartVelocity, XMLoadFloat3(&enemy.crashVelocity) * kDetachedPartSpeedRetention + kick);
		enemy.detachedPartId = m_scene.AddObject(part.model, part.worldTransform);
		part.id = enemy.detachedPartId;
		*m_scene.FindObject(enemy.detachedPartId) = std::move(part);

		XMFLOAT3 breakPoint;
		XMStoreFloat3(&breakPoint, XMVector3TransformCoord(
			wing ? XMVectorSet(cut, center.y, center.z, 1) : XMVectorSet(center.x, center.y, cut, 1), world));
		m_effects.SpawnExplosion(breakPoint, {}, wing ? kWingBreakBlastScale : kFuselageBreakBlastScale,
			kPartBreakFlashDurationScale);
		if (!wing) ++m_combat.audio.missileHits;
	}

	void FlightScene::UpdateDetachedAircraftPart(EnemyAircraft& enemy, float dt, bool explode) {
		using namespace DirectX;
		auto* part = m_scene.FindObject(enemy.detachedPartId);
		if (!part) return;

		const auto pivot = XMLoadFloat3(&enemy.detachedPartPivot);
		const auto world = XMLoadFloat4x4(&part->worldTransform);
		const auto spin = XMMatrixTranslationFromVector(-pivot) *
			XMMatrixRotationRollPitchYaw(
				dt * kDetachedPartSpinRates.x, dt * kDetachedPartSpinRates.y, dt * kDetachedPartSpinRates.z) *
			XMMatrixTranslationFromVector(pivot);
		auto& velocity = enemy.detachedPartVelocity;
		const auto movement = XMVectorSet(velocity.x * dt, (velocity.y - 0.5f * kDetachedPartGravity * dt) * dt, velocity.z * dt, 0);
		const auto nextWorld = spin * world * XMMatrixTranslationFromVector(movement);
		XMStoreFloat4x4(&part->worldTransform, nextWorld);
		velocity.y -= kDetachedPartGravity * dt;

		XMFLOAT3 position;
		XMStoreFloat3(&position, XMVector3TransformCoord(pivot, nextWorld));
		if (!explode && position.y > 0) return;

		position.y = std::max(position.y, 0.0f);
		m_effects.SpawnExplosion(position, {}, enemy.crashEffect == CrashEffect::FuselageBreak
			? kFuselageImpactBlastScale : kWingImpactBlastScale);
		++m_combat.audio.aircraftExplosions;
		m_scene.RemoveObject(enemy.detachedPartId);
		enemy.detachedPartId = Engine::InvalidObjectId;
	}

	void FlightScene::UpdateCrashEffect(EnemyAircraft& enemy, const Engine::SceneObject& object,
		float previousSeconds, float duration) {
		using namespace DirectX;
		if (!object.model) return;
		const auto bounds = object.model->GetLocalBounds();
		if (!bounds) return;
		const auto world = XMLoadFloat4x4(&object.worldTransform);
		const auto center = bounds->Center();
		const float progress = enemy.crashSeconds / duration;
		const auto& settings = m_effects.GetSettings().aircraftCrash;

		if (enemy.crashEffect == CrashEffect::EngineBursts) {
			const XMFLOAT3 engine{center.x, center.y, std::lerp(bounds->minimum.z, center.z, kEnginePositionFraction)};
			XMFLOAT3 position;
			XMStoreFloat3(&position, XMVector3TransformCoord(XMLoadFloat3(&engine), world));
			for (const float fraction : settings.engineBurstProgress) {
				if (previousSeconds < duration * fraction && progress >= fraction) {
					m_effects.SpawnExplosion(position, {}, kEngineBurstBaseBlastScale + fraction, kEngineBurstFlashDurationScale);
					m_effects.SpawnAircraftSmoke({position, 0});
					++m_combat.audio.missileHits;
				}
			}
		} else if (enemy.crashEffect == CrashEffect::FuelFire) {
			constexpr float fuelFireSpreadProgressRange = 0.65f;
			const float spread = std::clamp(
				(progress - settings.fuelFireStartProgress) / fuelFireSpreadProgressRange, 0.0f, 1.0f);
			if (spread <= 0) return;
			const float side = enemy.crashSmokeOrigin.x < center.x ? -1.0f : 1.0f;
			const float wingtip = side < 0 ? bounds->minimum.x : bounds->maximum.x;
			const auto position = XMVectorSet(std::lerp(center.x, wingtip, kFuelFireBaseWingFraction + kFuelFireWingSpreadFraction * spread),
				center.y, center.z, 1);
			AircraftFire fire;
			XMStoreFloat3(&fire.position, XMVector3TransformCoord(position, world));
			XMStoreFloat3(&fire.direction, XMVector3Normalize(XMLoadFloat3(&enemy.crashVelocity)));
			fire.seconds = enemy.crashSeconds;
			fire.size = kFuelFireInitialSize + spread * kFuelFireSizeIncrease;
			m_effects.AddAircraftFire(fire);
			if (previousSeconds < duration * settings.fuelFireFlashProgress &&
				progress >= settings.fuelFireFlashProgress)
				m_effects.SpawnExplosion(fire.position, {}, kFuelFireBlastScale, kFuelFireFlashDurationScale);
		}
	}

	void FlightScene::UpdateCrashingEnemies(float dt) {
		using namespace DirectX;
		const auto& settings = m_effects.GetSettings().aircraftCrash;

		std::erase_if(m_enemies, [&](EnemyAircraft& enemy) {
			if (enemy.health > 0) return false;
			const auto& profile = settings.profiles[enemy.crashVariant % settings.profiles.size()];
			const float duration = profile.durationSeconds;
			const float previousSeconds = enemy.crashSeconds;
			const float step = std::min(dt, duration - previousSeconds);
			const float breakupTime = duration * kFuselageBreakProgress;
			const float partExplosionTime = duration * (enemy.crashEffect == CrashEffect::WingBreak
				? kWingPartExplosionProgress : kFuselagePartExplosionProgress);
			UpdateDetachedAircraftPart(enemy, step, previousSeconds + step >= partExplosionTime);
			if (enemy.crashEffect == CrashEffect::FuselageBreak &&
				previousSeconds < breakupTime && previousSeconds + step >= breakupTime)
				DetachAircraftPart(enemy, false);
			auto* object = m_scene.FindObject(enemy.objectId);
			if (object) {
				auto& world = object->worldTransform;
				const auto smokeOrigin = XMLoadFloat3(&enemy.crashSmokeOrigin);
				const auto previousSmoke = XMVector3TransformCoord(smokeOrigin, XMLoadFloat4x4(&world));
				auto& velocity = enemy.crashVelocity;
				const float side = enemy.crashEffect == CrashEffect::WingBreak && object->localClipPlane.x != 0
					? object->localClipPlane.x : ((enemy.crashVariant / settings.profiles.size()) % 2 ? -1.0f : 1.0f);
				enemy.crashSeconds += step;
				const XMFLOAT3 previousPosition{world._41, world._42, world._43};
				const auto previousVelocity = XMLoadFloat3(&velocity);
				const auto horizontalVelocity = XMVector3TransformNormal(
					XMVectorSet(velocity.x, 0, velocity.z, 0),
					XMMatrixRotationY(side * profile.turnRate * step)) * std::exp(-profile.drag * step);
				const auto nextVelocity = XMVectorSetY(horizontalVelocity,
					velocity.y - profile.downwardAcceleration * step);
				XMStoreFloat3(&velocity, nextVelocity);
				XMFLOAT3 position;
				XMStoreFloat3(&position, XMLoadFloat3(&previousPosition) + (previousVelocity + nextVelocity) * (0.5f * step));

				const float divePitch = std::atan2(velocity.y, std::hypot(velocity.x, velocity.z));
				const float pitch = profile.spinRate > 0 ? kSpinPitchRadians + kSpinPitchOscillationRadians *
						std::sin(enemy.crashSeconds * kSpinPitchOscillationRate)
					: std::min(divePitch, kShallowestDivePitchRadians);
				const float yaw = std::atan2(-velocity.x, -velocity.z) + side * profile.spinRate * enemy.crashSeconds;
				const float roll = side * (profile.bank + profile.rollRate * enemy.crashSeconds);
				const auto flightPose = enemy.flightModel.GetWorldTransform();
				const auto initial = XMQuaternionRotationMatrix(XMLoadFloat4x4(&flightPose));
				const auto crashAttitude = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
				const float blend = 1.0f - std::exp(-enemy.crashSeconds * kCrashAttitudeBlendRate);
				const float scale = enemy.visualScale;
				XMStoreFloat4x4(&world, XMMatrixScaling(scale, scale, scale) *
					XMMatrixRotationQuaternion(XMQuaternionSlerp(initial, crashAttitude, blend)));
				world._41 = position.x;
				world._42 = position.y;
				world._43 = position.z;

				const float lowestY = GetLowestAircraftY(*object).value_or(position.y);
				if (lowestY > 0 && enemy.crashSeconds < duration) {
					UpdateCrashEffect(enemy, *object, previousSeconds, duration);
					const auto nextSmoke = XMVector3TransformCoord(smokeOrigin, XMLoadFloat4x4(&world));
					AircraftFire fire;
					XMStoreFloat3(&fire.position, nextSmoke);
					XMStoreFloat3(&fire.direction, XMVector3Normalize(nextVelocity));
					fire.seconds = enemy.crashSeconds;
					m_effects.AddAircraftFire(fire);
					m_effects.EmitAircraftSmokeTrail(previousSmoke, nextSmoke, previousSeconds, enemy.crashSeconds);
					return false;
				}

				world._42 -= std::min(lowestY, 0.0f);
				m_effects.SpawnExplosion(*object, lowestY > 0 ? velocity : XMFLOAT3{}, scale,
					settings.finalExplosionRadiusMultiplier, ExplosionStyle::FallingDebris);
				++m_combat.audio.aircraftExplosions;
				m_scene.RemoveObject(enemy.objectId);
			}
			UpdateDetachedAircraftPart(enemy, 0, true);
			m_enemyRadio.NotifyCrashImpact(enemy.objectId);
			return true;
		});
	}

	void FlightScene::ApplySeaCollision() {
		const auto* aircraft = m_scene.FindObject(m_playerId);
		if (m_combat.playerHealth <= 0 || !aircraft) return;

		const float lowestY = GetLowestAircraftY(*aircraft).value_or(std::numeric_limits<float>::infinity());
		if (lowestY > 0 || !std::isfinite(lowestY)) return;

		// 大きい更新幅でも爆発を海中に埋めず、機体下端を海面に合わせる
		auto impact = *aircraft;
		impact.worldTransform._42 -= lowestY;
		auto position = m_player.GetPosition();
		position.y -= lowestY;
		m_player.SetPosition(position);
		m_effects.SpawnExplosion(impact, m_player.GetVelocity(), m_playerVisualScale);
		++m_combat.audio.aircraftExplosions;
		m_combat.playerHealth = 0;
		RemoveObject(m_playerId);
		m_combat.result = CombatResult::Defeat;
		m_carrierLookSeconds = 0;

		// 墜落の爆発を表示しながら、司令官の無線を流す
		m_endingPlayed = true;
		StartCommandRadio("defeat");
	}

} // namespace Game
