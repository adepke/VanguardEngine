// Copyright (c) 2019-2022 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Utility/Singleton.h>
#include <Window/WindowFrame.h>
#include <Rendering/Device.h>
#include <Rendering/MeshFactory.h>
#include <Rendering/MaterialFactory.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/UserInterface.h>
#include <Rendering/DescriptorAllocator.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/RenderPipeline.h>
#include <Rendering/Atmosphere.h>
#include <Rendering/ClusteredLightCulling.h>
#include <Rendering/ImageBasedLighting.h>
#include <Rendering/Bloom.h>
#include <Rendering/OcclusionCulling.h>

#include <entt/entt.hpp>

struct MeshRenderable
{
	uint32_t positionOffset;
	uint32_t extraOffset;
	uint32_t indexOffset;
	uint32_t indexCount;
	uint32_t materialIndex;
	uint32_t batchId;
	float boundingSphereRadius;
};

class CommandList;

class Renderer : public Singleton<Renderer>
{
public:
	std::unique_ptr<WindowFrame> window;
	std::unique_ptr<RenderDevice> device;  // Destruct the device after all other resources.
	std::unique_ptr<MeshFactory> meshFactory;
	std::unique_ptr<MaterialFactory> materialFactory;

	float lastFrameTime = -1.f;
	std::unique_ptr<UserInterfaceManager> userInterface;
	Atmosphere atmosphere;
	ClusteredLightCulling clusteredCulling;
	ImageBasedLighting ibl;
	Bloom bloom;
	OcclusionCulling occlusionCulling;

	size_t renderableCount;

	ResourcePtr<ID3D12RootSignature> rootSignature;
	ResourcePtr<ID3D12CommandSignature> meshIndirectCommandSignature;

private:
	RenderGraphResourceManager renderGraphResources;

	BufferHandle instanceBuffer;
	BufferHandle cameraBuffer;

	RenderPipelineLayout meshCullLayout;
	RenderPipelineLayout prepassLayout;
	RenderPipelineLayout forwardOpaqueLayout;
	RenderPipelineLayout postProcessLayout;

	BufferHandle meshIndirectRenderArgs;

	bool cameraFrozen = false;
	XMMATRIX frozenView;
	XMMATRIX frozenProjection;

	bool shouldReloadShaders = false;

private:
	void CreateRootSignature();
	std::vector<MeshRenderable> UpdateObjects(const entt::registry& registry);
	void UpdateCameraBuffer(const entt::registry& registry);
	void CreatePipelines();
	BufferHandle CreateLightBuffer(const entt::registry& registry);

public:
	~Renderer();

	void Initialize(std::unique_ptr<WindowFrame>&& inWindow, std::unique_ptr<RenderDevice>&& inDevice, entt::registry& registry);

	// Entity data is safe to write to immediately after this function returns. Do not attempt to write before Render() returns.
	void Render(entt::registry& registry);

	void SubmitFrameTime(uint32_t timeUs);

	std::pair<uint32_t, uint32_t> GetResolution() const;
	void SetResolution(uint32_t width, uint32_t height, bool fullscreen);

	void FreezeCamera();
	void ReloadShaderPipelines();
};

inline void Renderer::SubmitFrameTime(uint32_t timeUs)
{
	lastFrameTime = static_cast<float>(timeUs);
}