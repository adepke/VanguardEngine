// Copyright (c) 2019-2021 Andrew Depke

#include <Editor/Editor.h>

#if ENABLE_EDITOR
#include <Editor/EditorUI.h>
#include <Rendering/RenderGraph.h>
#include <Rendering/RenderPass.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Rendering/ClusteredLightCulling.h>
#endif

Editor::Editor()
{
#if ENABLE_EDITOR
	ui = std::make_unique<EditorUI>();
#endif
}

Editor::~Editor()
{
	// Destroys the UI.
}

void Editor::Render(RenderGraph& graph, RenderDevice& device, Renderer& renderer, entt::registry& registry, RenderResource cameraBuffer, RenderResource depthStencil,
	RenderResource outputLDR, RenderResource backBuffer, const ClusterResources& clusterResources)
{
#if ENABLE_EDITOR
	// Render the active overlay if there is one.
	RenderResource activeOverlayTag{};
	switch (ui->activeOverlay)
	{
	case RenderOverlay::Clusters: activeOverlayTag = renderer.clusteredCulling.RenderDebugOverlay(graph, clusterResources.lightInfo, clusterResources.lightVisibility); break;
	default: break;
	}

	auto& editorPass = graph.AddPass("Editor Pass", ExecutionQueue::Graphics);
	editorPass.Read(cameraBuffer, ResourceBind::CBV);
	editorPass.Read(depthStencil, ResourceBind::SRV);
	editorPass.Read(outputLDR, ResourceBind::SRV);
	if (ui->activeOverlay != RenderOverlay::None)
	{
		editorPass.Read(activeOverlayTag, ResourceBind::SRV);
	}
	editorPass.Output(backBuffer, OutputBind::RTV, false);
	editorPass.Bind([&, cameraBuffer, depthStencil, outputLDR, activeOverlayTag](CommandList& list, RenderGraphResourceManager& resources)
	{
		renderer.userInterface->NewFrame();

		ui->DrawLayout();
		ui->DrawDemoWindow();
		ui->DrawScene(&device, registry, resources.GetTexture(outputLDR));
		ui->DrawEntityHierarchy(registry);
		ui->DrawEntityPropertyViewer(registry);
		ui->DrawPerformanceMetrics(renderer.lastFrameTime);
		ui->DrawRenderGraph(&device, resources.GetTexture(depthStencil), resources.GetTexture(outputLDR));
		ui->DrawAtmosphereControls(renderer.atmosphere);
		ui->DrawRenderVisualizer(&device, renderer.clusteredCulling, ui->activeOverlay != RenderOverlay::None ? resources.GetTexture(activeOverlayTag) : TextureHandle{});

		renderer.userInterface->Render(list, resources.GetBuffer(cameraBuffer));
	});
#endif
}