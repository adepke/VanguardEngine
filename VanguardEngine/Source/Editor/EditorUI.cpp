// Copyright (c) 2019-2022 Andrew Depke

#include <Editor/EditorUI.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Editor/EntityReflection.h>
#include <Editor/ImGuiExtensions.h>
#include <Rendering/Atmosphere.h>
#include <Rendering/Bloom.h>
#include <Rendering/ClusteredLightCulling.h>

#include <imgui_internal.h>

#include <algorithm>
#include <numeric>

void EditorUI::DrawMenu()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Entity Hierarchy", nullptr, &entityHierarchyOpen);
			ImGui::MenuItem("Entity Properties", nullptr, &entityPropertyViewerOpen);
			ImGui::MenuItem("Performance Metrics", nullptr, &performanceMetricsOpen);
			ImGui::MenuItem("Render Graph", nullptr, &renderGraphOpen);
			ImGui::MenuItem("Atmosphere Controls", nullptr, &atmosphereControlsOpen);
			ImGui::MenuItem("Bloom Controls", nullptr, &bloomControlsOpen);
			ImGui::MenuItem("Render Visualizer", nullptr, &renderVisualizerOpen);

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Window"))
		{
			ImGui::MenuItem("Fullscreen", nullptr, &fullscreen);

			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void EditorUI::DrawFrameTimeHistory()
{
	if (ImGui::Begin("Performance Metrics", &performanceMetricsOpen))
	{
		// Compute statistics.
		const auto [min, max] = std::minmax_element(frameTimes.begin(), frameTimes.end());
		const auto mean = std::accumulate(frameTimes.begin(), frameTimes.end(), 0) / static_cast<float>(frameTimes.size());

		auto* window = ImGui::GetCurrentWindow();
		auto& style = ImGui::GetStyle();

		const auto frameWidth = ImGui::GetContentRegionAvailWidth() - window->WindowPadding.x - ImGui::CalcTextSize("Mean: 00.000").x;
		const auto frameHeight = (ImGui::GetTextLineHeight() + style.ItemSpacing.y) * 3.f + 10.f;  // Max, mean, min.

		const ImRect frameBoundingBox = { window->DC.CursorPos, window->DC.CursorPos + ImVec2{ frameWidth, frameHeight } };

		ImGui::ItemSize(frameBoundingBox, style.FramePadding.y);
		if (!ImGui::ItemAdd(frameBoundingBox, 0))  // Don't support navigation to the frame.
		{
			ImGui::End();
			return;
		}

		ImGui::RenderFrame(frameBoundingBox.Min, frameBoundingBox.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

		// Internal region for rendering the plot lines.
		const ImRect frameRenderSpace = { frameBoundingBox.Min + style.FramePadding, frameBoundingBox.Max - style.FramePadding };

		// Adaptively update the sample count.
		frameTimeHistoryCount = frameRenderSpace.GetWidth() / 2.f;

		if (frameTimes.size() > 1)
		{
			const ImVec2 lineSize = { frameRenderSpace.GetWidth() / (frameTimes.size() - 1), frameRenderSpace.GetHeight() / (*max - *min) };
			const auto lineColor = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_PlotLines]);

			for (int i = 0; i < frameTimes.size() - 1; ++i)  // Don't draw the final point.
			{
				window->DrawList->AddLine(
					{ frameRenderSpace.Min.x + (lineSize.x * i), frameRenderSpace.Min.y + frameRenderSpace.GetHeight() - (lineSize.y * (frameTimes[i] - *min)) },
					{ frameRenderSpace.Min.x + (lineSize.x * (i + 1)), frameRenderSpace.Min.y + frameRenderSpace.GetHeight() - (lineSize.y * (frameTimes[i + 1] - *min)) },
					lineColor);
			}
		}

		if (min != frameTimes.end() && max != frameTimes.end())
		{
			ImGui::SameLine();
			ImGui::BeginGroup();

			ImGui::Text("Max:  %.3f", *max / 1000.f);
			ImGui::Text("Mean: %.3f", mean / 1000.f);
			ImGui::Text("Min:  %.3f", *min / 1000.f);

			ImGui::EndGroup();
		}
	}

	ImGui::End();
}

