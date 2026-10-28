#include "Engine/Aircraft/AircraftController.h"
#include "Engine/Aircraft/AircraftFlightModel.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Resources/Mesh/MaterialTexturePaths.h"
#include "Engine/Resources/JsonAssetReader.h"
#include "Engine/Resources/Texture/Texture.h"
#include "Engine/Renderer/Pipeline/GraphicsPipeline.h"
#include "Engine/Renderer/Device/RendererPasses.h"
#include "Engine/ExecutablePath.h"
#include "Engine/Renderer/Device/Renderer.h"
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>

using namespace Engine;
namespace fs = std::filesystem;

void Require(bool condition, const char* description) {
	if (!condition)
		throw std::runtime_error(description);
}

void TestFlightAndScene() {
	AircraftFlightParameters parameters;
	ValidateFlightParameters(parameters);
	for (const auto member : {&AircraftFlightParameters::liftReferenceSpeed, &AircraftFlightParameters::minimumSpeed}) {
		auto invalid = parameters;
		invalid.*member = 0.0f;
		bool rejected = false;
		try {
			AircraftFlightModel model;
			model.SetFlightParameters(invalid);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Zero flight divisor was accepted");
	}
	for (float bad : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.0f}) {
		auto invalid = parameters;
		invalid.drag = bad;
		bool rejected = false;
		try {
			ValidateFlightParameters(invalid);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Nonfinite/negative flight parameter was accepted");
	}
	auto invalid = parameters;
	invalid.criticalAngleRadians = invalid.stallAngleRadians;
	bool rejected = false;
	try {
		ValidateFlightParameters(invalid);
	} catch (const std::invalid_argument&) {
		rejected = true;
	}
	Require(rejected, "Equal stall/critical angles were accepted");
	AircraftFlightModel flight;
	flight.SetFlightParameters(parameters);
	for (int i = 0; i < 180; ++i)
		flight.Update({.pitch = 0.3f, .turn = 0.5f}, 1.0f / 60.0f);
	Require(std::isfinite(flight.GetSpeed()), "Flight integration produced NaN");
	const auto input =
		AircraftController::MapInput({.pitchUp = true, .turnLeft = true, .accelerate = true, .decelerate = true});
	Require(input.pitch == -1.0f && input.turn == 1.0f && input.throttle == 0.0f && input.highGTurn,
			"Key mapping/high-G combination changed");
	Require(AircraftController{}.ReadInput(nullptr).pitch == 0.0f, "Inactive input was not zero");

	Scene scene;
	auto mesh = std::make_shared<Model>();
	const auto removed = scene.AddObject(mesh);
	parameters.minimumSpeed = 1.0f;
	parameters.maximumSpeed = 400.0f;
	parameters.initialSpeed = 60.0f;
	scene.SetPlayerAircraftFlightParameters(parameters);
	const auto player = scene.AddPlayerObject(mesh);
	parameters.initialSpeed = 180.0f;
	const auto enemy = scene.AddEnemyAircraft(mesh, {0, 200, 0}, 1.0f, parameters, {});
	scene.RemoveObject(removed);
	Require(scene.GetObjects().size() == 2 && scene.GetObjects()[0].id == player && scene.GetObjects()[1].id == enemy,
			"Object IDs changed after deletion");
	scene.Update({}, 1.0f / 60.0f);
	Require(scene.GetObjects()[1].propellerAngle > scene.GetObjects()[0].propellerAngle * 2.0f,
			"Enemy propeller is still driven by player speed");
	scene.RemoveObject(player);
	const auto added = scene.AddObject(mesh);
	Require(added > enemy && scene.GetObjects()[0].id == enemy, "Stale object handle was reused");
	std::cout << "PASS: flight validation, integration, input, stable IDs, independent propellers\n";
}

void TestPathsAndJson(const fs::path& repository, const fs::path& fixture) {
	const auto textures = fixture / L"Textures";
	fs::create_directories(textures);
	const auto japanese = textures / L"機体_金属.png";
	std::ofstream(japanese) << "fixture";
	const std::u8string material = u8"機体";
	const auto found = MaterialTexturePaths::FindNamedMaterialTexturePath(
		fixture, std::string(reinterpret_cast<const char*>(material.data()), material.size()), {}, L"_金属.png", {});
	Require(found == japanese, "Japanese material name did not match UTF-8 filename");
	Require(MaterialTexturePaths::FindNamedMaterialTexturePath(fixture, "missing", {}, L"_金属.png", {}).empty(),
			"Missing material unexpectedly matched");
	const auto config = repository / L"Assets";
	const auto definition =
		SceneLoader{}.Load({.environment = config / L"Environment/environment.json",
							.playerAircraft = config / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
							.enemyAircraft = config / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
							.retroFilm = config / L"Environment/retroFilm.json"});
	Require(definition.playerAircraft.modelImport.propellerName == "Propeller" && definition.ocean.fogTop == 350.0f,
			"Model/fog JSON settings were not loaded");
	const auto json = fixture / L"reader.json";
	std::ofstream(json) << R"({"texture":"機体.png", "value":2.5})";
	JsonAssetReader reader;
	Require(reader.ResolveFloat(json, "value") == 2.5f, "Transient JSON acquisition failed");
	Require(reader.Resolve(json, "texture").filename() == L"機体.png", "UTF-8 JSON asset path was damaged");
	std::cout << "PASS: Japanese material paths, JSON acquisition, model/fog configuration\n";
}

void TestGpuTextures(const fs::path& fixture) {
	ComPtr<ID3D12Debug> debug;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
		debug->EnableDebugLayer();
	ComPtr<IDXGIFactory4> factory;
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));
	ComPtr<IDXGIAdapter> warp;
	ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
	ComPtr<ID3D12Device> device;
	ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));
	ComPtr<ID3D12CommandAllocator> allocator;
	ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
	ComPtr<ID3D12GraphicsCommandList> commands;
	ThrowIfFailed(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
											IID_PPV_ARGS(&commands)));
	ComPtr<ID3D12CommandQueue> queue;
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue)));
	const auto submit = [&] {
		ThrowIfFailed(commands->Close());
		ID3D12CommandList* lists[]{commands.Get()};
		queue->ExecuteCommandLists(1, lists);
		ComPtr<ID3D12Fence> fence;
		ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
		ThrowIfFailed(queue->Signal(fence.Get(), 1));
		const HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		Require(event != nullptr, "Fence event creation failed");
		ThrowIfFailed(fence->SetEventOnCompletion(1, event));
		const DWORD wait = WaitForSingleObject(event, 30000);
		CloseHandle(event);
		Require(wait == WAIT_OBJECT_0, "GPU submission did not finish");
		ThrowIfFailed(allocator->Reset());
		ThrowIfFailed(commands->Reset(allocator.Get(), nullptr));
	};

	DirectX::ScratchImage image;
	ThrowIfFailed(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 16, 16, 1, 1));
	const auto* pixels = image.GetImage(0, 0, 0);
	std::memset(pixels->pixels, 128, pixels->slicePitch);
	const auto png = fixture / L"機体.png";
	ThrowIfFailed(DirectX::SaveToWICFile(*pixels, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, png.c_str()));
	const auto extensionless = fixture / L"拡張子なし";
	fs::copy_file(png, extensionless, fs::copy_options::overwrite_existing);
	for (const auto format : {DXGI_FORMAT_BC3_UNORM_SRGB, DXGI_FORMAT_BC4_UNORM, DXGI_FORMAT_BC5_UNORM}) {
		const TextureLoadOptions options{
			.useSrgb = format == DXGI_FORMAT_BC3_UNORM_SRGB, .generateMipmaps = true, .targetFormat = format};
		{
			Texture texture;
			Require(texture.LoadFromFile(device.Get(), commands.Get(), png.wstring(), options),
					"BC texture load failed");
			const auto desc = texture.GetResource()->GetDesc();
			Require(desc.MipLevels == 5 && desc.Format == format, "Mip generation/compression order is broken");
			submit();
		}
		{
			Texture cached;
			Require(cached.LoadFromFile(device.Get(), commands.Get(), png.wstring(), options),
					"Disk cache reload failed");
			Require(cached.GetResource()->GetDesc().MipLevels == 5, "Disk cache lost mip levels");
			submit();
		}
	}
	Texture noExtension;
	Require(noExtension.LoadFromFile(device.Get(), commands.Get(), extensionless.wstring()),
			"Extensionless WIC image failed");
	Require(noExtension.GetResource()->GetDesc().MipLevels == 5, "Extensionless image lost mip levels");
	Texture missing;
	Require(!missing.LoadFromFile(device.Get(), commands.Get(), (fixture / L"missing").wstring()),
			"Missing extensionless image was accepted");
	const auto dotted = fixture / L"folder.with.dot";
	fs::create_directories(dotted);
	Require(!missing.LoadFromFile(device.Get(), commands.Get(), (dotted / L"missing").wstring()),
			"Dot in parent directory was treated as an extension");

	auto shared = Texture::LoadCached(device.Get(), commands.Get(), png.wstring());
	Require(shared == Texture::LoadCached(device.Get(), commands.Get(), png.wstring()),
			"Memory cache missed identical request");
	auto singleMip = Texture::LoadCached(device.Get(), commands.Get(), png.wstring(), {.generateMipmaps = false});
	Require(singleMip != shared && singleMip->GetResource()->GetDesc().MipLevels == 1, "Cache ignored mip options");
	submit();

	// A missing ocean must return before accessing pipeline/device state or issuing draw calls.
	RendererAssets assets;
	RendererPipelines pipelines;
	RendererConstantBuffers constants;
	OceanGeometry ocean;
	OceanRenderPass{}.Record({nullptr, nullptr, pipelines, constants, 0}, assets, ocean);
	RenderDevice{}.Resize(0, 100);

	// Real PSO creation after changing CWD proves shaders resolve beside the executable.
	const auto executable = ExecutableDirectory();
	const char* vs = "float4 main(float3 p:POSITION,float2 uv:TEXCOORD):SV_POSITION{return float4(p,1);}";
	const char* ps = "float4 main():SV_TARGET{return float4(1,1,1,1);}";
	for (const auto& stage : {std::pair{vs, L"BasicVS.cso"}, std::pair{ps, L"BasicPS.cso"}}) {
		ComPtr<ID3DBlob> blob;
		ThrowIfFailed(D3DCompile(stage.first, std::strlen(stage.first), nullptr, nullptr, nullptr, "main",
								 stage.second == std::wstring(L"BasicVS.cso") ? "vs_5_0" : "ps_5_0", 0, 0, &blob,
								 nullptr));
		ThrowIfFailed(D3DWriteBlobToFile(blob.Get(), (executable / stage.second).c_str(), TRUE));
	}
	const auto previous = fs::current_path();
	fs::current_path(fixture);
	GraphicsPipeline pipeline;
	pipeline.Initialize(device.Get(), {.addressMode = D3D12_TEXTURE_ADDRESS_MODE_CLAMP});
	fs::current_path(previous);

	ComPtr<ID3D12InfoQueue> messages;
	if (SUCCEEDED(device.As(&messages))) {
		for (UINT64 i = 0; i < messages->GetNumStoredMessages(); ++i) {
			SIZE_T size = 0;
			messages->GetMessage(i, nullptr, &size);
			std::vector<char> bytes(size);
			auto* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
			ThrowIfFailed(messages->GetMessage(i, message, &size));
			if (message->Severity == D3D12_MESSAGE_SEVERITY_ERROR ||
				message->Severity == D3D12_MESSAGE_SEVERITY_CORRUPTION)
				throw std::runtime_error(message->pDescription);
		}
	}
	ThrowIfFailed(commands->Close());
	std::cout
		<< "PASS: WARP BC3/BC4/BC5 mip chains, cache reload, extensionless/Unicode paths, ocean guard, shader CWD\n";
}

