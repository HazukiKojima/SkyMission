#include "GunAimSolver.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Game {
	using namespace DirectX;
	bool IsWithinAngle(const XMFLOAT3& forward, const XMFLOAT3& direction, float degrees) {
		const auto a = XMLoadFloat3(&forward);
		const auto b = XMLoadFloat3(&direction);
		if (XMVectorGetX(XMVector3LengthSq(a)) < 1e-8f || XMVectorGetX(XMVector3LengthSq(b)) < 1e-8f)
			return false;
		return XMVectorGetX(XMVector3Dot(XMVector3Normalize(a), XMVector3Normalize(b))) >=
			std::cos(XMConvertToRadians(degrees)) - 1e-6f;
	}

	std::optional<AimSolution> SolveGunAim(const XMFLOAT3& muzzle, const XMFLOAT3& shooterVelocity,
		const CombatTarget& target, float speed, float lifetime) {
		if (!std::isfinite(speed) || speed <= 0 || !std::isfinite(lifetime) || lifetime <= 0)
			return std::nullopt;
		const double rx = double(target.center.x) - muzzle.x;
		const double ry = double(target.center.y) - muzzle.y;
		const double rz = double(target.center.z) - muzzle.z;
		const double ux = double(target.velocity.x) - shooterVelocity.x;
		const double uy = double(target.velocity.y) - shooterVelocity.y;
		const double uz = double(target.velocity.z) - shooterVelocity.z;
		const double a = ux * ux + uy * uy + uz * uz - double(speed) * speed;
		const double b = 2 * (rx * ux + ry * uy + rz * uz);
		const double c = rx * rx + ry * ry + rz * rz;
		double time = std::numeric_limits<double>::infinity();
		if (std::abs(a) <= 1e-8 * (double(speed) * speed + 1)) {
			if (std::abs(b) > 1e-8)
				time = -c / b;
		} else {
			const double discriminant = b * b - 4 * a * c;
			if (discriminant < 0)
				return std::nullopt;
			const double q = -0.5 * (b + std::copysign(std::sqrt(discriminant), b));
			const double t1 = q / a;
			const double t2 = q == 0 ? time : c / q;
			if (t1 > 1e-6)
				time = t1;
			if (t2 > 1e-6)
				time = std::min(time, t2);
		}
		if (!std::isfinite(time) || time <= 1e-6 || time > lifetime)
			return std::nullopt;
		const auto relative =
			XMVectorSet(float(rx + ux * time), float(ry + uy * time), float(rz + uz * time), 0);
		AimSolution solution;
		XMStoreFloat3(&solution.direction, XMVector3Normalize(relative));
		XMStoreFloat3(&solution.aimPoint, XMLoadFloat3(&muzzle) + relative);
		solution.seconds = float(time);
		return solution;
	}
} // namespace Game
