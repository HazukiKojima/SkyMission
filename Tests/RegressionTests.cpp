#include "Game/Aircraft/AircraftController.h"
#include "Game/Aircraft/AircraftFlightModel.h"
#include "Game/Scene/FlightScene.h"
#include "Game/Scene/FlightSceneLoader.h"
#include "Engine/Resources/Mesh/MaterialTexturePaths.h"
#include "Engine/Resources/JsonAssetReader.h"
#include "Engine/Resources/Texture/Texture.h"
#include "Engine/Renderer/Pipeline/GraphicsPipeline.h"
#include "Engine/Renderer/Device/RendererPasses.h"
#include "Engine/ExecutablePath.h"
#include "Engine/Renderer/Device/Renderer.h"
#include "Engine/Window/Window.h"
#include <d3d12sdklayers.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>

using namespace Engine;
using namespace Game;
namespace fs = std::filesystem;

void Require(bool condition, const char* description) {
	if (!condition)
		throw std::runtime_error(description);
}

#include "WeaponTests.h"
#include "CombatTests.h"
#include "CombatEffectTests.h"

void TestFlightAndScene() {
	AircraftFlightParameters parameters;
	ValidateFlightParameters(parameters);
	for (const auto member :
		{&AircraftFlightParameters::liftReferenceSpeed, &AircraftFlightParameters::minimumSpeed}) {
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

	for (float bad :
		{std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -1.0f}) {
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
	const auto input = AircraftController::MapInput(
		{.pitchUp = true, .turnLeft = true, .accelerate = true, .decelerate = true});
	Require(input.pitch == -1.0f && input.turn == 1.0f && input.throttle == 0.0f && input.highGTurn,
		"Key mapping/high-G combination changed");
	Require(AircraftController{}.ReadInput({}).pitch == 0.0f, "Inactive input was not zero");

	FlightScene scene;
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
	Require(scene.GetObjects().size() == 2 && scene.GetObjects()[0].id == player &&
			scene.GetObjects()[1].id == enemy,
		"Object IDs changed after deletion");
	scene.Update({}, 1.0f / 60.0f);
	Require(scene.GetObjects()[1].nodeRotationRadians > scene.GetObjects()[0].nodeRotationRadians * 2.0f,
		"Enemy propeller is still driven by player speed");
	scene.RemoveObject(player);
	const auto added = scene.AddObject(mesh);
	Require(added > enemy && scene.GetObjects()[0].id == enemy, "Stale object handle was reused");
	std::cout << "PASS: flight validation, integration, input, stable IDs, independent propellers\n";
}

void TestRawInput() {
	RawInputState state;
	const auto keyboard1 = reinterpret_cast<HANDLE>(std::uintptr_t(1));
	const auto keyboard2 = reinterpret_cast<HANDLE>(std::uintptr_t(2));
	const auto mouse1 = reinterpret_cast<HANDLE>(std::uintptr_t(3));
	const auto mouse2 = reinterpret_cast<HANDLE>(std::uintptr_t(4));
	const auto key = [&](HANDLE device, USHORT vkey, USHORT flags = RI_KEY_MAKE, USHORT code = 0x11) {
		RAWKEYBOARD packet{};
		packet.VKey = vkey;
		packet.Flags = flags;
		packet.MakeCode = code;
		state.ProcessKeyboard(device, packet);
	};
	const auto mouse = [&](HANDLE device, USHORT buttons, LONG dx, LONG dy,
						   USHORT flags = MOUSE_MOVE_RELATIVE) {
		RAWMOUSE packet{};
		packet.usFlags = flags;
		packet.usButtonFlags = buttons;
		packet.lLastX = dx;
		packet.lLastY = dy;
		state.ProcessMouse(device, packet, 1920, 1080);
	};
	key(keyboard1, 'W');
	Require(!state.ConsumeFrame().IsKeyDown('W'), "Inactive raw keyboard packet was accepted");
	state.SetActive(true);
	key(keyboard1, 'W');
	key(keyboard1, 'W'); // 押下通知が重複しても状態は一つだけ保持する
	Require(AircraftController{}.ReadInput(state.ConsumeFrame()).pitch == -1.0f, "Raw W make was not mapped");
	Require(state.ConsumeFrame().IsKeyDown('W'), "Held key disappeared at next frame");
	key(keyboard2, 'W');
	key(keyboard1, 'W', RI_KEY_BREAK);
	Require(state.ConsumeFrame().IsKeyDown('W'), "One keyboard's break cleared another keyboard's held key");
	state.ForgetDevice(keyboard2);
	Require(!state.ConsumeFrame().IsKeyDown('W'), "Unplugged keyboard left a stuck key");
	key(keyboard1, VK_CONTROL);
	key(keyboard1, VK_CONTROL, RI_KEY_E0);
	key(keyboard1, VK_CONTROL, RI_KEY_BREAK);
	key(keyboard1, VK_SPACE);
	auto snapshot = state.ConsumeFrame();
	Require(
		!snapshot.IsKeyDown(VK_LCONTROL) && snapshot.IsKeyDown(VK_RCONTROL) && snapshot.IsKeyDown(VK_CONTROL),
		"Left/right Ctrl state is not independent");
	Require(
		AircraftController{}.ReadInput(snapshot).highGTurn, "Raw Ctrl+Space did not activate high-G input");
	key(keyboard1, VK_CONTROL, RI_KEY_E0 | RI_KEY_BREAK);
	key(keyboard1, VK_SPACE, RI_KEY_BREAK);
	key(keyboard1, VK_CONTROL, RI_KEY_E1, 0x1d);
	Require(!state.ConsumeFrame().IsKeyDown(VK_CONTROL), "Pause's synthetic Ctrl became stuck");
	key(keyboard1, 0xff);
	key(keyboard1, 'W', RI_KEY_MAKE, 0xff);
	Require(!state.ConsumeFrame().IsKeyDown('W'), "Keyboard overrun packet was retained");

	mouse(mouse1, RI_MOUSE_RIGHT_BUTTON_DOWN, 3, -2);
	mouse(mouse1, 0, 4, 5);
	mouse(mouse1, RI_MOUSE_RIGHT_BUTTON_UP, 2, 0);
	snapshot = state.ConsumeFrame();
	Require(!snapshot.rightMouseDown && snapshot.mouseDeltaX == 9 && snapshot.mouseDeltaY == 3,
		"Short right-button drag or accumulated motion was lost");
	const auto next = state.ConsumeFrame();
	Require(next.mouseDeltaX == 0 && next.mouseDeltaY == 0, "Raw mouse movement was replayed next frame");
	AircraftCameraController camera;
	camera.Initialize();
	DirectX::XMFLOAT4X4 before, after;
	DirectX::XMStoreFloat4x4(&before, camera.GetView());
	camera.Update(0.0f, snapshot);
	DirectX::XMStoreFloat4x4(&after, camera.GetView());
	Require(
		std::abs(after._13 - before._13) > 0.001f, "Camera did not consume released-in-frame drag motion");
	mouse(mouse1, 0, 100, 100);
	Require(state.ConsumeFrame().mouseDeltaX == 0, "Mouse movement without right drag rotated the camera");

	mouse(mouse1, RI_MOUSE_RIGHT_BUTTON_DOWN, 0, 0, MOUSE_MOVE_ABSOLUTE);
	mouse(mouse1, 0, 65535, 65535, MOUSE_MOVE_ABSOLUTE);
	snapshot = state.ConsumeFrame();
	Require(snapshot.mouseDeltaX == 1919 && snapshot.mouseDeltaY == 1079,
		"Absolute mouse coordinates were not converted");
	mouse(mouse2, 0, 32767, 32767, MOUSE_MOVE_ABSOLUTE);
	Require(state.ConsumeFrame().mouseDeltaX == 0, "New absolute mouse device caused a camera jump");
	key(keyboard1, 'W');
	state.SetActive(false);
	snapshot = state.ConsumeFrame();
	Require(
		!snapshot.active && !snapshot.IsKeyDown('W') && !snapshot.rightMouseDown && snapshot.mouseDeltaX == 0,
		"Focus loss did not clear keys, mouse buttons and motion");
	mouse(mouse1, RI_MOUSE_RIGHT_BUTTON_DOWN, 100, 100);
	key(keyboard1, 'W');
	state.SetActive(true);
	snapshot = state.ConsumeFrame();
	Require(
		!snapshot.IsKeyDown('W') && !snapshot.rightMouseDown, "Background input returned on focus regain");
	const auto position = camera.GetPosition();
	key(keyboard1, 'W');
	camera.Update(0.1f, state.ConsumeFrame());
	Require(std::abs(camera.GetPosition().z - position.z) > 1.0f,
		"Free camera did not consume raw keyboard state");

	const auto registrations = [] {
		UINT count = 0;
		Require(GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE)) != UINT(-1),
			"Raw registration query failed");
		std::vector<RAWINPUTDEVICE> devices(count);
		if (count)
			Require(GetRegisteredRawInputDevices(devices.data(), &count, sizeof(RAWINPUTDEVICE)) != UINT(-1),
				"Raw registration retrieval failed");
		return devices;
	};
	auto first = std::make_unique<Window>(64, 64, L"Raw Input test (hidden)", GetModuleHandleW(nullptr));
	{
		Window second(64, 64, L"Raw Input test 2 (hidden)", GetModuleHandleW(nullptr));
		first.reset(); // 一つ目の破棄後も二つ目の入力登録を維持する
		UINT matching = 0;
		for (const auto& device : registrations())
			if (device.hwndTarget == second.GetHandle()) {
				Require((device.dwFlags & RIDEV_INPUTSINK) == 0, "Raw input is receiving background events");
				++matching;
			}
		Require(matching == 2, "Window did not register foreground keyboard and mouse input");
		Require(!second.ConsumeInput().active, "Hidden unfocused window exposes active input");
		// タイトルバーの移動・サイズ変更が使うキャプチャを入力処理で解除しない
		SetCapture(second.GetHandle());
		Require(GetCapture() == second.GetHandle(), "Native mouse capture could not be established");
		second.ConsumeInput();
		Require(GetCapture() == second.GetHandle(), "Input polling stole native window-operation capture");
		SendMessageW(second.GetHandle(), WM_INPUT_DEVICE_CHANGE, GIDC_REMOVAL, 0);
		Require(GetCapture() == second.GetHandle(), "Device removal stole native window-operation capture");
		ReleaseCapture();
		SendMessageW(second.GetHandle(), WM_LBUTTONDOWN, MK_LBUTTON, 0);
		Require(GetCapture() != second.GetHandle(), "Left click started camera capture");
		SendMessageW(second.GetHandle(), WM_LBUTTONUP, 0, 0);
		SendMessageW(second.GetHandle(), WM_RBUTTONDOWN, MK_RBUTTON, 0);
		Require(GetCapture() == second.GetHandle(), "Client right drag did not capture the mouse");
		SendMessageW(second.GetHandle(), WM_RBUTTONUP, 0, 0);
		Require(GetCapture() != second.GetHandle(), "Right drag release kept camera capture");
		SendMessageW(second.GetHandle(), WM_RBUTTONDOWN, MK_RBUTTON, 0);
		SendMessageW(second.GetHandle(), WM_CANCELMODE, 0, 0);
		Require(GetCapture() != second.GetHandle(), "Cancel mode kept camera capture");
		SendMessageW(second.GetHandle(), WM_RBUTTONDOWN, MK_RBUTTON, 0);
		SendMessageW(second.GetHandle(), WM_KILLFOCUS, 0, 0);
		Require(GetCapture() != second.GetHandle(), "Focus loss kept camera capture");
		UINT resizeCount = 0;
		second.SetOnResize([&](UINT, UINT) { ++resizeCount; });
		Require(SetWindowPos(second.GetHandle(), nullptr, 0, 0, 320, 240,
			SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE), "Window resize failed");
		Require(resizeCount > 0, "Window resize did not reach its callback");
		SendMessageW(second.GetHandle(), WM_SYSCOMMAND, SC_MAXIMIZE, 0);
		Require(IsZoomed(second.GetHandle()), "Standard maximize command failed");
		SendMessageW(second.GetHandle(), WM_SYSCOMMAND, SC_RESTORE, 0);
		Require(!IsZoomed(second.GetHandle()), "Standard restore command failed");
		const auto handle = second.GetHandle();
		SendMessageW(handle, WM_SYSCOMMAND, SC_CLOSE, 0);
		Require(!IsWindow(handle), "Standard close command failed");
	}

	for (const auto& device : registrations())
		Require(device.hwndTarget == nullptr, "Destroyed window retained raw input registration");
	MSG message{};
	while (PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE)) {
	}
	std::cout << "PASS: Raw Input make/break, modifiers, multiple devices, drag, focus loss, registration "
				 "lifetime, camera/native capture, resize, maximize, restore and close\n";
}