void EditorUI::Update()
{
	if (fullscreen != Renderer::Get().window->IsFullscreen())
	{
		const auto [width, height] = Renderer::Get().GetResolution();
		Renderer::Get().window->SetSize(width, height, fullscreen);
	}
}

void EditorUI::DrawLayout()
{
	auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->GetWorkPos());
	ImGui::SetNextWindowSize(viewport->GetWorkSize());
	ImGui::SetNextWindowViewport(viewport->ID);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });

	// Always draw the dock space.
	ImGui::Begin("Dock Space", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking);

	ImGui::PopStyleVar(3);

	const auto dockSpaceId = ImGui::GetID("DockSpace");
	ImGui::DockSpace(dockSpaceId, { 0.f, 0.f });

	// Draw the menu in the dock space window.
	DrawMenu();

	ImGui::End();
}

void EditorUI::DrawDemoWindow()
{
	static bool demoWindowOpen = true;

	ImGui::ShowDemoWindow(&demoWindowOpen);
}

void EditorUI::DrawScene(RenderDevice* device, entt::registry& registry, TextureHandle sceneTexture)
{
	const auto& sceneDescription = device->GetResourceManager().Get(sceneTexture).description;

	ImGui::SetNextWindowSizeConstraints({ 100.f, 100.f }, { (float)sceneDescription.width, (float)sceneDescription.height });

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });  // Remove window padding.

	if (ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse))
	{
		const auto viewportMin = ImGui::GetWindowContentRegionMin();
		const auto viewportMax = ImGui::GetWindowContentRegionMax();
		const auto viewportSize = viewportMax - viewportMin;
		const auto widthUV = (1.f - (viewportSize.x / sceneDescription.width)) * 0.5f;
		const auto heightUV = (1.f - (viewportSize.y / sceneDescription.height)) * 0.5f;

		sceneWidthUV = widthUV;
		sceneHeightUV = heightUV;
		sceneViewportMin = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMin();
		sceneViewportMax = ImGui::GetWindowPos() + ImGui::GetWindowContentRegionMax();

		ImGui::Image(device, sceneTexture, { 1.f, 1.f }, { widthUV, heightUV }, { 1.f + widthUV, 1.f + heightUV });

		// Double clicking the viewport grants control.
		if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
		{
			// #TODO: Grant control to only the camera that the viewport is linked to, not every camera-owning entity.
			registry.view<const CameraComponent>().each([&](auto entity, const auto&)
			{
				if (!registry.all_of<ControlComponent>(entity))
				{
					registry.emplace<ControlComponent>(entity);
				}
			});
		}

		// Use a dummy object to get proper drag drop bounds.
		const float padding = 4.f;
		ImGui::SetCursorPos(ImGui::GetWindowContentRegionMin() + ImVec2{ padding, padding });
		ImGui::Dummy(ImGui::GetWindowContentRegionMax() - ImGui::GetWindowContentRegionMin() - ImVec2{ padding * 2.f, padding * 2.f });

		if (ImGui::BeginDragDropTarget())
		{
			if (const auto* payload = ImGui::AcceptDragDropPayload("RenderOverlay", ImGuiDragDropFlags_None))
			{
				renderOverlayOnScene = true;
			}

			ImGui::EndDragDropTarget();
		}
	}

	ImGui::End();

	ImGui::PopStyleVar();
}

