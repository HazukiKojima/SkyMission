#pragma once
#include "Game/Weapons/AircraftGun.h"
#include "Game/Weapons/ProjectileSystem.h"

// RegressionTests.cppのRequireと共通のGPU初期化環境を使用する。
void TestWeapons(const fs::path& repository, const fs::path& fixture) {
	using namespace DirectX;
	RawInputState raw;
	const auto mouse1 = reinterpret_cast<HANDLE>(std::uintptr_t(101));
	const auto mouse2 = reinterpret_cast<HANDLE>(std::uintptr_t(102));
	const auto mouse = [&](HANDLE device, USHORT flags) {
		RAWMOUSE packet{};
		packet.usButtonFlags = flags;
		raw.ProcessMouse(device, packet, 1920, 1080);
	};
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_DOWN);
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Inactive mouse fired");
	raw.SetActive(true);
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_DOWN);
	mouse(mouse2, RI_MOUSE_LEFT_BUTTON_DOWN);
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_UP);
	Require(raw.ConsumeFrame().leftMouseDown, "One mouse release cleared another mouse");
	raw.ForgetDevice(mouse2);
	Require(!raw.ConsumeFrame().leftMouseDown, "Disconnected mouse kept firing");
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_DOWN | RI_MOUSE_RIGHT_BUTTON_DOWN);
	const auto both = raw.ConsumeFrame();
	Require(both.leftMouseDown && both.rightMouseDown, "Left button disturbed right drag");
	raw.SetActive(false);
	Require(!raw.ConsumeFrame().leftMouseDown, "Focus loss kept firing");
	raw.SetActive(true);
	Require(!raw.ConsumeFrame().leftMouseDown, "Focus regain restored firing");
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_DOWN);
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_UP);
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Release did not stop firing");
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_DOWN);
	raw.ClearMouse();
	Require(!raw.ConsumeFrame().leftMouseDown, "Capture cancellation kept firing");
	const auto keyboard = reinterpret_cast<HANDLE>(std::uintptr_t(103));
	const auto key = [&](USHORT flags) {
		RAWKEYBOARD packet{};
		packet.VKey = 'F';
		packet.MakeCode = 0x21;
		packet.Flags = flags;
		raw.ProcessKeyboard(keyboard, packet);
	};
	key(RI_KEY_MAKE);
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "F key still fired");
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Held F still fired");
	key(RI_KEY_BREAK);
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "F break did not stop firing");
	mouse(mouse1, RI_MOUSE_LEFT_BUTTON_DOWN);
	Require(WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Left mouse did not fire");
	Require(WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Held left mouse did not keep firing");
	key(RI_KEY_MAKE);
	raw.SetActive(false);
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Focus loss did not stop mouse firing");
	raw.SetActive(true);
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Focus regain restored mouse firing");
	key(RI_KEY_MAKE);
	raw.ForgetDevice(keyboard);
	Require(!WeaponController{}.ReadInput(raw.ConsumeFrame()).fire, "Disconnected keyboard kept firing");

	const auto config = repository / L"Assets";
	const auto definition = FlightSceneLoader{}.Load({.environment = config / L"Environment/environment.json",
		.playerAircraft = config / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = config / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = config / L"Environment/retroFilm.json"});
	auto settings = definition.playerWeapon.gun;
	JsonAssetReader reader;
	Require(settings.maxProjectiles == 128 && settings.fireInterval == 0.1f &&
			definition.playerWeapon.ammunition.options.origin == ModelOrigin::BoundsCenter &&
			definition.playerWeapon.ammunition.options.normalizedExtent ==
				reader.ResolveFloat(config / L"Ammunition/20mmAmmo/modelImport.json", "normalizedExtent"),
		"Weapon/ammunition JSON was not loaded");
	for (float invalid :
		{0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
		for (auto member : {&GunSettings::fireInterval, &GunSettings::muzzleSpeed, &GunSettings::lifetime}) {
			auto bad = settings;
			bad.*member = invalid;
			bool rejected = false;
			try {
				ValidateGunSettings(bad);
			} catch (const std::invalid_argument&) {
				rejected = true;
			}
			Require(rejected, "Invalid weapon numeric setting accepted");
		}
	}
	for (bool duplicate : {false, true}) {
		auto bad = settings;
		if (duplicate)
			bad.muzzleNames[1] = bad.muzzleNames[0];
		else
			bad.maxProjectiles = 3;
		bool rejected = false;
		try {
			ValidateGunSettings(bad);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Duplicate muzzle or capacity below volley accepted");
	}
	const auto badWeaponPath = fixture / L"bad-weapon.json";
	const auto badPlayerPath = fixture / L"bad-player.json";
	const auto playerPath = config / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json";
	{
		std::ofstream player(badPlayerPath);
		player << "{\"model\":\"" << reader.Resolve(playerPath, "model").generic_string()
			   << "\",\"visualScale\":20,\"flightParameters\":\""
			   << reader.Resolve(playerPath, "flightParameters").generic_string() << "\",\"modelImport\":\""
			   << reader.Resolve(playerPath, "modelImport").generic_string() << "\",\"weapon\":\""
			   << badWeaponPath.generic_string() << "\"}";
	}
	const auto writeWeapon = [&](float capacity, float speed, const fs::path& ammunition) {
		std::ofstream weapon(badWeaponPath);
		weapon << "{\"muzzleL1\":\"GunMuzzle_L1\",\"muzzleL2\":\"GunMuzzle_L2\","
				  "\"muzzleR1\":\"GunMuzzle_R1\",\"muzzleR2\":\"GunMuzzle_R2\",\"fireInterval\":0.1,"
				  "\"lifetime\":3,\"maxProjectiles\":"
			   << capacity << ",\"muzzleSpeed\":" << speed << ",\"ammunition\":\""
			   << ammunition.generic_string() << "\"}";
	};
	const auto rejectConfig = [&](const std::string& field, const fs::path& source) {
		bool rejected = false;
		try {
			FlightSceneLoader{}.Load({.environment = config / L"Environment/environment.json",
				.playerAircraft = badPlayerPath,
				.enemyAircraft = config / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
				.retroFilm = config / L"Environment/retroFilm.json"});
		} catch (const std::runtime_error& error) {
			const std::string message = error.what();
			rejected = message.find(field) != std::string::npos &&
				message.find(source.filename().string()) != std::string::npos;
		}
		Require(rejected, "Invalid JSON configuration did not identify its field and source");
	};
	writeWeapon(4.5f, 900, "unused");
	rejectConfig("maxProjectiles", badWeaponPath);
	writeWeapon(128, 0, "unused");
	rejectConfig("muzzleSpeed", badWeaponPath);
	const auto badAmmo = fixture / L"bad-ammo.json";
	const auto badImport = fixture / L"bad-import.json";
	{
		std::ifstream source(config / L"Ammunition/20mmAmmo/modelImport.json");
		std::string text((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
		const auto position = text.find("\"normalizedExtent\"");
		Require(position != std::string::npos, "Ammo extent fixture could not be prepared");
		const auto valueStart = text.find(':', position) + 1;
		const auto valueEnd = text.find_first_of(",}", valueStart);
		text.replace(valueStart, valueEnd - valueStart, " 0.0");
		std::ofstream(badImport) << text;
		std::ofstream(badAmmo) << "{\"model\":\"unused.fbx\",\"modelImport\":\"" << badImport.generic_string()
							   << "\",\"modelForward\":\"+Y\"}";
	}
	writeWeapon(128, 900, badAmmo);
	rejectConfig("normalizedExtent", badAmmo);

	// メッシュは親の平行移動を受け、銃口はさらに子の変換を受ける。
	const auto nodeFile = fixture / L"muzzles.dae";
	const auto writeNodes = [&](bool duplicate) {
		std::ofstream file(nodeFile);
		file
			<< R"(<?xml version="1.0"?><COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
<asset><unit meter="1"/><up_axis>Y_UP</up_axis></asset><library_geometries><geometry id="mesh"><mesh>
<source id="positions"><float_array id="positions-array" count="9">0 0 0 2 0 0 0 2 0</float_array>
<technique_common><accessor source="#positions-array" count="3" stride="3"><param name="X" type="float"/><param name="Y" type="float"/><param name="Z" type="float"/></accessor></technique_common></source>
<vertices id="vertices"><input semantic="POSITION" source="#positions"/></vertices><triangles count="1"><input semantic="VERTEX" source="#vertices" offset="0"/><p>0 1 2</p></triangles>
</mesh></geometry></library_geometries><library_visual_scenes><visual_scene id="Scene"><node id="parent" name="parent"><translate>10 0 0</translate><instance_geometry url="#mesh"/>)";
		for (size_t i = 0; i < settings.muzzleNames.size(); ++i)
			file << "<node id=\"" << settings.muzzleNames[i] << "\" name=\"" << settings.muzzleNames[i]
				 << "\"><translate>1 3 0</translate></node>";
		if (duplicate)
			file << "<node id=\"" << settings.muzzleNames[0] << "\" name=\"" << settings.muzzleNames[0]
				 << "\"/>";
		file << "</node></visual_scene></library_visual_scenes><scene><instance_visual_scene "
				"url=\"#Scene\"/></scene></COLLADA>";
	};
	writeNodes(false);
	const HWND window = CreateWindowW(L"STATIC", L"Weapon regression (hidden)", WS_OVERLAPPEDWINDOW, 0, 0, 64,
		64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Weapon test window creation failed");
	{
		RenderDevice device;
		device.Initialize(window, 64, 64);
		CommandContext context;
		context.Initialize(&device);
		context.BeginFrame();
		auto model = std::make_shared<Model>();
		Require(model->Load(device.GetDevice(), context.GetCommandList(), &device, nodeFile.wstring()),
			"Node fixture import failed");
		const auto muzzle = model->FindNodePosition(settings.muzzleNames[0]);
		Require(muzzle && std::abs(muzzle->x) < 0.001f && std::abs(muzzle->y - 15.1f) < 0.001f,
			"Empty child node did not share parent/mesh normalization");
		Require(!model->FindNodePosition("missing"), "Missing node returned a position");
		AircraftGun gun;
		gun.Configure(settings, *model);
		Require(gun.Update({true}, 0.25f).size() == 3, "Low FPS did not emit three volleys");
		Require(gun.Update({false}, 0.01f).empty() && gun.Update({true}, 0.01f).empty(),
			"Repress bypassed cooldown");
		gun.Configure(settings, *model);
		size_t total = 0;
		for (int i = 0; i < 60; ++i)
			total += gun.Update({true}, 1.0f / 60.0f).size();
		Require(total == 10, "Continuous fire depends on frame rate");
		gun.Configure(settings, *model);
		Require(gun.Update({true}, -1).empty() &&
				gun.Update({true}, std::numeric_limits<float>::quiet_NaN()).empty(),
			"Invalid gun dt emitted a volley");
		const auto initial = gun.Update({true}, 0.01f);
		Require(initial.size() == 1 && std::abs(initial.front() - 0.01f) < 0.0001f,
			"Initial shot age was incorrect");
		gun.Update({false}, 0.25f);
		Require(gun.Update({true}, 0.01f).size() == 1, "Released cooldown did not advance");

		settings.muzzleSpeed = 900;
		ProjectileSystem projectiles;
		projectiles.Configure(settings, model, {0, 1, 0});
		Scene scene;
		XMFLOAT4X4 world, pose;
		XMStoreFloat4x4(&pose, XMMatrixTranslation(10, 100, 20));
		XMStoreFloat4x4(&world, XMMatrixScaling(2, 2, 2) * XMLoadFloat4x4(&pose));
		projectiles.FireVolley(scene, gun.GetMuzzles(), world, pose, {0, 0, -1}, {5, 2, -100}, 0.02f);
		Require(projectiles.GetCount() == 4 && scene.GetObjects().size() == 4,
			"Volley did not create four projectiles");
		const auto start = scene.GetObjects()[0].worldTransform;
		projectiles.Update(scene, std::numeric_limits<float>::infinity());
		projectiles.Update(scene, -1);
		Require(
			scene.GetObjects()[0].worldTransform._43 == start._43, "Invalid projectile dt changed position");
		Require(std::abs(start._41 - 10.1f) < 0.001f && std::abs(start._42 - 130.24f) < 0.001f &&
				std::abs(start._43) < 0.001f,
			"Muzzle scale, inherited velocity or partial-frame age was incorrect");
		const auto axis = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), XMLoadFloat4x4(&start));
		Require(XMVectorGetZ(axis) < -0.999f, "Ammo model forward did not point towards aircraft forward");
		projectiles.Update(scene, 0.1f);
		Require(std::abs(scene.GetObjects()[0].worldTransform._43 + 100) < 0.001f,
			"Projectile velocity changed after firing");
		for (int i = 0; i < 40; ++i)
			projectiles.FireVolley(scene, gun.GetMuzzles(), world, pose, {0, 0, -1}, {}, 0);
		Require(projectiles.GetCount() == 128, "Projectile cap did not prevent overflow");
		for (int i = 0; i < 13; ++i)
			projectiles.Update(scene, 0.25f);
		Require(projectiles.GetCount() == 0 && scene.GetObjects().empty(),
			"Expired projectiles left render objects");
		projectiles.FireVolley(scene, gun.GetMuzzles(), world, pose, {0, -1, 0}, {}, 0.2f);
		Require(projectiles.GetCount() == 0, "New projectile below sea level survived");
		projectiles.FireVolley(scene, gun.GetMuzzles(), world, pose, {0, -1, 0}, {}, 0);
		projectiles.Update(scene, 0.25f);
		Require(projectiles.GetCount() == 0, "Sea crossing did not remove existing projectiles");
		auto limited = settings;
		limited.maxProjectiles = 7;
		projectiles.Configure(limited, model, {0, 1, 0});
		XMStoreFloat4x4(
			&pose, XMMatrixRotationRollPitchYaw(0.3f, 0.7f, 0.5f) * XMMatrixTranslation(10, 100, 20));
		XMStoreFloat4x4(&world, XMMatrixScaling(2, 2, 2) * XMLoadFloat4x4(&pose));
		XMFLOAT3 rotatedForward;
		XMStoreFloat3(
			&rotatedForward, XMVector3TransformNormal(XMVectorSet(0, 0, -1, 0), XMLoadFloat4x4(&pose)));
		projectiles.FireVolley(scene, gun.GetMuzzles(), world, pose, rotatedForward, {}, 0);
		projectiles.FireVolley(scene, gun.GetMuzzles(), world, pose, rotatedForward, {}, 0);
		Require(projectiles.GetCount() == 4, "Less than four slots allowed a partial volley");
		const auto rotated = scene.GetObjects()[0].worldTransform;
		const auto rotatedAxis = XMVector3TransformNormal(XMVectorSet(0, 1, 0, 0), XMLoadFloat4x4(&rotated));
		Require(XMVectorGetX(XMVector3Dot(rotatedAxis, XMLoadFloat3(&rotatedForward))) > 0.999f,
			"Ammo did not inherit aircraft roll/pitch/yaw");
		const auto expectedPosition =
			XMVector3TransformCoord(XMLoadFloat3(&gun.GetMuzzles()[0]), XMLoadFloat4x4(&world));
		Require(std::abs(rotated._41 - XMVectorGetX(expectedPosition)) < 0.001f &&
				std::abs(rotated._43 - XMVectorGetZ(expectedPosition)) < 0.001f,
			"Rotated/scaled muzzle position did not match aircraft pose");

		// 同じ形状でも中心原点を指定するとY補正だけが変わる。
		auto centered = std::make_shared<Model>();
		ModelLoadOptions centeredOptions;
		centeredOptions.origin = ModelOrigin::BoundsCenter;
		Require(centered->Load(device.GetDevice(), context.GetCommandList(), &device, nodeFile.wstring(),
					centeredOptions),
			"Centered import failed");
		Require(std::abs(centered->FindNodePosition(settings.muzzleNames[0])->y - 10) < 0.001f,
			"Bounds-center origin was ignored");
		writeNodes(true);
		auto duplicate = std::make_shared<Model>();
		Require(duplicate->Load(device.GetDevice(), context.GetCommandList(), &device, nodeFile.wstring()),
			"Duplicate fixture import failed");
		Require(duplicate->CountNodes(settings.muzzleNames[0]) == 2 &&
				!duplicate->FindNodePosition(settings.muzzleNames[0]),
			"Duplicate nodes were ambiguous");
		bool rejected = false;
		try {
			gun.Configure(settings, *duplicate);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Duplicate model muzzle was accepted");
		auto missingSettings = settings;
		missingSettings.muzzleNames[0] = "not-exported";
		rejected = false;
		try {
			gun.Configure(missingSettings, *model);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Missing model muzzle was accepted");
		context.EndFrame();
		context.WaitForGpu();
	}
	writeNodes(false);
	DestroyWindow(window);
	std::cout << "PASS: left mouse firing, weapon settings, empty/duplicate nodes, timing, velocity, "
				 "lifetime, sea and projectile cap\n";
}