void TestPathsAndJson(const fs::path& repository, const fs::path& fixture) {
	const auto textures = fixture / L"Textures";
	fs::create_directories(textures);
	const auto japanese = textures / L"機体_金属.png";
	std::ofstream(japanese) << "fixture";
	const std::u8string material = u8"機体";
	const auto found = MaterialTexturePaths::FindNamedMaterialTexturePath(fixture,
		std::string(reinterpret_cast<const char*>(material.data()), material.size()), {}, L"_金属.png", {});
	Require(found == japanese, "Japanese material name did not match UTF-8 filename");
	Require(
		MaterialTexturePaths::FindNamedMaterialTexturePath(fixture, "missing", {}, L"_金属.png", {}).empty(),
		"Missing material unexpectedly matched");
	const auto config = repository / L"Assets";
	const auto definition = FlightSceneLoader{}.Load({.environment = config / L"Environment/environment.json",
		.playerAircraft = config / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = config / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = config / L"Environment/retroFilm.json"});
	Require(definition.playerAircraft.modelImport.rotatingNodeName == "Propeller" &&
			definition.environment.ocean.fogTop == 350.0f,
		"Model/fog JSON settings were not loaded");
	const auto json = fixture / L"reader.json";
	std::ofstream(json) << R"({"texture":"機体.png", "value":2.5})";
	JsonAssetReader reader;
	Require(reader.ResolveFloat(json, "value") == 2.5f, "Transient JSON acquisition failed");
	Require(reader.Resolve(json, "texture").filename() == L"機体.png", "UTF-8 JSON asset path was damaged");
	std::cout << "PASS: Japanese material paths, JSON acquisition, model/fog configuration\n";
}