void EditorUI::DrawEntityHierarchy(entt::registry& registry)
{
	if (entityHierarchyOpen)
	{
		entt::entity selectedEntity = entt::null;

		if (ImGui::Begin("Entity Hierarchy", &entityHierarchyOpen))
		{
			ImGui::Text("%i Entities", registry.size());
			ImGui::Separator();

			registry.each([this, &registry, &selectedEntity](auto entity)
			{
				ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_None;

				if (entity == hierarchySelectedEntity)
					nodeFlags |= ImGuiTreeNodeFlags_Selected;

				bool nodeOpen = false;

				ImGui::PushID(static_cast<int32_t>(entity));  // Use the entity as the ID.

				if (registry.all_of<NameComponent>(entity))
				{
					nodeOpen = ImGui::TreeNodeEx("EntityTreeNode", nodeFlags, registry.get<NameComponent>(entity).name.c_str());
				}

				else
				{
					// Strip the version info from the entity, we only care about the actual ID.
					nodeOpen = ImGui::TreeNodeEx("EntityTreeNode", nodeFlags, "Entity_%i", registry.entity(entity));
				}

				if (ImGui::IsItemClicked())
				{
					selectedEntity = entity;
				}

				if (nodeOpen)
				{
					// #TODO: Draw entity children.

					ImGui::TreePop();
				}

				ImGui::PopID();

				// Open the property viewer with focus on left click. Test the condition for each tree node.
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered(ImGuiHoveredFlags_None))
				{
					entityPropertyViewerOpen = true;
					entityPropertyViewerFocus = true;
				}
			});
		}

		ImGui::End();

		// Check if it's valid first, otherwise deselecting will remove the property viewer.
		if (registry.valid(selectedEntity))
		{
			hierarchySelectedEntity = selectedEntity;
		}
	}
}

void EditorUI::DrawEntityPropertyViewer(entt::registry& registry)
{
	if (entityPropertyViewerOpen)
	{
		if (entityPropertyViewerFocus)
		{
			entityPropertyViewerFocus = false;
			ImGui::SetNextWindowFocus();
		}

		if (ImGui::Begin("Property Viewer", &entityPropertyViewerOpen))
		{
			if (registry.valid(hierarchySelectedEntity))
			{
				uint32_t componentCount = 0;

				for (auto& [metaID, renderFunction] : EntityReflection::componentList)
				{
					entt::id_type metaList[] = { metaID };

					if (registry.runtime_view(std::cbegin(metaList), std::cend(metaList)).contains(hierarchySelectedEntity))
					{
						++componentCount;

						renderFunction(registry, hierarchySelectedEntity);

						ImGui::Separator();
					}
				}

				if (componentCount == 0)
				{
					ImGui::Text("No components.");
				}
			}
		}

		ImGui::End();
	}
}

void EditorUI::DrawPerformanceMetrics(float frameTimeMs)
{
	frameTimes.push_back(frameTimeMs);

	while (frameTimes.size() > frameTimeHistoryCount)
	{
		frameTimes.pop_front();
	}

	if (performanceMetricsOpen)
	{
		DrawFrameTimeHistory();
	}
}

void EditorUI::DrawRenderGraph(RenderDevice* device, TextureHandle depthStencil, TextureHandle scene)
{
	if (renderGraphOpen)
	{
		if (ImGui::Begin("Render Graph", &renderGraphOpen))
		{
			ImGui::Checkbox("Linearize depth", &linearizeDepth);

			if (linearizeDepth)
			{
				ImGui::GetWindowDrawList()->AddCallback([](auto* list)
				{
					list->BindConstants("drawData", { 1 });
				}, nullptr);
			}

			ImGui::Image(device, depthStencil, { 0.25f, 0.25f });

			if (linearizeDepth)
			{
				ImGui::GetWindowDrawList()->AddCallback([](auto* list)
				{
					list->BindConstants("drawData", { 0 });
				}, nullptr);
			}

			ImGui::Image(device, scene, { 0.25f, 0.25f });
		}

		ImGui::End();
	}
}

