#include "FlightScene.h"
namespace Game {
	// 再挑戦で戻すシミュレーションと表示状態だけを明示する。
	// 画面寸法、再挑戦用データ自体、外部のRenderer/Applicationは含めない。
	struct FlightScene::RestartState {
		FlightMission mission;
		CarrierMissileSystem carrierMissiles;
		ProjectileSystem hostileProjectiles;
		GunSettings enemyGunSettings;
		float commanderGunInterval;
		float battleSeconds;
		DirectX::XMFLOAT3 previousPlayerPosition;
		Engine::Scene scene;
		AircraftCameraController cameraController;
		AircraftFlightModel player;
		Engine::ObjectId playerId;
		AircraftAnimationSettings playerAnimation, enemyAnimation;
		float playerVisualScale;
		float playerPropellerAngle;
		std::vector<EnemyAircraft> enemies;
		AircraftGun gun;
		ProjectileSystem projectiles;
		bool weaponConfigured;
		GunSettings gunSettings;
		CombatSettings combatSettings;
		TargetingSystem targeting;
		CombatSnapshot combat;
		CombatEffectSystem effects;
		size_t enemySequence;
	};
	void FlightScene::SaveInitialState() {
		m_initialState = std::make_shared<const RestartState>(RestartState{.mission = m_mission,
			.carrierMissiles = m_carrierMissiles,
			.hostileProjectiles = m_hostileProjectiles,
			.enemyGunSettings = m_enemyGunSettings,
			.commanderGunInterval = m_commanderGunInterval,
			.battleSeconds = m_battleSeconds,
			.previousPlayerPosition = m_previousPlayerPosition,
			.scene = m_scene,
			.cameraController = m_cameraController,
			.player = m_player,
			.playerId = m_playerId,
			.playerAnimation = m_playerAnimation,
			.enemyAnimation = m_enemyAnimation,
			.playerVisualScale = m_playerVisualScale,
			.playerPropellerAngle = m_playerPropellerAngle,
			.enemies = m_enemies,
			.gun = m_gun,
			.projectiles = m_projectiles,
			.weaponConfigured = m_weaponConfigured,
			.gunSettings = m_gunSettings,
			.combatSettings = m_combatSettings,
			.targeting = m_targeting,
			.combat = m_combat,
			.effects = m_effects,
			.enemySequence = m_enemySequence});
	}
	void FlightScene::Restart(float dt) {
		const auto& initial = *m_initialState;
		m_mission = initial.mission;
		m_carrierMissiles = initial.carrierMissiles;
		m_hostileProjectiles = initial.hostileProjectiles;
		m_enemyGunSettings = initial.enemyGunSettings;
		m_commanderGunInterval = initial.commanderGunInterval;
		m_battleSeconds = initial.battleSeconds;
		m_previousPlayerPosition = initial.previousPlayerPosition;
		m_scene = initial.scene;
		m_cameraController = initial.cameraController;
		m_player = initial.player;
		m_playerId = initial.playerId;
		m_playerAnimation = initial.playerAnimation;
		m_enemyAnimation = initial.enemyAnimation;
		m_playerVisualScale = initial.playerVisualScale;
		m_playerPropellerAngle = initial.playerPropellerAngle;
		m_enemies = initial.enemies;
		m_gun = initial.gun;
		m_projectiles = initial.projectiles;
		m_weaponConfigured = initial.weaponConfigured;
		m_gunSettings = initial.gunSettings;
		m_combatSettings = initial.combatSettings;
		m_targeting = initial.targeting;
		m_combat = initial.combat;
		m_effects = initial.effects;
		m_enemySequence = initial.enemySequence;
		OnResize(m_width, m_height);
		ApplyRenderStates();
		RefreshCombatSnapshot();
		m_combat.audio.restarted = true;
		m_combat.audio.flying = true;
		UpdateCamera({}, dt, {});
		m_midMissionPlayed = m_endingPlayed = false;
		m_cinematic = {};
		m_combat.cinematic = {};
		if (m_openingEnabled) StartOpening();
	}
} // namespace Game