void RequireDefaultBufferContent(ID3D12Device* device, ID3D12GraphicsCommandList* commands,
	ID3D12Resource* source, D3D12_RESOURCE_STATES state, const void* expected, size_t bytes,
	const auto& submit) {
	D3D12_HEAP_PROPERTIES properties{};
	ThrowIfFailed(source->GetHeapProperties(&properties, nullptr));
	Require(properties.Type == D3D12_HEAP_TYPE_DEFAULT, "Procedural geometry remained in UPLOAD memory");
	ComPtr<ID3D12Resource> readback;
	const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
	const auto desc = CD3DX12_RESOURCE_DESC::Buffer(bytes);
	ThrowIfFailed(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(source, state, D3D12_RESOURCE_STATE_COPY_SOURCE);
	commands->ResourceBarrier(1, &barrier);
	commands->CopyBufferRegion(readback.Get(), 0, source, 0, bytes);
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE, state);
	commands->ResourceBarrier(1, &barrier);
	submit();
	void* data = nullptr;
	const CD3DX12_RANGE range(0, bytes);
	ThrowIfFailed(readback->Map(0, &range, &data));
	const bool equal = std::memcmp(data, expected, bytes) == 0;
	const CD3DX12_RANGE noWrites(0, 0);
	readback->Unmap(0, &noWrites);
	Require(equal, "Procedural geometry copy did not preserve vertex/index data");
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
	ThrowIfFailed(device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&commands)));
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
	ThrowIfFailed(
		DirectX::SaveToWICFile(*pixels, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, png.c_str()));
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
			Require(
				desc.MipLevels == 5 && desc.Format == format, "Mip generation/compression order is broken");
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
	auto singleMip =
		Texture::LoadCached(device.Get(), commands.Get(), png.wstring(), {.generateMipmaps = false});
	Require(singleMip != shared && singleMip->GetResource()->GetDesc().MipLevels == 1,
		"Cache ignored mip options");
	submit();

	VertexBuffer proceduralVertices;
	const float vertexData[]{1, 2, 3, 4, 5, 6};
	proceduralVertices.Initialize(
		device.Get(), commands.Get(), vertexData, sizeof(vertexData), 3 * sizeof(float));
	SkySphere sky;
	sky.Initialize(device.Get(), commands.Get(), 10.0f, 8, 4);
	OceanGeometry proceduralOcean;
	proceduralOcean.Initialize(device.Get(), commands.Get(), 2000.0f);
	submit();
	proceduralVertices.ReleaseUploadResources();
	sky.ReleaseUploadResources();
	proceduralOcean.ReleaseUploadResources();
	RequireDefaultBufferContent(device.Get(), commands.Get(), proceduralVertices.GetResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, vertexData, sizeof(vertexData), submit);
	const SkySphere::Vertex skyVertex{{0, 10, 0}, {0, 0}};
	const std::uint32_t skyIndices[]{0, 9, 1, 1, 9, 10};
	Require(sky.GetIndexCount() == 8 * 4 * 6, "Sky sphere index count changed");
	RequireDefaultBufferContent(device.Get(), commands.Get(), sky.GetVertexBufferResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &skyVertex, sizeof(skyVertex), submit);
	RequireDefaultBufferContent(device.Get(), commands.Get(), sky.GetIndexBufferResource(),
		D3D12_RESOURCE_STATE_INDEX_BUFFER, skyIndices, sizeof(skyIndices), submit);
	const float oceanVertex[]{-1000, 0, -1000, 0, 0};
	const std::uint32_t oceanIndices[]{0, 1, 1000, 1, 1001, 1000};
	Require(proceduralOcean.GetIndexCount() == 999 * 999 * 6, "Ocean index count changed");
	RequireDefaultBufferContent(device.Get(), commands.Get(), proceduralOcean.GetVertexBufferResource(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, oceanVertex, sizeof(oceanVertex), submit);
	RequireDefaultBufferContent(device.Get(), commands.Get(), proceduralOcean.GetIndexBufferResource(),
		D3D12_RESOURCE_STATE_INDEX_BUFFER, oceanIndices, sizeof(oceanIndices), submit);
	std::cout << "PASS: DEFAULT heap vertex/ocean/sky buffers, GPU readback after upload release\n";

	// Ocean欠落時はパイプライン・デバイスの参照や描画を行わない
	RendererAssets assets;
	RendererPipelines pipelines;
	RendererConstantBuffers constants;
	OceanGeometry ocean;
	OceanRenderPass{}.Record({nullptr, nullptr, pipelines, constants, 0}, assets, ocean);
	RenderDevice{}.Resize(0, 100);

	// 作業ディレクトリに依存せず、実行ファイルの隣からシェーダーを読み込む
	const auto executable = ExecutableDirectory();
	const char* vs = "float4 main(float3 p:POSITION,float2 uv:TEXCOORD):SV_POSITION{return float4(p,1);}";
	const char* ps = "float4 main():SV_TARGET{return float4(1,1,1,1);}";
	for (const auto& stage : {std::pair{vs, L"BasicVS.cso"}, std::pair{ps, L"BasicPS.cso"}}) {
		ComPtr<ID3DBlob> blob;
		ThrowIfFailed(D3DCompile(stage.first, std::strlen(stage.first), nullptr, nullptr, nullptr, "main",
			stage.second == std::wstring(L"BasicVS.cso") ? "vs_5_0" : "ps_5_0", 0, 0, &blob, nullptr));
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
	std::cout << "PASS: WARP BC3/BC4/BC5 mip chains, cache reload, extensionless/Unicode paths, ocean guard, "
				 "shader CWD\n";
}

void TestRendererSmoke(const fs::path& repository, const fs::path& fixture) {
	const auto config = repository / L"Assets";
	auto definition = FlightSceneLoader{}.Load({.environment = config / L"Environment/environment.json",
		.playerAircraft = config / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = config / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = config / L"Environment/retroFilm.json"});
	const auto model = fixture / L"triangles.obj";
	std::ofstream(model) << "o Body\nv -1 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n"
							"o Propeller\nv -1 0 -1\nv 1 0 -1\nv 0 1 -1\nf 4 5 6\n";
	definition.playerAircraft.model = definition.enemyAircraft.model = model;
	definition.playerAircraft.model = fixture / L"muzzles.dae";
	definition.environment.skyTexture = definition.environment.oceanTexture =
		definition.environment.oceanNormalTexture = definition.environment.oceanNormalTexture2 =
			fixture / L"機体.png";
	const HWND window = CreateWindowW(L"STATIC", L"SkyMission regression (hidden)", WS_OVERLAPPEDWINDOW, 0, 0,
		256, 256, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Hidden test window could not be created");
	// 必須モデルの失敗は、Rendererの破棄を経ても呼び出し元に届くこと。
	const auto invalidModel = fixture / L"invalid-model.obj";
	std::ofstream(invalidModel) << "This file contains no mesh.\n";
	for (const size_t modelIndex : {size_t(0), size_t(1), size_t(2)}) {
		auto invalidDefinition = definition.BuildRendererDefinition();
		auto& asset = invalidDefinition.models[modelIndex];
		asset.file = modelIndex == 1 ? invalidModel : fixture / L"存在しない機体.obj";
		const auto pathUtf8 = asset.file.u8string();
		const std::string expectedPath(reinterpret_cast<const char*>(pathUtf8.data()), pathUtf8.size());
		bool rejected = false;
		try {
			Renderer renderer;
			renderer.Initialize(window, 64, 64, invalidDefinition);
		} catch (const std::runtime_error& error) {
			const std::string message = error.what();
			rejected = message.find("Failed to load required model '" + asset.id + "': ") != std::string::npos &&
				message.find(expectedPath) != std::string::npos;
		}
		Require(rejected, "Required model failure did not report its asset ID and UTF-8 path");
	}
	std::cout << "PASS: missing player/ammunition and invalid enemy models reject initialization with ID/path\n";
	{
		Renderer renderer;
		renderer.Initialize(window, 64, 64, definition.BuildRendererDefinition());
		FlightScene scene;
		scene.Initialize(1.0f, 1.0f, definition.environment.ocean.size);
		scene.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
		scene.AddPlayerObject(renderer.GetModel(PlayerModelId));
		scene.AddEnemyAircraft(renderer.GetModel(EnemyModelId), {30, 100, 0}, 1.0f,
			definition.enemyAircraft.flightParameters, definition.enemyAIParameters);
		scene.ConfigurePlayerWeapon(definition.playerWeapon, renderer.GetModel(AmmunitionModelId));
		renderer.InitializeConstantBuffers(scene.GetRenderScene());
		renderer.ReserveModelCapacity(scene.GetObjects().size() + definition.playerWeapon.gun.maxProjectiles);
		const auto reserved = renderer.GetModelCapacity();
		Engine::HudDrawList hud;
		Game::CombatHudPresenter presenter;
		for (int i = 0; i < 6; ++i) {
			scene.Update({}, 0.05f, {}, {true});
			const auto view=renderer.UpdateFrameConstants(scene.GetRenderScene(), i / 60.0f);
			presenter.Build(scene.GetCombatSnapshot(),view,hud);
			renderer.Render(64, 64, scene.GetRenderScene(),&hud);
		}
		Require(scene.GetProjectileCount() >= 12, "Scene did not integrate continuous weapon fire");
		Require(renderer.GetModelCapacity() == reserved, "Firing grew GPU constant buffers despite reservation");

		renderer.Resize(0, 0);
		renderer.Render(0, 0, scene.GetRenderScene());
		renderer.Resize(96, 96);
		const auto resizedView=renderer.UpdateFrameConstants(scene.GetRenderScene(), 1.0f);
		Require(resizedView.width==96 && resizedView.height==96,"HUD view did not reflect renderer resize");
		presenter.Build(scene.GetCombatSnapshot(),resizedView,hud);
		renderer.Render(96, 96, scene.GetRenderScene(),&hud);
		scene.RemoveObject(scene.GetObjects()[0].id);
		for (int i = 0; i < 14; ++i) scene.Update({}, 0.25f, {}, {true});
		Require(scene.GetProjectileCount() == 0, "Removed player kept firing or existing projectiles did not expire");
		renderer.UpdateFrameConstants(scene.GetRenderScene(), 4.0f);
		renderer.Render(96, 96, scene.GetRenderScene());
	}

	DestroyWindow(window);
	std::cout << "PASS: hidden renderer, combined static model buffers, all passes, zero-size and restored "
				 "resize\n";
}

int wmain(int argc, wchar_t** argv) {
	try {
		Require(argc == 3, "Expected repository and fixture directories");
		ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
		const fs::path fixture = argv[2];
		fs::create_directories(fixture);
		TestFlightAndScene();
		TestCombatMath();
		TestCombatEffectLifetime();
		TestRawInput();
		TestPathsAndJson(argv[1], fixture);
		TestGpuTextures(fixture);
		TestWeapons(argv[1], fixture);
		TestCombatDamage(argv[1],fixture);
		TestHudRendering(fixture);
		TestRendererSmoke(argv[1], fixture);
		CoUninitialize();
		return 0;
	} catch (const std::exception& error) {
		std::cerr << "FAIL: " << error.what() << '\n';
		return 1;
	}
}
