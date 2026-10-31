#pragma once
#include "Engine/Renderer/Device/OceanGeometry.h"

void TestEnvironmentSettings(const fs::path& repository, const fs::path& fixture) {
	const auto assets = repository / L"Assets";
	const auto root = fixture / L"environment-validation";
	fs::create_directories(root / L"Environment");
	fs::create_directories(root / L"Combat");
	fs::create_directories(root / L"UI");
	fs::copy_file(assets / L"Combat/targeting.json", root / L"Combat/targeting.json",
		fs::copy_options::overwrite_existing);
	fs::copy_file(
		assets / L"UI/combatHud.json", root / L"UI/combatHud.json", fs::copy_options::overwrite_existing);
	const auto oceanPath = root / L"Environment/ocean.json";
	const auto skyPath = root / L"Environment/sky.json";
	const auto environmentPath = root / L"Environment/environment.json";
	std::ofstream(environmentPath) << R"({"ocean":"ocean.json","sky":"sky.json"})";
	const auto read = [](const fs::path& path) {
		std::ifstream file(path);
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	};
	const auto originalOcean = read(assets / L"Environment/Ocean/Ocean.json");
	const auto originalSky = read(assets / L"Environment/Sky/sky.json");
	const auto load = [&]() {
		return FlightSceneLoader{}.Load({.environment = environmentPath,
			.playerAircraft = assets / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
			.enemyAircraft = assets / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
			.retroFilm = assets / L"Environment/retroFilm.json"});
	};
	const auto replace = [](std::string text, const std::string& field, const std::string& value) {
		const auto key = text.find(char(34) + field + char(34));
		Require(key != std::string::npos, "Environment fixture field missing");
		const auto begin = text.find(':', key) + 1;
		const auto first = text.find_first_not_of(" \t\r\n", begin);
		const auto end = text[first] == '[' ? text.find(']', first) + 1 : text.find_first_of(",}", first);
		text.replace(begin, end - begin, " " + value);
		return text;
	};
	const auto reject = [&](bool ocean, const std::string& field, const std::string& value) {
		std::ofstream(oceanPath) << (ocean ? replace(originalOcean, field, value) : originalOcean);
		std::ofstream(skyPath) << (!ocean ? replace(originalSky, field, value) : originalSky);
		bool rejected = false;
		try {
			load();
		} catch (const std::runtime_error& error) {
			const std::string message = error.what();
			rejected = message.find(field) != std::string::npos &&
				message.find(ocean ? "ocean.json" : "sky.json") != std::string::npos;
		}
		Require(rejected, "Invalid environment value did not report field and source file");
	};
	for (const auto& field :
		{"size", "waveLength", "normalUvScale", "wavePatternUvScale", "edgeFadeWidth", "fogTop"})
		reject(true, field, "0");
	reject(true, "waveLength", "-1");
	reject(true, "waveSteepness", "-1");
	reject(true, "fogDensity", "-1");
	reject(true, "fogEndDistance", "0");
	reject(true, "edgeFadeStart", "1");
	reject(true, "edgeFadeWidth", "2");
	reject(false, "radius", "0");
	reject(false, "sunDirection", "[0,0,0]");
	reject(false, "sunIntensity", "-1");
	reject(false, "sunColor", "[1,-1,1]");
	reject(false, "ambientColor", "[1,1,-1]");
	std::ofstream(oceanPath) << replace(replace(originalOcean, "waveSteepness", "0"), "fogDensity", "0");
	std::ofstream(skyPath) << originalSky;
	Require(load().environment.ocean.fogDensity == 0, "Valid flat water or disabled fog was rejected");
	for (float invalid :
		{0.0f, -1.0f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
		bool rejected = false;
		try {
			OceanGeometry::CalculateWaveHeight(1, 1, 1, invalid, 1);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "CPU wave calculation accepted invalid wavelength");
	}
	Require(std::isfinite(OceanGeometry::CalculateWaveHeight(1, 1, 1, 100, 0)),
		"Flat ocean wave height was not finite");
	std::cout << "PASS: environment ranges, JSON field/path diagnostics and CPU wavelength guard"
			  << std::endl;
}