void TestRendererSmoke(const fs::path& repository, const fs::path& fixture) {
	const auto config = repository / L"Assets";
	auto definition = SceneLoader{}.Load({.environment = config / L"Environment/environment.json",
										  .playerAircraft = config / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
										  .enemyAircraft = config / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
										  .retroFilm = config / L"Environment/retroFilm.json"});
	const auto model = fixture / L"triangles.obj";
	std::ofstream(model) << "o Body\nv -1 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"
							"o Propeller\nv -1 0 -1\nv 1 0 -1\nv 0 1 -1\nf 4 5 6\n";
	definition.playerAircraft.model = definition.enemyAircraft.model = model;
	definition.skyTexture = definition.oceanTexture = definition.oceanNormalTexture = definition.oceanNormalTexture2 =
		fixture / L"機体.png";
	const HWND window = CreateWindowW(L"STATIC", L"SkyMission regression (hidden)", WS_OVERLAPPEDWINDOW, 0, 0, 256, 256,
									  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Hidden test window could not be created");
	{
		Renderer renderer;
		renderer.Initialize(window, 64, 64, definition);
		Scene scene;
		scene.Initialize(window, 1.0f, 1.0f, definition.ocean.size);
		scene.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
		scene.AddPlayerObject(renderer.GetModel());
		scene.AddEnemyAircraft(renderer.GetEnemyAircraftModel(), {30, 100, 0}, 1.0f,
							   definition.enemyAircraft.flightParameters, definition.enemyAIParameters);
		renderer.InitializeConstantBuffers(scene);
		for (int i = 0; i < 3; ++i) {
			scene.Update({}, 1.0f / 60.0f);
			renderer.UpdateFrameConstants(scene, i / 60.0f);
			renderer.Render(64, 64, scene);
		}
		renderer.Resize(0, 0);
		renderer.Render(0, 0, scene);
		renderer.Resize(96, 96);
		renderer.UpdateFrameConstants(scene, 1.0f);
		renderer.Render(96, 96, scene);
	}
	DestroyWindow(window);
	std::cout << "PASS: hidden renderer, combined static model buffers, all passes, zero-size and restored resize\n";
}

int wmain(int argc, wchar_t** argv) {
	try {
		Require(argc == 3, "Expected repository and fixture directories");
		ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
		const fs::path fixture = argv[2];
		fs::create_directories(fixture);
		TestFlightAndScene();
		TestPathsAndJson(argv[1], fixture);
		TestGpuTextures(fixture);
		TestRendererSmoke(argv[1], fixture);
		CoUninitialize();
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
