#pragma once

namespace Game {
	// 長いフレームの経過時間を一度に進めないための上限。
	inline constexpr float kMaximumSimulationDeltaSeconds = 0.25f;
}
