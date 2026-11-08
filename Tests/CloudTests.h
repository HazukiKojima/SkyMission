#pragma once
#include "Game/Scene/CloudSettingsLoader.h"

void TestCloudSettings(const fs::path& repository, const fs::path& fixture) {
	JsonAssetReader reader;
	Require(!LoadCloudSettings(reader, fixture / L"missing-clouds.json").enabled,
		"Missing clouds must be optional");
	const auto settings = LoadCloudSettings(reader, repository / L"Assets/Environment/Clouds.json");
	Require(settings.enabled && settings.bottom > 1500 && settings.steps <= 64,
		"Cloud defaults or step budget changed");
	ValidateCloudSettings(settings);
	const auto reject = [&](CloudSettings invalid) {
		bool rejected = false;
		try {
			ValidateCloudSettings(invalid);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		Require(rejected, "Invalid cloud settings accepted");
	};
	auto invalid = settings;
	invalid.top = invalid.bottom;
	reject(invalid);
	invalid = settings;
	invalid.density = std::numeric_limits<float>::quiet_NaN();
	reject(invalid);
	invalid = settings;
	invalid.density = -0.01f;
	reject(invalid);
	invalid = settings;
	invalid.coverage = 2;
	reject(invalid);
	invalid = settings;
	invalid.steps = 128;
	reject(invalid);
	std::ofstream(fixture / L"disabled-clouds.json") << R"({"enabled":0})";
	Require(
		!LoadCloudSettings(reader, fixture / L"disabled-clouds.json").enabled, "Disabled cloud file ignored");
	std::cout << "PASS: optional cloud settings, layer/density/coverage/step validation\n";
}

void TestCloudRendering(const fs::path& repository, const fs::path& fixture,
	UINT width = 1000, UINT height = 600, bool profile = false) {
	const auto config = repository / L"Assets";
	const auto definition = FlightSceneLoader{}.Load({.environment = config / L"Environment/environment.json",
		.playerAircraft = config / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = config / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = config / L"Environment/retroFilm.json",
		.bomb = config / L"Ammunition/Bomb/ammunition.json"});
	const HWND window = CreateWindowW(L"STATIC", L"Cloud verification (hidden)", WS_OVERLAPPEDWINDOW, 0, 0,
		width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Cloud test window creation failed");
	{
		RenderDevice device;
		device.Initialize(window, width, height);
		CommandContext context;
		context.Initialize(&device);
		RendererPipelines pipelines;
		pipelines.Initialize(device.GetDevice());
		RendererAssets assets;
		OceanGeometry ocean;
		RendererConstantBuffers buffers;
		buffers.Initialize(device.GetDevice());
		RendererFrameUpdater updater;
		ComPtr<ID3D12QueryHeap> timestamps;
		ComPtr<ID3D12Resource> timestampReadback;
		UINT64 frequency = 0;
		if (profile) {
			std::cout << "Cloud review adapter: " << fs::path(device.GetAdapterName()).string()
				<< ", " << width << "x" << height << '\n';
			const D3D12_QUERY_HEAP_DESC query{D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 6, 0};
			ThrowIfFailed(device.GetDevice()->CreateQueryHeap(&query, IID_PPV_ARGS(&timestamps)));
			const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
			const auto buffer = CD3DX12_RESOURCE_DESC::Buffer(6 * sizeof(UINT64));
			ThrowIfFailed(device.GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE,
				&buffer, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&timestampReadback)));
			ThrowIfFailed(device.GetCommandQueue()->GetTimestampFrequency(&frequency));
		}
		const auto quad = fixture / L"cloud-occluder.obj";
		std::ofstream(quad) << "v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\nf 1 2 3\nf 1 3 4\nf 1 3 2\nf 1 4 3\n";
		context.BeginFrame();
		auto* commands = context.GetCommandList();
		assets.InitializeSkySphere(device.GetDevice(), commands, definition.environment.sky.radius);
		ocean.Initialize(device.GetDevice(), commands, definition.environment.ocean.size);
		assets.Load(&device, &context,
			{definition.environment,
				{{"occluder", quad,
					{.normalizedExtent = 2, .groundOffset = 0, .origin = ModelOrigin::BoundsCenter}}}});
		context.EndFrame();
		context.WaitForGpu();
		assets.ReleaseUploadResources();
		ocean.ReleaseUploadResources();
		Scene scene;
		const auto& sky = definition.environment.sky;
		scene.SetEnvironment(
			{sky.sunDirection, sky.sunIntensity, sky.sunColor, sky.ambientIntensity, sky.ambientColor});
		double lastCloudMs = 0, lastPostMs = 0;
		const auto render = [&](CloudSettings clouds, const DirectX::XMFLOAT3& position,
								const DirectX::XMFLOAT3& target, const wchar_t* file, float elapsed = 0.0f) {
			Camera camera;
			camera.Initialize(DirectX::XM_PIDIV4, float(width) / height, 1, 10000000);
			camera.LookAt(position, target);
			scene.SetCamera(camera);
			updater.Configure(definition.environment.ocean, clouds);
			updater.Update(
				scene, elapsed, definition.environment.retroFilm, buffers, device.GetDevice(), &context);
			context.BeginFrame();
			context.TransitionResource(device.GetSceneRenderTarget(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
			context.TransitionResource(device.GetSceneDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_WRITE);
			const D3D12_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
			const D3D12_RECT scissor{0, 0, LONG(width), LONG(height)};
			commands->RSSetViewports(1, &viewport);
			commands->RSSetScissorRects(1, &scissor);
			const float clear[] = {0.1f, 0.1f, 0.1f, 1};
			const auto rtv = device.GetSceneRtvHandle(), dsv = device.GetDsvHandle();
			commands->ClearRenderTargetView(rtv, clear, 0, nullptr);
			commands->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
			commands->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
			const FrameContext frame{commands, &device, pipelines, buffers, context.GetFrameIndex()};
			SkyRenderPass{}.Record(frame, assets);
			if (profile) commands->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 4);
			OceanRenderPass{}.Record(frame, assets, ocean);
			if (profile) {
				commands->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 5);
				commands->ResolveQueryData(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 4, 2,
					timestampReadback.Get(), 4 * sizeof(UINT64));
			}
			RenderStatistics statistics;
			ModelRenderPass{}.Record(frame, scene, updater.GetDrawDecisions(), statistics);
			context.TransitionResource(device.GetSceneRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			context.TransitionResource(device.GetSceneDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			if (profile) {
				CloudRenderPass{}.Record(frame); // warm up before the measured repetitions
				commands->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);
				for (int repeat = 0; repeat < 8; ++repeat) CloudRenderPass{}.Record(frame);
				commands->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);
				commands->ResolveQueryData(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2,
					timestampReadback.Get(), 0);
			} else {
				CloudRenderPass{}.Record(frame);
			}
			context.TransitionResource(device.GetCurrentRenderTarget(), D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
			const auto output = device.GetCurrentRtvHandle();
			commands->OMSetRenderTargets(1, &output, FALSE, nullptr);
			if (profile) commands->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2);
			RetroFilmRenderPass{}.Record(frame);
			if (profile) {
				commands->EndQuery(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 3);
				commands->ResolveQueryData(timestamps.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 2, 2,
					timestampReadback.Get(), 2 * sizeof(UINT64));
			}
			context.TransitionResource(device.GetCurrentRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT);
			context.EndFrame();
			context.WaitForGpu();
			if (profile) {
				UINT64* ticks = nullptr;
				const CD3DX12_RANGE reads(0, 6 * sizeof(UINT64)), noWrites(0, 0);
				ThrowIfFailed(timestampReadback->Map(0, &reads, reinterpret_cast<void**>(&ticks)));
				const double cloudMs = double(ticks[1] - ticks[0]) * 1000 / double(frequency) / 8;
				const double postMs = double(ticks[3] - ticks[2]) * 1000 / double(frequency);
				const double oceanMs = double(ticks[5] - ticks[4]) * 1000 / double(frequency);
				timestampReadback->Unmap(0, &noWrites);
				std::cout << "Ocean GPU ms: " << fs::path(file).string() << " " << oceanMs << '\n';
				lastCloudMs = cloudMs;
				lastPostMs = postMs;
				std::cout << "Cloud GPU ms (8 draws): " << fs::path(file).string() << " " << cloudMs << '\n';
				std::cout << "Post GPU ms (includes full-resolution cirrus): " << fs::path(file).string()
					<< " " << postMs << '\n';
			}
			DirectX::ScratchImage capture;
			if (profile && std::wstring_view(file) == L"clouds-cumulus.png") {
				DirectX::ScratchImage cloudCapture;
				ThrowIfFailed(DirectX::CaptureTexture(device.GetCommandQueue(), device.GetCloudRenderTarget(),
					false, cloudCapture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
					D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
				ThrowIfFailed(DirectX::SaveToDDSFile(*cloudCapture.GetImage(0, 0, 0),
					DirectX::DDS_FLAGS_NONE, (fixture / L"clouds-density.dds").c_str()));
			}
			ThrowIfFailed(DirectX::CaptureTexture(device.GetCommandQueue(), device.GetCurrentRenderTarget(),
				false, capture, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT));
			const auto* image = capture.GetImage(0, 0, 0);
			ThrowIfFailed(DirectX::SaveToWICFile(
				*image, DirectX::WIC_FLAGS_NONE, GUID_ContainerFormatPng, (fixture / file).c_str()));
			std::vector<uint8_t> pixels(width * height * 4);
			for (UINT row = 0; row < height; ++row)
				std::memcpy(
					pixels.data() + row * width * 4, image->pixels + row * image->rowPitch, width * 4);
			return pixels;
		};
		if (profile) {
			render(definition.environment.clouds, {0, 1145, 0}, {0, 1000, -8000}, L"ocean-flight-level.png", 12);
			render(definition.environment.clouds, {0, 1145, 0}, {0, -1200, -4000}, L"ocean-flight-down.png", 12);
			render(definition.environment.clouds, {0, 70, 0}, {0, 40, -3000}, L"ocean-low-level.png", 12);
			// Fixed raised-cloud settings; capture a full turn below and inside the layer.
			for (unsigned level = 0; level < 2; ++level) {
				std::array<double, 12> turnTimes{};
				const DirectX::XMFLOAT3 eye = level == 0 ? DirectX::XMFLOAT3{0, 1500, 0} :
					DirectX::XMFLOAT3{16330, 5100, -20900};
				for (unsigned direction = 0; direction < 12; ++direction) {
					const float angle = direction * DirectX::XM_2PI / 12;
					const DirectX::XMFLOAT3 target{eye.x + std::sin(angle) * 8000,
						eye.y + (level == 0 ? 2500.0f : 500.0f), eye.z - std::cos(angle) * 8000};
					const auto file = L"clouds-turn-" + std::to_wstring(level) + L"-" +
						std::to_wstring(direction) + L".png";
					render(definition.environment.clouds, eye, target, file.c_str());
					turnTimes[direction] = lastCloudMs + lastPostMs;
				}
				std::sort(turnTimes.begin(), turnTimes.end());
				std::cout << "Turn GPU Cloud+Post ms: altitude=" << eye.y << " min=" << turnTimes.front()
					<< " median=" << (turnTimes[5] + turnTimes[6]) * 0.5 << " max=" << turnTimes.back()
					<< " spread=" << turnTimes.back() - turnTimes.front() << '\n';
			}
		}
		auto clouds = definition.environment.clouds;
		// Keep the dense interior fixtures at their authored altitude; the turn sweep uses live defaults.
		clouds.bottom = 1800;
		clouds.top = 10500;
		clouds.coverage = 0.34f; // Exercise the denser case as well as the sparse default mix.
		clouds.density = 0.0015f;
		auto disabled = clouds;
		disabled.enabled = false;
		const auto clearSky = render(disabled, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-disabled.png");
		auto mixed = definition.environment.clouds;
		auto thinOnly = mixed;
		thinOnly.density = 0;
		const auto mixedPixels = render(mixed, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-sparse-mix.png");
		const auto thinPixels = render(thinOnly, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-sparse-thin-only.png");
		size_t thickPixels = 0;
		for (size_t i = 0; i < mixedPixels.size(); i += 4)
			if (std::abs(int(mixedPixels[i]) - int(thinPixels[i])) > 8) ++thickPixels;
		Require(thickPixels > size_t(width) * height / 1000 && thickPixels < size_t(width) * height * 35 / 100,
			"Default mixture must include a small amount of visible thick clouds");
		const auto cloudySky = render(clouds, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-cumulus.png");
		size_t changed = 0;
		for (size_t i = 0; i < cloudySky.size(); i += 4)
			if (std::memcmp(cloudySky.data() + i, clearSky.data() + i, 3)) ++changed;
		Require(changed > 10000, "Volumetric cumulus is not visible");
		Require(cloudySky == render(clouds, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-repeat.png"),
			"Clouds flicker at a fixed time/camera");
		auto alternateSeed = clouds;
		alternateSeed.seed += 17;
		Require(cloudySky != render(alternateSeed, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-seed.png"),
			"Cloud shapes and groups ignored seed");
		scene.SetEnvironment({{-sky.sunDirection.x, sky.sunDirection.y, -sky.sunDirection.z},
			sky.sunIntensity, sky.sunColor, sky.ambientIntensity, sky.ambientColor});
		Require(cloudySky != render(clouds, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-reverse-sun.png"),
			"Cloud lighting ignored sun direction");
		scene.SetEnvironment(
			{sky.sunDirection, sky.sunIntensity, sky.sunColor, sky.ambientIntensity, sky.ambientColor});
		Require(render(disabled, {0, 1500, 0}, {0, 0, -1000}, L"clouds-down-clear.png") ==
				render(clouds, {0, 1500, 0}, {0, 0, -1000}, L"clouds-down.png"),
			"Cloud layer drew underneath itself onto close ocean");
		auto stationary = clouds;
		stationary.wind = {0, 0};
		Require(render(stationary, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-stationary.png", 60) !=
				render(clouds, {0, 1500, 0}, {0, 3500, -8000}, L"clouds-wind.png", 60),
			"Clouds ignored wind settings");
		render(clouds, {0, 3200, 0}, {0, 3200, -8000}, L"clouds-inside.png");
		render(clouds, {16330, 2800, -19000}, {16330, 3000, -22613}, L"clouds-near.png");
		render(clouds, {16330, 3000, -20900}, {16330, 3000, -22613}, L"clouds-close.png");
		render(clouds, {8000, 2300, -6000}, {10160, 2700, -10360}, L"clouds-developed-near.png");
		render(clouds, {10000, 2600, -8500}, {10160, 2700, -10360}, L"clouds-developed-close.png");
		const auto developedInside = render(clouds, {16330, 2600, -20900}, {16330, 2600, -21900},
			L"clouds-developed-inside.png");
		const auto developedClear = render(disabled, {16330, 2600, -20900}, {16330, 2600, -21900},
			L"clouds-developed-inside-clear.png");
		size_t insidePixels = 0;
		for (size_t i = 0; i < developedInside.size(); i += 4)
			if (std::abs(int(developedInside[i]) - int(developedClear[i])) > 8) ++insidePixels;
		Require(insidePixels > size_t(width) * height / 2,
			"Camera inside a developed cloud must be surrounded by visible density");
		const auto insideMoved = render(clouds, {16330.5f, 2600, -20900.5f},
			{16330.5f, 2600, -21900.5f}, L"clouds-developed-inside-moved.png");
		double insideMotionDifference = 0;
		for (size_t i = 0; i < insideMoved.size(); i += 4)
			for (size_t channel = 0; channel < 3; ++channel)
				insideMotionDifference += std::abs(int(insideMoved[i + channel]) - int(developedInside[i + channel]));
		Require(insideMotionDifference / (double(width) * height * 3) < 1,
			"Sub-meter movement inside a developed cloud causes unstable shading");
		const auto insideBefore = render(
			clouds, {2000000, 3200, -2000000}, {2000000, 3200, -2008000}, L"clouds-inside-moving-before.png");
		const auto insideAfter = render(clouds, {2000000.5f, 3200, -2000000.5f},
			{2000000.5f, 3200, -2008000.5f}, L"clouds-inside-moving-after.png");
		double motionDifference = 0;
		for (size_t i = 0; i < insideBefore.size(); i += 4)
			for (size_t channel = 0; channel < 3; ++channel)
				motionDifference += std::abs(int(insideBefore[i + channel]) - int(insideAfter[i + channel]));
		Require(motionDifference / (width * height * 3) < 1,
			"Sub-meter movement inside distant clouds causes temporal noise");
		const auto above = render(clouds, {0, 7400, 0}, {0, 7400, -20000}, L"clouds-above.png");
		const auto aboveClear = render(disabled, {0, 7400, 0}, {0, 7400, -20000}, L"clouds-above-clear.png");
		const auto high = render(clouds, {0, 12327, 0}, {0, 12327, -20000}, L"clouds-high.png");
		const auto highClear = render(disabled, {0, 12327, 0}, {0, 12327, -20000}, L"clouds-high-clear.png");
		size_t coveredPixels = 0;
		for (UINT row = height / 2; row < height; ++row)
			for (UINT column = 0; column < width; ++column) {
				const auto offset = (row * width + column) * 4;
				if (std::abs(int(high[offset]) - int(highClear[offset])) > 24) ++coveredPixels;
			}
		Require(coveredPixels > width * height / 100 && coveredPixels < width * height * 35 / 100,
			"Thick clouds must remain scattered; translucent cirrus may veil the clear sky");
		render(clouds, {0, 7400, 0}, {0, 4000, -20000}, L"clouds-above-down.png");
		render(clouds, {0, 4500, 0}, {0, 4500, -20000}, L"clouds-stacked-levels.png");
		auto cirrusOnly = clouds;
		cirrusOnly.density = 0;
		const auto cirrus = render(cirrusOnly, {0, 1500, 0}, {0, 5500, -15000}, L"clouds-cirrus-reference.png");
		const auto cirrusClear = render(disabled, {0, 1500, 0}, {0, 5500, -15000}, L"clouds-cirrus-clear.png");
		size_t cirrusPixels = 0;
		for (size_t i = 0; i < cirrus.size(); i += 4) {
			const int red = int(cirrus[i]) - int(cirrusClear[i]);
			const int blue = int(cirrus[i + 2]) - int(cirrusClear[i + 2]);
			if (red > 8 || blue > 8) {
				++cirrusPixels;
			}
		}
		Require(cirrusPixels > size_t(width) * height / 100,
			"High-altitude cirrus wisps are not visible");
		Require(cirrus == render(cirrusOnly, {0, 1500, 0}, {0, 5500, -15000}, L"clouds-cirrus-repeat.png"),
			"Cirrus has unstable pixel noise at a fixed camera and time");
		const auto cirrusWind = render(cirrusOnly, {0, 1500, 0}, {0, 5500, -15000}, L"clouds-cirrus-wind.png", 60);
		auto fixedCirrus = cirrusOnly;
		fixedCirrus.wind = {0, 0};
		const auto cirrusStationary = render(fixedCirrus, {0, 1500, 0}, {0, 5500, -15000},
			L"clouds-cirrus-stationary.png", 60);
		size_t movingCirrusPixels = 0;
		for (UINT row = 0; row < height * 3 / 4; ++row)
			for (UINT column = 0; column < width; ++column) {
				const auto offset = (size_t(row) * width + column) * 4;
				if (std::memcmp(cirrusStationary.data() + offset, cirrusWind.data() + offset, 3)) ++movingCirrusPixels;
			}
		Require(movingCirrusPixels > size_t(width) * height / 1000,
			"Cirrus world-space wisps do not move with the wind");
		const auto elevated = render(clouds, {0, 7600, 0}, {0, 7600, -20000}, L"clouds-high-relief.png");
		const auto elevatedClear = render(disabled, {0, 7600, 0}, {0, 7600, -20000}, L"clouds-high-relief-clear.png");
		size_t highCloudPixels = 0;
		// Rays above the horizon must see the upper cloud domain, not the lower cloud bank.
		for (UINT row = 0; row < height / 2 - 2; ++row)
			for (UINT column = 0; column < width; ++column) {
				const auto offset = (size_t(row) * width + column) * 4;
				if (std::abs(int(elevated[offset]) - int(elevatedClear[offset])) > 8) ++highCloudPixels;
			}
		Require(highCloudPixels > size_t(width) * height / 500 && highCloudPixels < size_t(width) * height / 3,
			"Upper clouds must exist above 7600m with clear gaps between them");
		const auto below = render(clouds, {0, 900, 0}, {0, 900, -20000}, L"clouds-below-horizon.png");
		const auto belowClear = render(disabled, {0, 900, 0}, {0, 900, -20000}, L"clouds-below-clear.png");
		const auto horizonDifference = [&](const auto& first, const auto& second) {
			size_t pixels = 0;
			for (UINT row = height * 47 / 100; row < height * 58 / 100; ++row)
				for (UINT column = 0; column < width; ++column) {
					const auto offset = (row * width + column) * 4;
					if (std::memcmp(first.data() + offset, second.data() + offset, 3)) ++pixels;
				}
			return pixels;
		};
		Require(horizonDifference(above, aboveClear) > width * 2 &&
				horizonDifference(below, belowClear) > width * 2,
			"Clouds do not extend to the horizon from above/below");
		auto lowQuality = clouds;
		lowQuality.steps = 16;
		const auto lowHorizon = render(lowQuality, {0, 7400, 0}, {0, 7400, -20000}, L"clouds-low-steps.png");
		Require(horizonDifference(lowHorizon, aboveClear) > width * 2,
			"Lowest cloud step budget truncates the horizon");
		Require(device.GetCloudWidth() == (width + 2) / 3 && device.GetCloudHeight() == (height + 2) / 3,
			"Cloud ray marching is not running at one-third resolution");
		DirectX::XMFLOAT4X4 transform;
		DirectX::XMStoreFloat4x4(&transform,
			DirectX::XMMatrixScaling(150, 150, 150) * DirectX::XMMatrixTranslation(0, 1500, -100));
		scene.AddObject(assets.GetModel("occluder"), transform);
		Require(render(disabled, {0, 1500, 0}, {0, 1500, -1000}, L"clouds-occluder-clear.png") ==
				render(clouds, {0, 1500, 0}, {0, 1500, -1000}, L"clouds-occluder.png"),
			"Clouds covered a closer foreground model");
		ComPtr<ID3D12InfoQueue> queue;
		if (SUCCEEDED(device.GetDevice()->QueryInterface(IID_PPV_ARGS(&queue))))
			for (UINT64 i = 0; i < queue->GetNumStoredMessages(); ++i) {
				SIZE_T bytes = 0;
				queue->GetMessage(i, nullptr, &bytes);
				std::vector<uint8_t> data(bytes);
				auto* message = reinterpret_cast<D3D12_MESSAGE*>(data.data());
				ThrowIfFailed(queue->GetMessage(i, message, &bytes));
				Require(message->Severity != D3D12_MESSAGE_SEVERITY_ERROR &&
						message->Severity != D3D12_MESSAGE_SEVERITY_CORRUPTION,
					message->pDescription);
			}
	}
	DestroyWindow(window);
	std::cout << "PASS: volumetric cumulus GPU readback, deterministic shading, wind, inside clouds and "
				 "foreground/ocean depth occlusion\n";
}
