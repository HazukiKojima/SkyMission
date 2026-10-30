#pragma once
#include "Game/Combat/CombatController.h"
#include "Game/Combat/GunAimSolver.h"
#include "Game/Combat/TargetingSystem.h"
#include "Game/Combat/ProjectileCollision.h"
#include "Game/UI/CombatHudPresenter.h"

void TestCombatMath() {
	using namespace DirectX;
	RawInputState raw;
	raw.SetActive(true);
	RAWKEYBOARD key{}; key.VKey=VK_TAB; key.MakeCode=0x0f;
	const auto keyboard=reinterpret_cast<HANDLE>(std::uintptr_t(17));
	raw.ProcessKeyboard(keyboard,key); key.Flags=RI_KEY_BREAK; raw.ProcessKeyboard(keyboard,key);
	Require(CombatController{}.ReadInput(raw.ConsumeFrame()).nextTarget,"Short Tab press was lost");
	Require(!CombatController{}.ReadInput(raw.ConsumeFrame()).nextTarget,"Tab edge was consumed twice");
	key.Flags=RI_KEY_MAKE; raw.ProcessKeyboard(keyboard,key); raw.ConsumeFrame(); raw.ProcessKeyboard(keyboard,key);
	Require(!raw.ConsumeFrame().WasKeyPressed(VK_TAB),"Tab autorepeat produced another edge");
	key.Flags=RI_KEY_BREAK; raw.ProcessKeyboard(keyboard,key); key.Flags=RI_KEY_MAKE; raw.ProcessKeyboard(keyboard,key);
	raw.SetActive(false); raw.SetActive(true);
	Require(!raw.ConsumeFrame().WasKeyPressed(VK_TAB),"Focus regain restored a Tab edge");

	CombatTarget target{1,{0,0,-900},{},"ENEMY 01",100};
	const auto aim=SolveGunAim({}, {}, target,900,3);
	Require(aim && std::abs(aim->seconds-1)<0.001f && aim->direction.z < -0.999f,"Stationary intercept failed");
	target.velocity={100,0,0};
	const auto lateral=SolveGunAim({}, {}, target,900,3);
	Require(lateral && lateral->direction.x>0.1f,"Lateral target did not require lead");
	const auto inherited=SolveGunAim({}, {100,0,0}, target,900,3);
	Require(inherited && std::abs(inherited->direction.x)<0.001f,"Shooter velocity was not subtracted");
	target.velocity={0,0,900};
	const auto linear=SolveGunAim({}, {}, target,900,3);
	Require(linear && std::abs(linear->seconds-0.5f)<0.001f,"Linear intercept case failed");
	target.velocity={0,0,-1800};
	Require(!SolveGunAim({}, {}, target,900,3),"Unreachable target had an intercept");
	target.velocity={};
	Require(!SolveGunAim({}, {}, target,900,0.5f),"Intercept beyond lifetime was accepted");
	Require(IsWithinAngle({0,0,-1},{0.02f,0,-1},3) && !IsWithinAngle({0,0,-1},{0.1f,0,-1},3),"Assist angle boundary failed");

	std::vector<CombatTarget> targets{{3,{200,0,-900},{},"3",100},{1,{0,0,-900},{},"1",100},{2,{0,0,900},{},"2",100}};
	TargetingSystem targeting; targeting.Configure({});
	targeting.Update(targets,{}, {0,0,-1},false,0.25f);
	Require(targeting.GetSelected()==1 && targeting.GetState()==LockState::Acquiring,"Initial automatic selection failed");
	targeting.Update(targets,{}, {0,0,-1},false,0.25f);
	Require(targeting.GetState()==LockState::Locked,"Acquire timer did not lock");
	targeting.Update(targets,{}, {0,0,-1},true,0);
	Require(targeting.GetSelected()==2 && targeting.GetState()==LockState::Selected,"Tab did not select rear target");
	targeting.Update(targets,{}, {0,0,-1},true,0);
	Require(targeting.GetSelected()==3,"Tab did not follow stable IDs");
	targeting.Update(targets,{}, {0,0,-1},true,0);
	Require(targeting.GetSelected()==1,"Tab did not wrap");
	targeting.Update(targets,{}, {0,0,-1},false,0.25f); targeting.Update(targets,{}, {0,0,-1},false,0.25f);
	targets[1].center={0,0,900};
	targeting.Update(targets,{}, {0,0,-1},false,0.1f);
	Require(targeting.GetState()==LockState::Locked,"Lock grace was skipped");
	targeting.Update(targets,{}, {0,0,-1},false,0.15f);
	Require(targeting.GetState()==LockState::Selected,"Lock grace did not expire");
	targets.erase(targets.begin()+1); targeting.Update(targets,{}, {0,0,-1},false,0);
	Require(targeting.GetSelected()==3,"Destroyed selected target was not replaced");
	targeting.Update({}, {}, {0,0,-1},false,0);
	Require(targeting.GetState()==LockState::None,"Empty target list retained selection");

	CollisionTarget box{4,{{-1,-1,-1},{1,1,1}},{0,10,-50},{}};
	XMStoreFloat4x4(&box.world,XMMatrixRotationY(0.7f)*XMMatrixTranslation(0,10,-50));
	Require(IntersectMovingTarget({0,10,0},{0,10,-100},box,0,1).has_value(),"Fast projectile missed rotated OBB");
	Require(!IntersectMovingTarget({20,10,0},{20,10,-100},box,0,1),"OBB accepted a distant miss");
	box.previousPosition={-10,10,-50}; XMStoreFloat4x4(&box.world,XMMatrixTranslation(10,10,-50));
	Require(IntersectMovingTarget({0,10,0},{0,10,-100},box,0,1).has_value(),"Moving target crossing was missed");
	std::cout<<"PASS: Tab edges, targeting/lock/grace, intercepts, assist cone and moving OBB collision\n";
}

