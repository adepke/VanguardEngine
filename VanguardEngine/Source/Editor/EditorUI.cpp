// Copyright (c) 2019-2021 Andrew Depke

#include <Editor/EditorUI.h>
#include <Rendering/Device.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Editor/EntityReflection.h>
#include <Editor/ImGuiExtensions.h>
#include <Rendering/Atmosphere.h>

#include <imgui.h>
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

				for (auto& [metaID, renderFunction] : EntityReflection::componentMap)
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