void EditorUI::DrawAtmosphereControls(Atmosphere& atmosphere)
{
	if (atmosphereControlsOpen)
	{
		if (ImGui::Begin("Atmosphere", &atmosphereControlsOpen))
		{
			constexpr float maxZenithAngle = 3.14159f;
			ImGui::DragFloat("Solar zenith angle", &atmosphere.solarZenithAngle, 0.01f, -maxZenithAngle, maxZenithAngle, "%.3f");

			ImGui::Separator();

			bool dirty = false;
			dirty |= ImGui::DragFloat("Bottom radius", &atmosphere.model.radiusBottom, 0.2f, 1.f, atmosphere.model.radiusTop, "%.3f");
			dirty |= ImGui::DragFloat("Top radius", &atmosphere.model.radiusTop, 0.2f, atmosphere.model.radiusBottom, 10000.f, "%.3f");
			dirty |= ImGui::DragFloat3("Rayleigh scattering", (float*)&atmosphere.model.rayleighScattering, 0.001f, 0.f, 1.f, "%.6f");
			dirty |= ImGui::DragFloat3("Mie scattering", (float*)&atmosphere.model.mieScattering, 0.001f, 0.f, 1.f, "%.6f");
			dirty |= ImGui::DragFloat3("Mie extinction", (float*)&atmosphere.model.mieExtinction, 0.001f, 0.f, 1.f, "%.6f");
			dirty |= ImGui::DragFloat3("Absorption extinction", (float*)&atmosphere.model.absorptionExtinction, 0.001f, 0.f, 1.f, "%.6f");
			dirty |= ImGui::DragFloat3("Surface color", (float*)&atmosphere.model.surfaceColor, 0.01f, 0.f, 1.f, "%.3f");
			dirty |= ImGui::DragFloat3("Solar irradiance", (float*)&atmosphere.model.solarIrradiance, 0.01f, 0.f, 100.f, "%.4f");
			
			if (dirty)
			{
				atmosphere.MarkModelDirty();
			}
		}

		ImGui::End();
	}
}

void EditorUI::DrawBloomControls(Bloom& bloom)
{
	if (bloomControlsOpen)
	{
		if (ImGui::Begin("Bloom", &bloomControlsOpen))
		{
			ImGui::DragFloat("Intensity", &bloom.intensity, 0.01f, 0.f, 1.f, "%.2f");
			ImGui::DragFloat("Internal blend", &bloom.internalBlend, 0.01f, 0.f, 1.f, "%.2f");
		}

		ImGui::End();
	}
}

