// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Rendering/RenderGraphResource.h>
#include <Rendering/ResourceHandle.h>
#include <Utility/Singleton.h>

#include <vector>

class RenderDevice;
class RenderGraph;

// GPU shape structs.
struct DebugCubeInstance
{
	XMMATRIX transform;  // Applied to a unit cube spanning [-0.5, 0.5].
	XMFLOAT4 color;
};

struct DebugSphereInstance
{
	XMFLOAT3 center;
	float radius;
	XMFLOAT4 color;
};

// Immediate-mode debug rendering, primarily for shapes. Drawing API can be used at any
// point during the frame.
class DebugDraw : public Singleton<DebugDraw>
{
private:
	// #TODO: ugly, improve this.
	enum DepthMode
	{
		DepthTested = 0,  // Occluded against scene geometry.
		AlwaysOnTop = 1,  // Ignore depth, always visible.
		ModeCount = 2
	};

	RenderDevice* device = nullptr;

	// #TODO: don't separate by mode.
	std::vector<DebugCubeInstance> cubes[ModeCount];
	std::vector<DebugSphereInstance> spheres[ModeCount];

public:
	void Initialize(RenderDevice* inDevice);

	void DrawCube(const XMMATRIX& transform, const XMFLOAT4& color, bool depthTest = true);
	void DrawSphere(const XMFLOAT3& center, float radius, const XMFLOAT4& color, bool depthTest = true);
	// #TODO: Add API for drawing froxels.

	void Render(RenderGraph& graph, RenderResource cameraBuffer, RenderResource depthStencil, RenderResource outputTarget);
};

namespace Draw
{
	inline void Cube(const XMMATRIX& transform, const XMFLOAT4& color, bool depthTest = true)
	{
		DebugDraw::Get().DrawCube(transform, color, depthTest);
	}

	inline void Cube(const XMFLOAT3& center, const XMFLOAT3& extent, const XMFLOAT4& color, bool depthTest = true)
	{
		const auto transform = XMMatrixScaling(extent.x, extent.y, extent.z) * XMMatrixTranslation(center.x, center.y, center.z);
		DebugDraw::Get().DrawCube(transform, color, depthTest);
	}

	inline void Sphere(const XMFLOAT3& center, float radius, const XMFLOAT4& color, bool depthTest = true)
	{
		DebugDraw::Get().DrawSphere(center, radius, color, depthTest);
	}
}
