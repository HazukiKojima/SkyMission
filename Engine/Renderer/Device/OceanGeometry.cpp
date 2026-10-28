#include "OceanGeometry.h"
#include "../../Resources/Buffer/BufferUtilities.h"
#include <cmath>
#include <numbers>
#include <cstdint>
#include <cstring>
#include <vector>

namespace Engine {
	namespace {
		constexpr float kPi = std::numbers::pi_v<float>;
		constexpr int kGridSize = 1000;

		struct OceanVertex {
			float pos[3];
			float uv[2];
		};
	} // namespace

	void OceanGeometry::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commands, float oceanSize) {
		std::vector<OceanVertex> vertices;
		std::vector<uint32_t> indices;

		vertices.reserve(static_cast<size_t>(kGridSize) * kGridSize);
		indices.reserve(static_cast<size_t>(kGridSize - 1) * (kGridSize - 1) * 6);

		for (int z = 0; z < kGridSize; ++z) {
			for (int x = 0; x < kGridSize; ++x) {
				const float px = static_cast<float>(x) / (kGridSize - 1) * oceanSize - oceanSize * 0.5f;
				const float pz = static_cast<float>(z) / (kGridSize - 1) * oceanSize - oceanSize * 0.5f;
				const float u = static_cast<float>(x) / (kGridSize - 1);
				const float v = static_cast<float>(z) / (kGridSize - 1);

				vertices.push_back({{px, 0.0f, pz}, {u, v}});
			}
		}

		for (int z = 0; z < kGridSize - 1; ++z) {
			for (int x = 0; x < kGridSize - 1; ++x) {
				const uint32_t i0 = z * kGridSize + x;
				const uint32_t i1 = i0 + 1;
				const uint32_t i2 = (z + 1) * kGridSize + x;
				const uint32_t i3 = i2 + 1;
				indices.push_back(i0);
				indices.push_back(i1);
				indices.push_back(i2);
				indices.push_back(i1);
				indices.push_back(i3);
				indices.push_back(i2);
			}
		}

		m_indexCount = static_cast<UINT>(indices.size());
		m_vertexBuffer = std::make_unique<VertexBuffer>();
		m_vertexBuffer->Initialize(device, commands, vertices.data(),
								   static_cast<UINT>(sizeof(OceanVertex) * vertices.size()), sizeof(OceanVertex));

		m_indexBuffer = CreateStaticBuffer(device, commands, indices.data(), sizeof(uint32_t) * indices.size(),
										   D3D12_RESOURCE_STATE_INDEX_BUFFER, m_indexUpload);

		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		m_indexBufferView.SizeInBytes = static_cast<UINT>(sizeof(uint32_t) * indices.size());
	}

	void OceanGeometry::ReleaseUploadResources() {
		if (m_vertexBuffer)
			m_vertexBuffer->ReleaseUploadResources();
		m_indexUpload.Reset();
	}

	float OceanGeometry::CalculateWaveHeight(float x, float z, float time, float wavelength, float steepness) {
		const float baseWindX = 1.0f / std::sqrt(1.0f + 0.6f * 0.6f);
		const float baseWindZ = 0.6f / std::sqrt(1.0f + 0.6f * 0.6f);
		float height = 0.0f;

		for (int i = 0; i < 6; ++i) {
			const float phaseOffset = static_cast<float>(i) * 21.53f;
			const float randomAngle = std::sin(static_cast<float>(i) * 7.3f) * 1.1f;
			const float sinAngle = std::sin(randomAngle);
			const float cosAngle = std::cos(randomAngle);
			const float directionX = cosAngle * baseWindX - sinAngle * baseWindZ;
			const float directionZ = sinAngle * baseWindX + cosAngle * baseWindZ;
			const float k = 2.0f * kPi / wavelength;
			const float waveSpeed = std::sqrt(9.8f / k);
			const float phase = k * (directionX * x + directionZ * z - waveSpeed * time * 3.0f) + phaseOffset;

			height += (steepness / k) * std::sin(phase);
			wavelength *= 0.82f;
			steepness *= 0.72f;
		}

		return height;
	}
} // namespace Engine
