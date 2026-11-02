#pragma once

void TestShips(const fs::path& repository, const fs::path& fixture) {
	const auto source = repository / L"Assets/Ships/ships.json";
	const auto ships = LoadShipDefinitions(JsonAssetReader{}, source);
	Require(ships.size() == 3, "Expected three ships");
	for (const auto& ship : ships) {
		Require(fs::is_regular_file(ship.asset.file), "Ship model relative path did not resolve");
		Require(ship.asset.options.normalizedExtent == 1 && ship.asset.options.groundOffset == 0,
			"Ship normalization must allow world size and waterline placement");
		const auto transform = ship.BuildTransform();
		Require(transform._41 == ship.position.x && transform._43 == ship.position.z &&
				std::abs(transform._42 - (ship.position.y + ship.waterlineOffset)) < 0.001f,
			"Ship transform lost position or waterline offset");
	}
	const auto path = fixture / L"ships.json";
	const auto write = [&](const char* size, const char* position, const char* rotation = "[0,0,0]",
						   const char* waterline = "-10") {
		std::ofstream(path) << "{\"shipCount\":1,\"ship1Model\":\"ship.fbx\",\"ship1Position\":" << position
							<< ",\"ship1RotationDegrees\":" << rotation << ",\"ship1Size\":" << size
							<< ",\"ship1WaterlineOffset\":" << waterline << "}";
	};
	write("100", "[1,2,3]", "[0,90,0]");
	const auto ship = LoadShipDefinitions(JsonAssetReader{}, path).front();
	Require(ship.asset.file == fixture / L"ship.fbx", "Ship path must resolve relative to JSON");
	const auto transform = ship.BuildTransform();
	Require(std::abs(transform._11) < 0.001f && std::abs(transform._13 + 100) < 0.001f,
		"Ship size or rotation was ignored");
	const auto reject = [&](const char* size, const char* position, const char* rotation,
							const char* waterline) {
		write(size, position, rotation, waterline);
		bool rejected = false;
		try {
			LoadShipDefinitions(JsonAssetReader{}, path);
		} catch (const std::runtime_error& error) {
			rejected = std::string(error.what()).find("ships.json") != std::string::npos;
		}
		Require(rejected, "Invalid ship settings accepted or source path omitted");
	};
	for (const auto* size : {"0", "-1", "1e100"})
		reject(size, "[0,0,0]", "[0,0,0]", "0");
	reject("1", "[0,1e100,0]", "[0,0,0]", "0");
	reject("1", "[0,0]", "[0,0,0]", "0");
	reject("1", "[0,0,0]", "[0,1e100,0]", "0");
	reject("1", "[0,0,0]", "[0,0,0]", "1e100");
	const auto images = fixture / L"ship-textures";
	fs::create_directories(images / L"Textures");
	std::ofstream(images / L"Textures/hull.jpeg") << "fixture";
	Require(MaterialTexturePaths::FindTexturePath(images, L"C:/export/hull.jpg", {}) ==
			images / L"Textures/hull.jpeg",
		"FBX jpg reference did not find supplied jpeg");
	Require(MaterialTexturePaths::FindTexturePath(images, L"missing.jpg", {}).empty(),
		"Missing texture should use the existing fallback");
	std::cout << "PASS: ship configuration, relative paths, transforms, validation and JPEG aliases\n";
}