void EditorUI::DrawRenderVisualizer(RenderDevice* device, ClusteredLightCulling& clusteredCulling, TextureHandle overlay)
{
	if (renderVisualizerOpen)
	{
		if (ImGui::Begin("Render Visualizer", &renderVisualizerOpen))
		{
			ImGui::Combo("Active overlay", (int*)&activeOverlay, [](void*, int index, const char** output)
			{
				auto overlay = (RenderOverlay)index;

				switch (overlay)
				{
				case RenderOverlay::None: *output = "None"; break;
				case RenderOverlay::Clusters: *output = "Clusters"; break;
				default: return false;
				}

				return true;
			}, nullptr, 2);  // Note: Make sure to update the hardcoded count when new overlays are added.

			ImGui::Separator();

			if (activeOverlay != RenderOverlay::None)
			{
				if (!renderOverlayOnScene)
				{
					ImGui::ImageButton(device, overlay, { 0.25f, 0.25f });

					if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
					{
						ImGui::SetDragDropPayload("RenderOverlay", nullptr, 0);

						ImGui::ImageButton(device, overlay, { 0.1f, 0.1f }, { 0.f, 0.f }, { 1.f, 1.f }, { 1.f, 1.f, 1.f, 0.5f });

						ImGui::EndDragDropSource();
					}
				}

				else
				{
					ImGui::Text("Overlay enabled.");
				}
			}

			else
			{
				ImGui::Text("No active overlay.");
			}
		}

		ImGui::End();
	}

	// Don't bound scene overlay rendering by the visibility of the render visualization window.
	if (activeOverlay != RenderOverlay::None && renderOverlayOnScene)
	{
		const auto proxyWindowFlags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoDocking;

		ImGui::SetNextWindowPos(sceneViewportMin);
		ImGui::SetNextWindowSize(sceneViewportMax - sceneViewportMin);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.f, 0.f });
		if (ImGui::Begin("Render Overlay Proxy", nullptr, proxyWindowFlags))
		{
			ImGui::Image(device, overlay, { 1.f, 1.f }, { sceneWidthUV, sceneHeightUV }, { 1.f + sceneWidthUV, 1.f + sceneHeightUV }, { 1.f, 1.f, 1.f, 0.25f });

			// Any additional overlay-specific tooling.
			switch (activeOverlay)
			{
			case RenderOverlay::Clusters:
			{
				// Render a color scale.

				const auto sceneViewportSize = sceneViewportMax - sceneViewportMin;
				const auto colorScaleSize = ImVec2{ sceneViewportSize.x * 0.35f, 20.f };
				const auto colorScalePosMin = ImVec2{ sceneViewportMin.x + sceneViewportSize.x * 0.5f - colorScaleSize.x * 0.5f, sceneViewportMax.y - colorScaleSize.y - 40.f };
				auto* drawList = ImGui::GetForegroundDrawList();
				drawList->AddRectFilledMultiColor(colorScalePosMin, colorScalePosMin + colorScaleSize, IM_COL32(0, 255, 0, 255), IM_COL32(255, 0, 0, 255), IM_COL32(255, 0, 0, 255), IM_COL32(0, 255, 0, 255));
				
				const auto frameThickness = 4.f;
				const auto padding = ImVec2{ frameThickness - 1.f, frameThickness - 1.f };
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, frameThickness);
				ImGui::RenderFrameBorder(colorScalePosMin - padding, colorScalePosMin + colorScaleSize + padding);
				ImGui::PopStyleVar();

				const char* titleText = "Cluster froxel bins light count";
				const char* leftText = "0";
				char rightText[8];
				ImFormatString(rightText, std::size(rightText), "%i", clusteredCulling.maxLightsPerFroxel);

				const auto titleSize = ImGui::CalcTextSize(titleText);
				const auto leftSize = ImGui::CalcTextSize(leftText);
				const auto rightSize = ImGui::CalcTextSize(rightText);

				const float textPadding = 8.f;
				ImGui::SetCursorScreenPos({ sceneViewportMin.x + sceneViewportSize.x * 0.5f - titleSize.x * 0.5f, colorScalePosMin.y - titleSize.y * 0.5f - textPadding - 6.f });
				ImGui::Text(titleText);

				ImGui::SetCursorScreenPos({ sceneViewportMin.x + sceneViewportSize.x * 0.5f - colorScaleSize.x * 0.5f - leftSize.x - textPadding, colorScalePosMin.y + colorScaleSize.y * 0.5f - leftSize.y * 0.5f });
				ImGui::Text(leftText);

				ImGui::SetCursorScreenPos({ sceneViewportMin.x + sceneViewportSize.x * 0.5f + colorScaleSize.x * 0.5f + textPadding, colorScalePosMin.y + colorScaleSize.y * 0.5f - rightSize.y * 0.5f });
				ImGui::Text(rightText);

				break;
			}

			default: break;
			}
		}

		ImGui::End();

		ImGui::PopStyleVar();

		// The overlay proxy has no input, so we need a secondary window to remove the overlay from the scene.
		const auto overlayToolsFlags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoDocking;

		const char* buttonText = "Remove render overlay";
		const auto padding = ImGui::GetStyle().WindowPadding + ImGui::GetStyle().FramePadding;
		const auto overlayToolsWindowSize = ImGui::CalcTextSize(buttonText) + padding * 2.f;

		ImGui::SetNextWindowPos(sceneViewportMax - overlayToolsWindowSize - ImVec2{ 20, 20 });
		ImGui::SetNextWindowSize(overlayToolsWindowSize);
		ImGui::SetNextWindowBgAlpha(0.8f);
		if (ImGui::Begin("Render Overlay Tools", nullptr, overlayToolsFlags))
		{
			if (ImGui::Button(buttonText))
			{
				renderOverlayOnScene = false;
			}
		}

		ImGui::End();
	}
}