#pragma once

namespace Engine {
	struct AircraftFlightParameters {
		// 速度はワールド単位毎秒、加速度はワールド単位毎秒二乗。
		float initialSpeed = 60.0f;
		float minimumSpeed = 120.0f;
		float maximumSpeed = 150.0f;
		float acceleration = 60.0f;
		float drag = 0.035f;
		// 姿勢変化率やバンク角。角度はラジアン。
		float maximumBankRadians = 1.308996939f;
		float pitchRateRadians = 0.872664626f;
		float turnRateRadians = 0.95f;
		float yawRateRadians = 0.45f;
		float rollLevelRate = 2.0f;
		float controlResponse = 80.0f;
		float rollInputResponse = 1.8f;
		float minimumTurnFactor = 0.55f;
		float maximumTurnFactor = 1.4f;
		// ハイG旋回時の旋回倍率と追加抗力。
		float highGTurnMultiplier = 1.8f;
		float highGDrag = 0.35f;
		// 重力・失速・揚力の特性。
		float gravity = 9.81f;
		float stallSpeed = 28.0f;
		float liftReferenceSpeed = 60.0f;
		float maximumLiftFactor = 4.0f;
		float stallAngleRadians = 0.610865238f;
		float criticalAngleRadians = 1.308996939f;
		// バンク旋回による抗力と、飛行経路を機首方向へ追従させる強さ。
		float turnDrag = 0.32f;
		float flightPathResponse = 1.8f;
	};
}