void TestCombatDamage(const fs::path& repository, const fs::path& fixture) {
	using namespace DirectX;
	const auto assets=repository/L"Assets";
	const auto definition=FlightSceneLoader{}.Load({.environment=assets/L"Environment/environment.json",
		.playerAircraft=assets/L"Aircraft/N1K2JShidenKai/N1K2JShidenKai.json",.enemyAircraft=assets/L"Aircraft/F4U4Corsair/F4U4Corsair.json",
		.retroFilm=assets/L"Environment/retroFilm.json"});
	const auto cubePath=fixture/L"combat-box.obj";
	std::ofstream(cubePath)<<"v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
		"f 1 2 3\nf 1 3 4\nf 5 7 6\nf 5 8 7\nf 1 5 6\nf 1 6 2\nf 4 3 7\nf 4 7 8\n";
	const auto window=CreateWindowW(L"STATIC",L"Combat test (hidden)",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
	Require(window!=nullptr,"Combat test window failed");
	{
		RenderDevice device; device.Initialize(window,64,64); CommandContext context; context.Initialize(&device); context.BeginFrame();
		auto player=std::make_shared<Model>(), enemy=std::make_shared<Model>();
		Require(player->Load(device.GetDevice(),context.GetCommandList(),&device,(fixture/L"muzzles.dae").wstring()),"Combat player fixture failed");
		ModelLoadOptions centered; centered.origin=ModelOrigin::BoundsCenter;
		Require(enemy->Load(device.GetDevice(),context.GetCommandList(),&device,cubePath.wstring(),centered),"Combat enemy fixture failed");
		Require(enemy->GetLocalBounds() && std::abs(enemy->GetLocalBounds()->maximum.z-5)<0.001f,"Normalized model bounds were incorrect");
		{
			FlightScene scene; scene.Initialize(1,1,10000);
			auto combat=definition.combat; combat.bulletDamage=100; scene.ConfigureCombat(combat);
			scene.SetPlayerAircraftFlightParameters(definition.playerAircraft.flightParameters);
			scene.AddPlayerObject(player); scene.ConfigurePlayerWeapon(definition.playerWeapon,player);
			scene.AddEnemyAircraft(enemy,{0,15,-160},5,definition.enemyAircraft.flightParameters,definition.enemyAIParameters,100,"ENEMY 01");
			scene.Update({},0.25f,{}, {true});
			const auto& state=scene.GetCombatSnapshot();
			Require(state.enemies.empty() && state.destroyedSeconds>0 && state.allDownSeconds>0,
				"Projectile damage did not destroy enemy and notify all-down");
			Require(state.selected==Engine::InvalidObjectId && state.lockState==LockState::None,"Destroyed enemy retained target reference");
			scene.Update({},0.25f,{}, {true});
			Require(scene.GetCombatSnapshot().allDownSeconds<3,"All-down timer was repeatedly restarted");
		}
		GunSettings gun=definition.playerWeapon.gun;
		std::array<XMFLOAT3,4> muzzles{}; XMFLOAT4X4 world,pose;
		XMStoreFloat4x4(&world,XMMatrixTranslation(0,20,0)); pose=world;
		CollisionTarget nearBox{1,*enemy->GetLocalBounds(),{0,20,-100},{}}, farBox{2,*enemy->GetLocalBounds(),{0,20,-160},{}};
		XMStoreFloat4x4(&nearBox.world,XMMatrixTranslation(0,20,-100)); XMStoreFloat4x4(&farBox.world,XMMatrixTranslation(0,20,-160));
		const std::array boxes{farBox,nearBox};
		ProjectileSystem bullets; bullets.Configure(gun,player,{0,1,0}); Scene scene;
		bullets.FireVolley(scene,muzzles,world,pose,{0,0,-1},{},0.25f,{},boxes,0.25f);
		const auto hits=bullets.TakeHits();
		Require(hits.size()==4 && std::all_of(hits.begin(),hits.end(),[](const HitEvent& hit){return hit.target==1;}),"Projectile did not hit nearest enemy once");
		Require(bullets.GetCount()==0 && bullets.TakeHits().empty(),"Consumed projectile generated duplicate hit");
		gun.lifetime=0.05f; bullets.Configure(gun,player,{0,1,0});
		bullets.FireVolley(scene,muzzles,world,pose,{0,0,-1},{},0.25f,{},boxes,0.25f);
		Require(bullets.TakeHits().empty(),"Expired projectile hit beyond its lifetime");
		gun.lifetime=3; bullets.Configure(gun,player,{0,1,0});
		AimAssistance assistance{CombatTarget{3,{30,20,-1000},{},"TEST",100},3};
		bullets.FireVolley(scene,muzzles,world,pose,{0,0,-1},{},0,assistance);
		bullets.Update(scene,0.1f);
		Require(scene.GetObjects().front().worldTransform._41>1,"Locked in-cone target did not adjust projectile direction");
		const auto fixedDirection=scene.GetObjects().front().worldTransform._41;
		bullets.Update(scene,0.1f);
		Require(std::abs(scene.GetObjects().front().worldTransform._41-2*fixedDirection)<0.001f,"Projectile homed after firing");
		context.EndFrame(); context.WaitForGpu();
	}
	DestroyWindow(window);
	std::cout<<"PASS: model bounds, damage/defeat/target invalidation, nearest collision, lifetime and straight assisted fire\n";
}

void TestHudRendering(const fs::path& fixture) {
	using namespace DirectX;
	const HWND window=CreateWindowW(L"STATIC",L"HUD regression (hidden)",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
	Require(window!=nullptr,"HUD test window failed");
	{
		RenderDevice device; device.Initialize(window,800,600);
		CommandContext context; context.Initialize(&device);
		HudRenderer renderer;
		context.BeginFrame(); renderer.Initialize(device,context.GetCommandList()); context.EndFrame(); context.WaitForGpu(); renderer.ReleaseUploadResources();
		for (const auto size : {XMFLOAT2{800,600},XMFLOAT2{1920,1080}}) {
			const UINT width=UINT(size.x),height=UINT(size.y);
			context.WaitForGpu(); device.Resize(width,height); context.SynchronizeFrameIndex(device);
			Engine::RenderView view; view.width=width; view.height=height;
			XMStoreFloat4x4(&view.view,XMMatrixIdentity());
			XMStoreFloat4x4(&view.projection,XMMatrixPerspectiveFovLH(XM_PIDIV4,float(width)/height,1,10000));
			CombatSnapshot snapshot;
			snapshot.enemies={{1,{0,30,1000},{},"ENEMY 01",100},{2,{-200,50,1500},{},"ENEMY 02",100},{3,{5000,0,1000},{},"ENEMY 03",100}};
			snapshot.selected=1; snapshot.lockState=LockState::Locked; snapshot.forward={0,0,1};
			snapshot.gunImpactPoint=XMFLOAT3{20,10,1000};
			snapshot.aim=AimSolution{{0.05f,0,1},{60,50,1100},1}; snapshot.assistReady=true; snapshot.hitSeconds=0.15f;
			Engine::HudDrawList list; CombatHudPresenter{}.Build(snapshot,view,list);
			Require(!list.GetVertices().empty() && list.GetVertices().size()<Engine::HudDrawList::MaxVertices,"HUD mesh was empty or overflowed");
			Require(ProjectHudPoint({0,0,100},view).visible && ProjectHudPoint({0,0,-100},view).behind,"HUD projection front/rear failed");
			snapshot.selected=3; CombatHudPresenter{}.Build(snapshot,view,list);
			for (const auto& vertex:list.GetVertices()) Require(std::isfinite(vertex.position.x)&&std::isfinite(vertex.position.y),"HUD projection generated NaN");
			snapshot.selected=1; CombatHudPresenter{}.Build(snapshot,view,list);
			context.BeginFrame(); auto* commands=context.GetCommandList(); auto* target=device.GetCurrentRenderTarget();
			context.TransitionResource(target,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);
			const auto rtv=device.GetCurrentRtvHandle(); const float background[]={0.015f,0.025f,0.035f,1};
			commands->ClearRenderTargetView(rtv,background,0,nullptr); commands->OMSetRenderTargets(1,&rtv,FALSE,nullptr);
			const D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1}; const D3D12_RECT scissor{0,0,LONG(width),LONG(height)};
			commands->RSSetViewports(1,&viewport); commands->RSSetScissorRects(1,&scissor);
			renderer.Record(device,commands,list);
			context.TransitionResource(target,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 bytes=0;
			const auto desc=target->GetDesc(); device.GetDevice()->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
			ComPtr<ID3D12Resource> readback; const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_READBACK);
			const auto buffer=CD3DX12_RESOURCE_DESC::Buffer(bytes);
			ThrowIfFailed(device.GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
			const CD3DX12_TEXTURE_COPY_LOCATION source(target,0),destination(readback.Get(),footprint);
			commands->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
			context.TransitionResource(target,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);
			context.EndFrame(); context.WaitForGpu();
			void* mapped=nullptr; const CD3DX12_RANGE range(0,SIZE_T(bytes)); ThrowIfFailed(readback->Map(0,&range,&mapped));
			ScratchImage image; ThrowIfFailed(image.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,width,height,1,1));
			const auto* pixels=image.GetImage(0,0,0); size_t green=0;
			for (UINT y=0;y<height;++y) {
				const auto* row=static_cast<const uint8_t*>(mapped)+footprint.Offset+size_t(y)*footprint.Footprint.RowPitch;
				std::memcpy(pixels->pixels+y*pixels->rowPitch,row,size_t(width)*4);
				for (UINT x=0;x<width;++x) if (row[x*4+1]>80 && row[x*4+1]>row[x*4]*2) ++green;
			}
			const CD3DX12_RANGE noWrites(0,0); readback->Unmap(0,&noWrites);
			Require(green>100,"HUD did not produce visible green pixels");
			const auto path=fixture/("hud-"+std::to_string(width)+".png");
			ThrowIfFailed(SaveToWICFile(*pixels,WIC_FLAGS_NONE,GUID_ContainerFormatPng,path.c_str()));
		}
	}
	DestroyWindow(window);
	std::cout<<"PASS: native HUD atlas, projection, GPU readback, 800x600/1920x1080 and resize\n";
}
