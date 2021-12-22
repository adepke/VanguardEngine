// Copyright (c) 2019-2021 Andrew Depke

#pragma once

#include <Rendering/Base.h>
#include <Utility/Singleton.h>
#include <Window/WindowFrame.h>
#include <Rendering/Device.h>
#include <Rendering/MeshFactory.h>
#include <Rendering/ResourceHandle.h>
#include <Rendering/PipelineBuilder.h>
#include <Rendering/Material.h>
#include <Rendering/UserInterface.h>
#include <Rendering/DescriptorAllocator.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Rendering/Atmosphere.h>
#include <Rendering/ClusteredLightCulling.h>
#include <Rendering/ImageBasedLighting.h>

#include <entt/entt.hpp>

class CommandList;

class Renderer : public Singleton<Renderer>
{
public:
	std::unique_ptr<WindowFrame> window;
	std::unique_ptr<RenderDevice> device;  // Destruct the device after all other resources.
	std::unique_ptr<MeshFactory> meshFactory;

private:
	RenderGraphResourceManager renderGraphResources;

	PipelineBuilder pipelines;  // Manages all the pipelines.

	BufferHandle cameraBuffer;

	DescriptorHandle nullDescriptor;

public:
	float lastFrameTime = -1.f;

public:
	std::unique_ptr<UserInterfaceManager> userInterface;
	Atmosphere atmosphere;
	ClusteredLightCulling clusteredCulling;
	ImageBasedLighting ibl;

	BufferHandle instanceBuffer;
	size_t instanceOffset = 0;

private:
	void UpdateCameraBuffer(const entt::registry& registry);
	void CreatePipelines();
	std::pair<BufferHandle, size_t> CreateInstanceBuffer(const entt::registry& registry);
	BufferHandle CreateLightBuffer(const entt::registry& registry);

public:
	~Renderer();

	void Initialize(std::unique_ptr<WindowFrame>&& inWindow, std::unique_ptr<RenderDevice>&& inDevice, entt::registry& registry);

	// Entity data is safe to write to immediately after this function returns. Do not attempt to write before Render() returns.
	void Render(entt::registry& registry);

	void SubmitFrameTime(uint32_t timeUs);

	std::pair<uint32_t, uint32_t> GetResolution() const;
	void SetResolution(uint32_t width, uint32_t height, bool fullscreen);
};

inline void Renderer::SubmitFrameTime(uint32_t timeUs)
{
	lastFrameTime = static_cast<float>(timeUs);
}