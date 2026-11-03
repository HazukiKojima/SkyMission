#pragma once
#include "Game/UI/RadarMapPresenter.h"
#include "Engine/Camera/Camera.h"
#include "Game/UI/HudProjection.h"

void TestRadarMap() {
	using namespace DirectX;
	for (const XMFLOAT2 size : {XMFLOAT2{800, 600}, XMFLOAT2{1920, 1080}, XMFLOAT2{2560, 1080}}) {
		Engine::RenderView view;
		view.width = unsigned(size.x);
		view.height = unsigned(size.y);
		const auto layout = Game::RadarMapPresenter::Layout(view);
		const auto approximately = [](float a, float b) {
			return std::abs(a - b) < 0.001f;
		};
		Require(approximately(layout.origin.x, 48 * layout.scale) &&
				approximately(layout.origin.y + layout.size, size.y - 48 * layout.scale),
			"Radar is not anchored to the actual lower-left viewport corner");
		const XMFLOAT3 player{12000, 3000, -18000};
		for (const XMFLOAT2 direction : {XMFLOAT2{0, -1}, XMFLOAT2{1, 0}, XMFLOAT2{0, 1}, XMFLOAT2{-1, 0}}) {
			for (float distance : {0.0f, 5000.0f, 10000.0f, 20000.0f}) {
				const auto point = Game::RadarMapPresenter::Project(
					{player.x + direction.x * distance, -90000, player.z + direction.y * distance}, player,
					layout);
				const float radius = std::min(distance / 10000, 1.0f) * layout.radius;
				Require(point.valid && point.outside == (distance > 10000) &&
						approximately(point.pixel.x, layout.center.x - direction.x * radius) &&
						approximately(point.pixel.y, layout.center.y + direction.y * radius),
					"Radar cardinal direction, horizontal range or outer-edge clamping failed");
			}
		}
		// 実際の左手座標系カメラを北へ向け、画面とレーダーの左右が一致することを確認する
		Engine::Camera northCamera;
		northCamera.Initialize(XM_PIDIV4, size.x / size.y, 1, 20000);
		northCamera.LookAt(player, {player.x, player.y, player.z - 10000});
		auto northView = view;
		XMStoreFloat4x4(&northView.view, northCamera.GetView());
		XMStoreFloat4x4(&northView.projection, northCamera.GetProjection());
		for (float side : {-1.0f, 1.0f}) {
			const XMFLOAT3 contact{player.x + side * 1000, player.y, player.z - 5000};
			const auto screen = Game::ProjectHudPoint(contact, northView);
			const auto radar = Game::RadarMapPresenter::Project(contact, player, layout);
			Require(
				screen.visible && (screen.pixel.x - size.x * 0.5f) * (radar.pixel.x - layout.center.x) > 0,
				"Radar horizontal placement is mirrored against the game camera");
		}
		const auto diagonal =
			Game::RadarMapPresenter::Project({player.x + 10000, player.y, player.z - 10000}, player, layout);
		Require(diagonal.outside &&
				approximately(
					std::hypot(diagonal.pixel.x - layout.center.x, diagonal.pixel.y - layout.center.y),
					layout.radius),
			"Radar range is not radial");
		Require(
			!Game::RadarMapPresenter::Project({std::numeric_limits<float>::quiet_NaN(), 0, 0}, player, layout)
				.valid,
			"Radar accepted a non-finite contact");
		Game::CombatSnapshot snapshot;
		snapshot.playerPosition = player;
		snapshot.enemies = {{1, {player.x + 5000, 400, player.z}, {0, 0, -500}, "CONTACT", 100, {1, 0, 0}}};
		Engine::HudDrawList list;
		const auto build = [&] {
			list.Reset(view.width, view.height);
			Game::RadarMapPresenter{}.Build(snapshot, view, list);
		};
		const auto sameVertex = [](const Engine::HudVertex& a, const Engine::HudVertex& b) {
			return a.position.x == b.position.x && a.position.y == b.position.y && a.uv.x == b.uv.x &&
				a.uv.y == b.uv.y && a.color.x == b.color.x && a.color.y == b.color.y &&
				a.color.z == b.color.z && a.color.w == b.color.w;
		};
		build();
		const auto baseline = list.GetVertices();
		snapshot.enemies.front().velocity = {900, 500, 0};
		snapshot.muzzle = {100000, 100000, 100000};
		snapshot.forward = {1, 0, 0};
		XMStoreFloat4x4(&view.view, XMMatrixRotationY(XM_PIDIV2));
		build();
		Require(std::equal(baseline.begin(), baseline.end(), list.GetVertices().begin(),
					list.GetVertices().end(), sameVertex),
			"Radar depends on camera rotation, muzzle position or aircraft velocity");
		snapshot.playerHeading = {1, 0, 0};
		build();
		float left = layout.center.x, right = layout.center.x;
		for (auto vertex = list.GetVertices().end() - 18; vertex != list.GetVertices().end(); ++vertex) {
			const float pixelX = (vertex->position.x + 1) * size.x * 0.5f;
			left = std::min(left, pixelX);
			right = std::max(right, pixelX);
		}
		Require(layout.center.x - left > right - layout.center.x,
			"Positive-X aircraft heading did not point toward radar left");
		Require(std::equal(baseline.begin(), baseline.end() - 18, list.GetVertices().begin(), sameVertex) &&
				!std::equal(baseline.end() - 18, baseline.end(), list.GetVertices().end() - 18, sameVertex),
			"Player heading rotated radar contacts or failed to rotate the player icon");
		const auto eastHeading = list.GetVertices();
		snapshot.enemies.front().heading = {0, 0, -1};
		build();
		Require(!std::equal(eastHeading.end() - 36, eastHeading.end() - 18, list.GetVertices().end() - 36,
					sameVertex),
			"Enemy icon ignored its actual heading");
		snapshot.selected = 1;
		build();
		Require(
			list.GetVertices().size() == baseline.size() + 48, "Selected radar contact was not bracketed");
		snapshot.selected = Engine::InvalidObjectId;
		snapshot.enemies.front().center.x = player.x + 20000;
		build();
		Require(list.GetVertices().size() == baseline.size() - 6,
			"Outside contact did not switch to an edge marker");
		snapshot.enemies.clear();
		build();
		Require(list.GetVertices().size() == baseline.size() - 18, "Destroyed aircraft remained on radar");
		snapshot.enemies = {{1, {player.x + 5000, 0, player.z}, {}, "CONTACT", 100}};
		snapshot.playerPosition.x += 1000;
		const auto moved = Game::RadarMapPresenter::Project(
			snapshot.enemies.front().center, snapshot.playerPosition, layout);
		Require(approximately(moved.pixel.x, layout.center.x - layout.radius * 0.4f),
			"Radar did not follow player translation");
		for (const auto& vertex : list.GetVertices())
			Require(std::isfinite(vertex.position.x) && std::isfinite(vertex.position.y) &&
					vertex.position.x >= -1.01f && vertex.position.x <= 1.01f &&
					vertex.position.y >= -1.01f && vertex.position.y <= 1.01f,
				"Radar escaped the viewport");
	}
	Engine::RenderView zero;
	Engine::HudDrawList empty;
	empty.Reset(0, 0);
	Game::RadarMapPresenter{}.Build({}, zero, empty);
	Require(empty.GetVertices().empty(), "Zero-sized view generated radar vertices");
	std::cout
		<< "PASS: north-up radar, cardinal ranges, headings, selection, lifecycle and viewport anchoring\n";
}
