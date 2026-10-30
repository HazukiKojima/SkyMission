#include "CombatHudPresenter.h"
#include <algorithm>
#include <cmath>

namespace Game {
	using namespace DirectX;
	HudProjection ProjectHudPoint(const XMFLOAT3& point, const Engine::RenderView& view) {
		const auto clip = XMVector4Transform(XMVectorSet(point.x, point.y, point.z, 1),
			XMLoadFloat4x4(&view.view) * XMLoadFloat4x4(&view.projection));
		const float w = XMVectorGetW(clip), divisor = std::max(std::abs(w), 1e-5f);
		const float x = XMVectorGetX(clip) / divisor, y = XMVectorGetY(clip) / divisor, z = XMVectorGetZ(clip) / divisor;
		return { {(x + 1) * view.width * 0.5f,(1 - y) * view.height * 0.5f},
			w > 0 && std::abs(x) <= 1 && std::abs(y) <= 1 && z >= 0 && z <= 1, w <= 0 };
	}

	namespace {
		void DrawExplosion(const ExplosionEffect& effect, const Engine::RenderView& view, Engine::HudDrawList& list) {
			if (effect.age < 0 || effect.age >= ExplosionEffect::lifetime) return;
			// 演出全体の予算を確保し、照準と文字の描画領域も残す
			if (list.GetVertices().size() + 4800 > Engine::HudDrawList::MaxVertices - 6000) return;
			const float t = effect.age, size = effect.radius;
			const auto cameraWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view.view));
			const auto right = cameraWorld.r[0];
			const auto origin = XMLoadFloat3(&effect.position);
			const auto drift = XMLoadFloat3(&effect.velocity) * (0.18f * (1 - std::exp(-t * 2)));
			const auto disc = [&](XMVECTOR location, float radius, XMFLOAT4 color, unsigned seed = 0) {
				XMFLOAT3 point, edge;
				XMStoreFloat3(&point, location); XMStoreFloat3(&edge, location + right * radius);
				const auto a = ProjectHudPoint(point, view), b = ProjectHudPoint(edge, view);
				if (a.behind || b.behind) return;
				const float pixels = std::hypot(b.pixel.x-a.pixel.x, b.pixel.y-a.pixel.y);
				if (a.pixel.x + pixels < 0 || a.pixel.y + pixels < 0 ||
					a.pixel.x - pixels > view.width || a.pixel.y - pixels > view.height) return;
				if (seed) list.CloudPuff(a.pixel, std::min(pixels, float(std::max(view.width, view.height))*2),
					seed*1.71f + t*(seed%2 ? 0.12f : -0.09f), seed, color);
				else list.SoftDisc(a.pixel, std::min(pixels, float(std::max(view.width, view.height))*2), color);
			};
			// 大きさと上昇速度の異なる煙塊を重ねて乱流を表現する
			for (int i = 0; i < 22; ++i) {
				const float phase = i*2.399963f, variation = 0.55f+(i%7)*0.11f;
				const float spread = size*(0.2f + (1-std::exp(-t*1.4f))*1.3f)*variation;
				const auto location = origin + drift + XMVectorSet(std::cos(phase)*spread,
					std::sin(phase)*spread*0.6f + t*size*(0.35f+(i%4)*0.12f), std::sin(phase*2)*spread*0.5f,0);
				const float alpha = std::min(t*5,0.85f)*std::clamp((3.5f-t)/1.3f,0.0f,1.0f);
				const float shade=0.09f+(i%5)*0.025f;
				disc(location,size*(0.38f+t*0.27f)*variation,{shade,shade*0.95f,shade*0.88f,alpha},i+1);
			}
			// 火炎の塊ごとに着火時刻と燃焼時間を変え、輪郭を崩す
			for (int i = 0; i < 17; ++i) {
				const float delay=i==0 ? 0 : (i%5)*0.025f;
				const float age=t-delay, life=0.48f+(i%5)*0.095f;
				if (age<0 || age>=life) continue;
				const float heat=1-age/life, phase=i*2.399963f;
				const float spread=size*(0.15f+age*1.15f)*(i==0 ? 0 : 0.9f);
				const auto location=origin+drift+XMVectorSet(std::cos(phase)*spread,
					std::sin(phase)*spread*0.8f+age*size*0.55f,std::sin(phase*2)*spread*0.4f,0);
				const float radius=size*(0.28f+std::sqrt(age)*0.5f)*(0.8f+(i%3)*0.22f);
				disc(location,radius,{1,0.27f+heat*0.35f,0.025f+heat*0.09f,heat*0.95f},i+27);
			}
			// 白熱はごく短くし、均一な衝撃波の輪は描かない
			if (t<0.09f) {
				const float flash=1-t/0.09f;
				disc(origin,size*(0.65f+t*2),{1,0.85f,0.55f,flash*0.8f});
			}
			// 重力で落下する破片と燃える軌跡
			for (int i=0; i<24; ++i) {
				const float life=1.1f+(i%7)*0.22f;
				if (t >= life) continue;
				const float phase=i*2.399963f, y=std::sin(i*17.31f)*0.85f;
				const float radial=std::sqrt(1-y*y), speed=size*(0.8f+(i%9)*0.21f);
				const auto velocity=XMVectorSet(std::cos(phase)*radial*speed, y*speed, std::sin(phase)*radial*speed, 0);
				const auto positionAt = [&](float age) { return origin + XMLoadFloat3(&effect.velocity)*(age*0.2f) +
					velocity*age + XMVectorSet(0,-18*age*age,0,0); };
				XMFLOAT3 point, tail;
				XMStoreFloat3(&point, positionAt(t)); XMStoreFloat3(&tail, positionAt(std::max(0.0f,t-0.018f)));
				const auto a=ProjectHudPoint(point,view), b=ProjectHudPoint(tail,view);
				if (!a.visible || !b.visible) continue;
				const float fade=1-t/life, scale=view.height/1080.0f;
				if (i%3==0) {
					for (int puff=1; puff<=3; ++puff) {
						const float age=std::max(0.0f,t-puff*0.07f);
						disc(positionAt(age),size*(0.035f+puff*0.014f),{0.12f,0.11f,0.1f,fade*0.5f},i+puff+3);
					}
				}
				list.Line(a.pixel,b.pixel,std::max(1.0f,scale*3),{1,0.25f,0.015f,fade*0.65f});
				list.Line(a.pixel,b.pixel,std::max(0.7f,scale*1.2f),{1,0.85f,0.4f,fade});
			}
		}
	}
	void CombatHudPresenter::Build(const CombatSnapshot& snapshot, const Engine::RenderView& view,
		Engine::HudDrawList& list) const {
		list.Reset(view.width, view.height);
		if (view.width == 0 || view.height == 0) return;
		const float scale = std::min(float(view.width) / 1920, float(view.height) / 1080);
		const float line = std::max(1.0f, m_settings.lineWidth * scale), font = m_settings.fontSize * scale;
		const auto color = m_settings.color;
		for (const auto& explosion : snapshot.explosions) DrawExplosion(explosion, view, list);
		// 入射方向へ跳ね返る金属片と、機体の移動を引き継ぐ着弾煙
		const auto cameraWorld = XMMatrixInverse(nullptr, XMLoadFloat4x4(&view.view));
		for (const auto& impact : snapshot.impacts) {
			if (list.GetVertices().size() + 600 > Engine::HudDrawList::MaxVertices - 6000) break;
			const float t=impact.age;
			if (t<0 || t>=0.45f) continue;
			const auto origin=XMLoadFloat3(&impact.position), inherited=XMLoadFloat3(&impact.velocity);
			const auto normal=-XMVector3Normalize(XMLoadFloat3(&impact.direction));
			const auto reference=std::abs(XMVectorGetY(normal))<0.9f ? XMVectorSet(0,1,0,0) : XMVectorSet(1,0,0,0);
			const auto tangent=XMVector3Normalize(XMVector3Cross(normal,reference));
			const auto bitangent=XMVector3Cross(normal,tangent);
			const float seed=impact.position.x*0.73f+impact.position.y*1.37f+impact.position.z*0.19f;
			const auto random=[&](int index) {
				const float value=std::sin(seed+index*127.1f)*43758.5453f;
				return value-std::floor(value);
			};
			const auto puff=[&](XMVECTOR location,float radius,XMFLOAT4 tint,unsigned index,bool textured) {
				XMFLOAT3 point,edge;
				XMStoreFloat3(&point,location); XMStoreFloat3(&edge,location+cameraWorld.r[0]*radius);
				const auto a=ProjectHudPoint(point,view),b=ProjectHudPoint(edge,view);
				if (!a.visible || b.behind) return;
				const float pixels=std::min(std::hypot(b.pixel.x-a.pixel.x,b.pixel.y-a.pixel.y),float(view.height));
				if (textured) list.CloudPuff(a.pixel,pixels,random(index)*XM_2PI+t*0.7f,index,tint);
				else list.SoftDisc(a.pixel,pixels,tint);
			};
			// 小さな灰色の煙を残し、火球にはしない
			for (unsigned i=0; i<8; ++i) {
				const auto offset=tangent*(random(i+2)-0.5f)*t*9 + bitangent*(random(i+12)-0.5f)*t*9;
				const auto location=origin+inherited*(0.45f*t)+normal*(1+t*5)+offset+XMVectorSet(0,t*2,0,0);
				const float alpha=std::min(t*24,0.7f)*std::clamp((0.45f-t)/0.22f,0.0f,1.0f);
				puff(location,2.0f+t*(9+random(i+22)*7),{0.36f,0.34f,0.3f,alpha},i+7,true);
			}
			if (t<0.1f) {
				const float flash=1-t/0.1f;
				puff(origin+inherited*t,8.0f,{1,0.55f,0.15f,flash*0.45f},0,false);
				puff(origin+inherited*t,4.8f,{1,0.95f,0.75f,flash},0,false);
			}
			for (int i=0; i<28; ++i) {
				const float life=0.16f+random(i+32)*0.28f;
				if (t>=life) continue;
				const float angle=random(i+52)*XM_2PI, outward=0.2f+random(i+72)*0.8f;
				const auto direction=XMVector3Normalize(normal*outward + tangent*std::cos(angle) + bitangent*std::sin(angle));
				const float speed=24+random(i+92)*60;
				const auto positionAt=[&](float age) {
					return origin+inherited*(age*0.7f)+direction*(speed*(1-std::exp(-age*3))/3)+XMVectorSet(0,-9.8f*age*age,0,0);
				};
				XMFLOAT3 head,tail;
				XMStoreFloat3(&head,positionAt(t)); XMStoreFloat3(&tail,positionAt(std::max(0.0f,t-(0.012f+random(i+112)*0.024f))));
				const auto a=ProjectHudPoint(head,view),b=ProjectHudPoint(tail,view);
				if (!a.visible || !b.visible) continue;
				const float heat=1-t/life;
				const float width=std::max(1.1f,scale*(1.8f+random(i+132)*2.2f));
				list.Line(a.pixel,b.pixel,width*2,{1,0.38f,0.06f,heat*0.3f});
				list.Line(a.pixel,b.pixel,width,{1,0.4f+heat*0.55f,0.12f+heat*0.55f,std::sqrt(heat)});
			}
		}
		const float ox = (view.width - 1920 * scale) * 0.5f, oy = (view.height - 1080 * scale) * 0.5f;
		const auto label = [&](float x, float y, std::string_view text) { list.Text({ ox + x * scale,oy + y * scale }, text, font, color); };
		// The boresight follows the gun axis independently of the predicted bullet path.
		XMFLOAT3 sight;
		XMStoreFloat3(&sight, XMLoadFloat3(&snapshot.muzzle) + XMLoadFloat3(&snapshot.forward) * CombatSnapshot::gunSightDistance);
		const auto cross = ProjectHudPoint(sight, view);
		if (cross.visible) {
			list.Line({ cross.pixel.x - 12 * scale,cross.pixel.y }, { cross.pixel.x + 12 * scale,cross.pixel.y }, line, color);
			list.Line({ cross.pixel.x,cross.pixel.y - 12 * scale }, { cross.pixel.x,cross.pixel.y + 12 * scale }, line, color);
		}
		// Lead marker: aim the boresight at this point to intercept the selected target.
		// aimPoint includes target motion and compensates for inherited aircraft velocity.
		const auto circlePoint = snapshot.lockState == LockState::Locked &&
			snapshot.selected != Engine::InvalidObjectId && snapshot.aim
			? std::optional<XMFLOAT3>{snapshot.aim->aimPoint} : std::nullopt;
		if (circlePoint) {
			const auto impact = ProjectHudPoint(*circlePoint, view);
			if (impact.visible) {
				const auto p = impact.pixel;
				const XMFLOAT4 sightColor{ 0.65f,1.0f,0.65f,color.w };
				list.Circle(p, 34 * scale, 1, line, sightColor);
				list.Circle(p, 25 * scale, 1, line, sightColor);
				list.Circle(p, 5 * scale, 1, line, sightColor);
				for (int i = 0; i < 12; ++i) {
					const float angle = XM_2PI * i / 12;
					const float x = std::cos(angle), y = std::sin(angle);
					list.Line({ p.x + x * 31 * scale,p.y + y * 31 * scale },
						{ p.x + x * (i % 3 == 0 ? 40 : 37) * scale,p.y + y * (i % 3 == 0 ? 40 : 37) * scale }, line, sightColor);
				}
				for (const XMFLOAT2 axis : {XMFLOAT2{ 1,0 }, XMFLOAT2{ -1,0 }, XMFLOAT2{ 0,1 }, XMFLOAT2{ 0,-1 }})
					list.Line({ p.x + axis.x * 10 * scale,p.y + axis.y * 10 * scale },
						{ p.x + axis.x * 23 * scale,p.y + axis.y * 23 * scale }, line, sightColor);
			}
		}
		for (const auto& enemy : snapshot.enemies) {
			const auto projected = ProjectHudPoint(enemy.center, view);
			const bool selected = enemy.id == snapshot.selected;
			if (projected.visible) {
				const auto p = projected.pixel;
				const float radius = (selected ? 32.0f : 22.0f) * scale, corner = radius * 0.4f;
				for (float x : {-1.0f, 1.0f}) for (float y : {-1.0f, 1.0f}) {
					const XMFLOAT2 a{ p.x + x * radius,p.y + y * radius };
					list.Line(a, { a.x - x * corner,a.y }, line, color); list.Line(a, { a.x,a.y - y * corner }, line, color);
				}
				if (selected) {
					const auto distance = int(XMVectorGetX(XMVector3Length(XMLoadFloat3(&enemy.center) - XMLoadFloat3(&snapshot.muzzle))));
					list.Text({ std::clamp(p.x + radius + 8 * scale,0.0f,std::max(0.0f,view.width - 260 * scale)),
						std::clamp(p.y + radius + 8 * scale,0.0f,std::max(0.0f,view.height - 2 * font)) }, enemy.name + "  " + std::to_string(distance), font, color);
					if (snapshot.lockState == LockState::Acquiring) list.Circle(p, radius + 8 * scale, snapshot.acquireProgress, line, color);
					if (snapshot.lockState == LockState::Locked) list.Text({ p.x - radius,std::max(0.0f,p.y - radius - font - 4 * scale) }, "LOCK", font, color);
				}
			}
			else if (selected) {
				const float cx = view.width * 0.5f, cy = view.height * 0.5f;
				float dx = projected.pixel.x - cx, dy = projected.pixel.y - cy;
				if (std::hypot(dx, dy) < 0.001f) dy = 1;
				const float ratio = 1 / std::max(std::abs(dx) / std::max(1.0f, cx - 50 * scale), std::abs(dy) / std::max(1.0f, cy - 50 * scale));
				const XMFLOAT2 tip{ cx + dx * ratio,cy + dy * ratio };
				const float length = std::hypot(dx, dy), ux = dx / length, uy = dy / length;
				list.Line(tip, { tip.x - ux * 20 * scale - uy * 9 * scale,tip.y - uy * 20 * scale + ux * 9 * scale }, line, color);
				list.Line(tip, { tip.x - ux * 20 * scale + uy * 9 * scale,tip.y - uy * 20 * scale - ux * 9 * scale }, line, color);
			}

		}
		if (snapshot.selected == Engine::InvalidObjectId) label(80, 80, "NO TARGET");
		else if (!snapshot.aim) label(80, 80, "OUT OF RANGE");
		if (snapshot.destroyedSeconds > 0) label(800, 720, "DESTROYED");
		else if (snapshot.hitSeconds > 0) label(920, 640, "HIT");
		if (snapshot.allDownSeconds > 0) label(760, 180, "ALL TARGETS DOWN");
	}
}
