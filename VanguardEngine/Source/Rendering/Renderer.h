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
#include <Rendering/Clouds.h>
#include <Rendering/AccelerationStructures.h>
#include <Rendering/Shadows.h>
#include <Rendering/TextureCapture.h>
#include <Utility/SlotAllocator.h>

#include <entt/entt.hpp>

#include <filesystem>
#include <vector>

struct TransformComponent;
struct MeshComponent;
struct ObjectData;

class CommandList;

class Renderer : public Singleton<Renderer>
{
public:
	std::unique_ptr<WindowFrame> window;
	std::unique_ptr<RenderDevice> device;  // Destruct the device after all other resources.
	std::unique_ptr<MeshFactory> meshFactory;
	std::unique_ptr<MaterialFactory> materialFactory;

	float lastFrameTime = -1.f;
	double appTime = 0;
	uint32_t appFrame = 0;
	std::unique_ptr<UserInterfaceManager> userInterface;
	Atmosphere atmosphere;
	ClusteredLightCulling clusteredCulling;
	ImageBasedLighting ibl;
	Bloom bloom;
	OcclusionCulling occlusionCulling;
	Clouds clouds;
	AccelerationStructures accelerationStructures;
	RayTracedShadows rayTracedShadows;

	size_t renderableCount = 0;

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

	SlotAllocator instanceBufferAllocator;
	std::vector<entt::entity> pendingMeshes;  // Meshes awaiting GPU scene allocation.
	bool drawArgsDirty = false;

	// State for capture requests.
	bool capturePending = false;
	std::filesystem::path capturePath;
	TextureCapture::PendingReadback pendingCapture;
	bool lastCaptureSucceeded = false;

private:
	void CreateRootSignature();

	// ECS hooks, used to manage GPU scene
	void OnMeshConstruct(entt::registry& registry, entt::entity entity);
	void OnMeshDestroy(entt::registry& registry, entt::entity entity);
	void OnSlotDestroy(entt::registry& registry, entt::entity entity);
	void OnTransformDirty(entt::registry& registry, entt::entity entity);

	// Allocates slots for new meshes, rebuilds indirect draw args on structural changes, and
	// uploads instance data for dirty objects only.
	void UpdateGpuScene(entt::registry& registry);

	void UpdateCameraBuffer(const entt::registry& registry);
	void CreatePipelines();
	BufferHandle CreateLightBuffer(const entt::registry& registry);

public:
	~Renderer();

	void Initialize(std::unique_ptr<WindowFrame>&& inWindow, std::unique_ptr<RenderDevice>&& inDevice, entt::registry& registry);

	// Entity data is safe to write to immediately after this function returns. Do not attempt to write before Render() returns.
	void Render(entt::registry& registry);

	void SubmitFrameTime(uint32_t timeUs);
	// Returns app time in seconds.
	double GetAppTime() const;
	// Returns the current frame number.
	uint32_t GetAppFrame() const;

	std::pair<uint32_t, uint32_t> GetResolution() const;
	void SetResolution(uint32_t width, uint32_t height, bool fullscreen);

	void FreezeCamera();
	void ReloadShaderPipelines();
	void ResetAppTime();

	// Called before rendering, requests that the upcoming frame get written to the specified file path.
	void RequestCapture(const std::filesystem::path& path);
	// Checks if the previous capture was successful.
	bool CaptureSucceeded() const noexcept { return lastCaptureSucceeded; }
};

inline void Renderer::SubmitFrameTime(uint32_t timeUs)
{
	lastFrameTime = static_cast<float>(timeUs);
	appTime += lastFrameTime * 0.001 * 0.001;
}