#pragma once
#include "Engine/Renderer/Device/RendererPasses.h"
#include "Engine/Renderer/Device/RendererFrameUpdater.h"
#include "Engine/Renderer/Hud/HudRenderer.h"

// 本番のモデルと描画パスで、導入の構図と暗転前後をPNGに記録する
void TestOpeningCinematicRendering(const fs::path& repository, const fs::path& fixture) {
	const auto root = repository / L"Assets";
	const auto definition = FlightSceneLoader{}.Load({.environment = root / L"Environment/environment.json",
		.playerAircraft = root / L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",
		.enemyAircraft = root / L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm = root / L"Environment/retroFilm.json"});
	const HWND window = CreateWindowW(L"STATIC", L"Opening capture (hidden)", WS_OVERLAPPEDWINDOW, 0, 0, 1280,
		720, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
	Require(window != nullptr, "Opening capture window failed");
	{
		RenderDevice device;
		device.Initialize(window, 1280, 720);
		CommandContext context;
		context.Initialize(&device);
		RendererPipelines pipelines;
		pipelines.Initialize(device.GetDevice());
		RendererAssets assets;
		OceanGeometry ocean;
		RendererConstantBuffers buffers;
		buffers.Initialize(device.GetDevice());
		RendererFrameUpdater updater;
		updater.Configure(definition.environment.ocean, definition.environment.clouds);
		HudRenderer hudRenderer;
		context.BeginFrame();
		auto* commands = context.GetCommandList();
		assets.InitializeSkySphere(device.GetDevice(), commands, definition.environment.sky.radius);
		ocean.Initialize(device.GetDevice(), commands, definition.environment.ocean.size);
		assets.Load(&device, &context,
			{definition.environment,
				{{EnemyModelId, definition.enemyAircraft.model, definition.enemyAircraft.modelImport},
					{PlayerModelId, definition.playerAircraft.model,
						definition.playerAircraft.modelImport}}});
		hudRenderer.Initialize(device, commands);
		context.EndFrame();
		context.WaitForGpu();
		assets.ReleaseUploadResources();
		ocean.ReleaseUploadResources();
		hudRenderer.ReleaseUploadResources();
		FlightScene flight;
		flight.ConfigureCombat(definition.combat);
		flight.Initialize(1280.0f / 720, 1, definition.environment.ocean.size);
		flight.SetPlayerAircraftPosition({0, 1500, 0});
		flight.SetPlayerAircraftVisualScale(definition.playerAircraft.visualScale);
		flight.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
		flight.AddPlayerObject(assets.GetModel(PlayerModelId));
		flight.ConfigureMission(definition.mission, assets.GetModel(EnemyModelId),
			definition.enemyAircraft.visualScale, definition.enemyAircraft.flightParameters,
			definition.enemyAIParameters);
		const auto& sky = definition.environment.sky;
		flight.SetEnvironment(
			{sky.sunDirection, sky.sunIntensity, sky.sunColor, sky.ambientIntensity, sky.ambientColor});
		flight.OnResize(1280, 720);
		flight.Update({}, 0.001f);
		flight.EnableOpening(definition.opening, assets.GetModel(EnemyModelId),
			definition.enemyAircraft.visualScale, definition.environment.ocean.size);
		Require(flight.GetRenderScene().GetObjects().size() == 4,
			"Opening cinematic did not display the four-aircraft formation");
		{
			auto custom = definition.opening;
			custom.formationOffsets = {{0, 0, 0}, {2, 1, -3}};
			custom.originOffsetMeters = {10, 20, -30};
			auto changed = flight;
			changed.EnableOpening(custom, assets.GetModel(EnemyModelId), definition.enemyAircraft.visualScale,
				definition.environment.ocean.size);
			const auto& objects = changed.GetRenderScene().GetObjects();
			Require(objects.size() == 2, "Configured formation count was ignored");
			const auto& oldWorld = flight.GetRenderScene().GetObjects().front().worldTransform;
			Require(std::abs(objects.front().worldTransform._41 - oldWorld._41 - 10) < 0.01f &&
					std::abs(objects.front().worldTransform._42 - oldWorld._42 - 20) < 0.01f &&
					std::abs(objects.front().worldTransform._43 - oldWorld._43 - 4970) < 0.01f,
				"Configured opening origin was ignored");
			const auto& bounds = *assets.GetModel(EnemyModelId)->GetLocalBounds();
			const float scale = std::max(0.01f,
				std::max({bounds.maximum.x - bounds.minimum.x, bounds.maximum.y - bounds.minimum.y,
					bounds.maximum.z - bounds.minimum.z}) *
					definition.enemyAircraft.visualScale / 200);
			Require(std::abs(objects[1].worldTransform._41 - objects[0].worldTransform._41 -
						2 * custom.formationSpacingMeters * scale) < 0.01f &&
					std::abs(objects[1].worldTransform._43 - objects[0].worldTransform._43 +
						3 * custom.formationDepthMeters * scale) < 0.01f,
				"Configured formation offsets lost their model-relative scaling");
		}
		const auto capture = [&](const wchar_t* file, UINT width, UINT height) {
			device.Resize(width, height);
			context.SynchronizeFrameIndex(device);
			flight.OnResize(width, height);
			const auto& scene = flight.GetRenderScene();
			updater.Update(scene, flight.GetCombatSnapshot().opening.elapsedSeconds,
				definition.environment.retroFilm, buffers, device.GetDevice(), &context);
			context.BeginFrame();
			context.TransitionResource(device.GetSceneRenderTarget(),
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
			context.TransitionResource(device.GetSceneDepth(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				D3D12_RESOURCE_STATE_DEPTH_WRITE);
			const D3D12_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
			const D3D12_RECT scissor{0, 0, LONG(width), LONG(height)};
			commands->RSSetViewports(1, &viewport);
			commands->RSSetScissorRects(1, &scissor);
			const float clear[] = {0, 0, 0, 1};
			const auto rtv = device.GetSceneRtvHandle(), dsv = device.GetDsvHandle();
			commands->ClearRenderTargetView(rtv, clear, 0, nullptr);
			commands->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
			commands->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
			const FrameContext frame{commands, &device, pipelines, buffers, context.GetFrameIndex()};
			SkyRenderPass{}.Record(frame, assets);
			OceanRenderPass{}.Record(frame, assets, ocean);
			RenderStatistics statistics;
			ModelRenderPass{}.Record(frame, scene, updater.GetDrawDecisions(), statistics);
			context.TransitionResource(device.GetSceneRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			context.TransitionResource(device.GetSceneDepth(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			CloudRenderPass{}.Record(frame);
			context.TransitionResource(device.GetCurrentRenderTarget(), D3D12_RESOURCE_STATE_PRESENT,
				D3D12_RESOURCE_STATE_RENDER_TARGET);
			const auto output = device.GetCurrentRtvHandle();
			commands->OMSetRenderTargets(1, &output, FALSE, nullptr);
			RetroFilmRenderPass{}.Record(frame);
			HudDrawList hud;
			hud.Reset(width, height);
			auto view = updater.GetRenderView();
			view.width = width;
			view.height = height;
			CombatHudPresenter{}.Build(flight.GetCombatSnapshot(), view, hud);
			Require(!hud.GetVertices().empty(), "Opening capture omitted its overlay");
			hudRenderer.Record(device, commands, hud);
			context.TransitionResource(device.GetCurrentRenderTarget(), D3D12_RESOURCE_STATE_RENDER_TARGET,
				D3D12_RESOURCE_STATE_PRESENT);
			context.EndFrame();
			context.WaitForGpu();
			DirectX::ScratchImage pixels;
			ThrowIfFailed(DirectX::CaptureTexture(device.GetCommandQueue(), device.GetCurrentRenderTarget(),
				false, pixels, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_PRESENT));
			if (flight.GetCombatSnapshot().opening.fadeOpacity >= 0.999f) {
				const auto* image = pixels.GetImage(0, 0, 0);
				const auto* center = image->pixels + height / 2 * image->rowPitch + width / 2 * 4;
				Require(
					center[0] < 2 && center[1] < 2 && center[2] < 2, "Opening cut was not covered by black");
			}
			ThrowIfFailed(DirectX::SaveToWICFile(*pixels.GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE,
				GUID_ContainerFormatPng, (fixture / file).c_str()));
		};
		capture(L"opening-00.png", 1280, 720);
		for (int i = 0; i < 12; ++i)
			flight.Update({}, 0.25f);
		capture(L"opening-03.png", 1280, 720);
		capture(L"opening-wide.png", 1920, 810);
		capture(L"opening-small.png", 800, 600);
		for (int i = 0; i < 4; ++i)
			flight.Update({}, 0.25f);
		capture(L"opening-black.png", 1280, 720);
		for (int i = 0; i < 3; ++i)
			flight.Update({}, 0.25f);
		capture(L"opening-player.png", 1280, 720);
		flight.Update({}, 0.25f);
		capture(L"opening-gameplay.png", 1280, 720);
	}
	DestroyWindow(window);
	std::cout << "PASS: opening real-model captures, resize, black transition and player handoff\n";
}
