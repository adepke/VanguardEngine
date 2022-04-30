// Copyright (c) 2019-2022 Andrew Depke

#include <Editor/EditorUI.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Rendering/RenderGraphResourceManager.h>
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
#include <string>
#include <sstream>
#include <optional>

void EditorUI::DrawMenu()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Controls", nullptr, &controlsOpen);
			ImGui::MenuItem("Console", "F2", &consoleOpen);
			ImGui::MenuItem("Entity Hierarchy", nullptr, &entityHierarchyOpen);
			ImGui::MenuItem("Entity Properties", nullptr, &entityPropertyViewerOpen);
			ImGui::MenuItem("Metrics", nullptr, &metricsOpen);
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
		return;
	}

	ImGui::RenderFrame(frameBoundingBox.Min, frameBoundingBox.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

	// Internal region for rendering the plot lines.
	const ImRect frameRenderSpace = { frameBoundingBox.Min + style.FramePadding, frameBoundingBox.Max - style.FramePadding };

	// Adaptively update the sample count.
	frameTimeHistoryCount = frameRenderSpace.GetWidth() / 2.f;

	if (frameTimes.size() > 1)
	{
		// Pad out the min/max range.
		const auto range = std::max((*max - *min) + 5.f, 20.f);

		const ImVec2 lineSize = { frameRenderSpace.GetWidth() / (frameTimes.size() - 1), frameRenderSpace.GetHeight() / (range * 2.f) };
		const auto lineColor = ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuiCol_PlotLines]);

		for (int i = 0; i < frameTimes.size() - 1; ++i)  // Don't draw the final point.
		{
			window->DrawList->AddLine(
				{ frameRenderSpace.Min.x + (lineSize.x * i), frameRenderSpace.Min.y + (frameRenderSpace.GetHeight() / 2.f) + (mean - frameTimes[i]) * lineSize.y },
				{ frameRenderSpace.Min.x + (lineSize.x * (i + 1)), frameRenderSpace.Min.y + (frameRenderSpace.GetHeight() / 2.f) + (mean - frameTimes[i + 1]) * lineSize.y },
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

bool EditorUI::ExecuteCommand(const std::string& command)
{
	const auto assignment = command.find('=');
	const auto call = command.find("()");
	if (assignment == std::string::npos && call == std::string::npos)
	{
		return false;
	}

	const auto strip = [](const auto& str)
	{
		std::string result = str;
		const auto start = str.find_first_not_of(' ');
		const auto end = str.find_last_not_of(' ');
		if (end != std::string::npos)
		{
			result.erase(end + 1);
		}
		if (start != std::string::npos)
		{
			result.erase(0, start);
		}

		return result;
	};

	std::string cvar;
	std::string data;
	if (assignment != std::string::npos)
	{
		cvar = strip(command.substr(0, assignment));
		data = strip(command.substr(assignment + 1));
	}
	else
		cvar = strip(command.substr(0, call));

	if (cvar.size() == 0 || (assignment != std::string::npos && data.size() == 0))
	{
		return false;
	}

	std::transform(cvar.begin(), cvar.end(), cvar.begin(), [](auto c)
	{
		return std::tolower(c);
	});

	std::optional<const Cvar*> cvarData;

	// Search for the proper capitalization.
	for (const auto& [key, cvarIt] : CvarManager::Get().cvars)
	{
		auto cvarName = cvarIt.name;
		std::transform(cvarName.begin(), cvarName.end(), cvarName.begin(), [](auto c)
		{
			return std::tolower(c);
		});

		if (cvarName == cvar)
		{
			cvarData = &cvarIt;
			break;
		}
	}

	if (!cvarData)
	{
		return false;
	}

	std::stringstream dataStream;
	dataStream << data;

	switch ((*cvarData)->type)
	{
	case Cvar::CvarType::Int:
	{
		int data;
		dataStream >> data;
		CvarManager::Get().SetVariable(entt::hashed_string::value((*cvarData)->name.c_str(), (*cvarData)->name.size()), data);
		break;
	}
	case Cvar::CvarType::Float:
	{
		float data;
		dataStream >> data;
		CvarManager::Get().SetVariable(entt::hashed_string::value((*cvarData)->name.c_str(), (*cvarData)->name.size()), data);
		break;
	}
	case Cvar::CvarType::Function:
	{
		CvarManager::Get().ExecuteVariable(entt::hashed_string::value((*cvarData)->name.c_str(), (*cvarData)->name.size()));
		break;
	}
	default:
		VGLogError(logEditor, "Attempted to execute cvar command with unknown type {}", (*cvarData)->type);
		return false;
	}

	return true;
}

void EditorUI::DrawConsole(entt::registry& registry, const ImVec2& min, const ImVec2& max)
{
	consoleClosedThisFrame = false;

	auto& io = ImGui::GetIO();
	static bool newPress = true;
	if (io.KeysDown[VK_F2])
	{
		if (newPress)
		{
			consoleClosedThisFrame = consoleOpen;
			consoleOpen = !consoleOpen;
			newPress = false;
		}
	}
	else
	{
		newPress = true;
	}

	if (consoleOpen)
	{
		auto& style = ImGui::GetStyle();
		auto windowMin = min;
		auto windowMax = max;

		// Limit the height.
		constexpr auto heightMax = 220.f;
		const auto height = std::min(max.y - min.y, heightMax);
		windowMax.y = windowMin.y + height;

		constexpr auto frameColor = IM_COL32(20, 20, 20, 238);
		constexpr auto frameColorDark = IM_COL32(20, 20, 20, 245);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, frameColor);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);

		if (ImGui::BeginChildFrame(ImGui::GetID("Console History"), { 0, height }))
		{
			auto* window = ImGui::GetCurrentWindow();
			auto& style = ImGui::GetStyle();

			ImGui::RenderFrame(windowMin, windowMax, frameColor, true);
			
			for (const auto& message : consoleMessages)
			{
				ImGui::Text("%s", message.c_str());
			}

			if (needsScrollUpdate)
			{
				ImGui::SetScrollHereY(1.f);
				needsScrollUpdate = false;
			}

			consoleFullyScrolled = ImGui::GetCursorPosY() - ImGui::GetScrollY() < 240.f;  // Near the bottom, autoscroll.
		}

		ImGui::EndChildFrame();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor();

		const auto inputBoxSize = 25.f;

		static char buffer[256] = { 0 };  // Input box text buffer.

		std::vector<std::pair<const Cvar*, size_t>> cvarMatches;
		if (buffer[0] != '\0')
		{
			std::string bufferStr = buffer;
			std::transform(bufferStr.begin(), bufferStr.end(), bufferStr.begin(), [](auto c)
			{
				return std::tolower(c);
			});

			for (const auto& [key, cvar] : CvarManager::Get().cvars)
			{
				auto cvarName = cvar.name;
				std::transform(cvarName.begin(), cvarName.end(), cvarName.begin(), [](auto c)
				{
					return std::tolower(c);
				});

				if (const auto pos = cvarName.find(bufferStr); pos != std::string::npos)
				{
					cvarMatches.emplace_back(&cvar, pos);
				}
			}
		}

		ImGui::PushStyleColor(ImGuiCol_FrameBg, frameColorDark);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 2, 2 });
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 2, 0 });

		if (ImGui::BeginChildFrame(ImGui::GetID("Console Input"), { 0, inputBoxSize }, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			const auto textBarStart = ImGui::GetCursorPos() + ImGui::GetWindowPos();

			// Draw hint triangle.
			const auto spacing = 6.f;
			const auto offset = 2.f;
			const auto p1 = ImVec2{ textBarStart.x + spacing, textBarStart.y + spacing - offset };
			const auto p2 = ImVec2{ textBarStart.x + spacing, textBarStart.y - spacing + inputBoxSize - offset };
			const auto p3 = ImVec2{ textBarStart.x - spacing + inputBoxSize, textBarStart.y + spacing + (inputBoxSize - 2.f * spacing) * 0.5f - offset };
			ImGui::GetWindowDrawList()->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 255, 255, 245));

			const auto textCallback = [](ImGuiInputTextCallbackData* data) -> int
			{
				switch (data->EventFlag)
				{
				case ImGuiInputTextFlags_CallbackCompletion:
				{
					const char* wordEnd = data->Buf + data->CursorPos;
					const char* wordStart = wordEnd;
					while (wordStart > data->Buf)
					{
						const char c = wordStart[-1];
						if (c == ' ' || c == '\t' || c == ',' || c == ';')
							break;
						--wordStart;
					}

					// Raw matches are all possible, but autocomplete should only factor in matches that are currently equivalent.
					// Exception to this is one raw match with no exact match.
					const auto* rawMatches = (std::vector<std::pair<const Cvar*, size_t>>*)data->UserData;
					std::vector<std::string> matches;
					matches.reserve(rawMatches->size());
					for (const auto& match : *rawMatches)
					{
						if (match.second == 0)
							matches.emplace_back(match.first->name);
					}

					// Autocomplete to partial match.
					if (matches.size() == 0 && rawMatches->size() == 1)
					{
						matches.emplace_back(rawMatches->at(0).first->name);
					}

					if (matches.size() == 1)
					{
						data->DeleteChars((int)(wordStart - data->Buf), (int)(wordEnd - wordStart));
						data->InsertChars(data->CursorPos, matches[0].c_str());
						data->InsertChars(data->CursorPos, " ");
					}

					else if (matches.size() > 1)
					{
						int matchLength = wordEnd - wordStart;
						while (true)
						{
							int c = 0;
							bool allCandidatesMatches = true;
							for (int i = 0; i < matches.size() && allCandidatesMatches; ++i)
							{
								if (i == 0)
									c = toupper(matches[i][matchLength]);
								else if (c == 0 || c != toupper(matches[i][matchLength]))
									allCandidatesMatches = false;
							}
							if (!allCandidatesMatches)
								break;
							++matchLength;
						}

						if (matchLength > 0)
						{
							data->DeleteChars((int)(wordStart - data->Buf), (int)(wordEnd - wordStart));
							const auto matchString = matches[0].c_str();
							data->InsertChars(data->CursorPos, matchString, matchString + matchLength);
						}
					}

					break;
				}

				case ImGuiInputTextFlags_CallbackHistory:
				{
					// #TODO: History if empty, otherwise autocomplete.

					break;
				}
				}

				return 0;
			};

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + inputBoxSize + style.ItemSpacing.x);
			const auto inputFlags = ImGuiInputTextFlags_AutoSelectAll |
				ImGuiInputTextFlags_EnterReturnsTrue |
				ImGuiInputTextFlags_CallbackCompletion |
				ImGuiInputTextFlags_CallbackHistory;
			if (ImGui::InputTextEx("", "", buffer, std::size(buffer), { windowMax.x - windowMin.x, 0 }, inputFlags, textCallback, (void*)&cvarMatches))
			{
				if (ExecuteCommand(buffer))
				{
					buffer[0] = '\0';  // Clear the field.
					needsScrollUpdate = true;
				}
			}	
			ImGui::SetItemDefaultFocus();
			if (ImGui::IsWindowAppearing() || ImGui::IsItemDeactivatedAfterEdit())
			{
				registry.clear<ControlComponent>();
				ImGui::SetKeyboardFocusHere();
				consoleInputFocus = true;
			}

			// If the user unfocuses the input box, then IsItemDeactivated() will be 0 for a frame.
			// We need to lock out the recapture feature until the console is closed and reopened in this case.
			consoleInputFocus &= !ImGui::IsItemDeactivated() || ImGui::IsItemDeactivatedAfterEdit();
		}

		ImGui::EndChildFrame();
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();

		const auto entries = cvarMatches.size();
		if (entries > 0)
		{
			const auto entrySize = ImGui::CalcTextSize("Dummy").y + style.ItemSpacing.y;
			const auto autocompBoxMaxHeight = entrySize * 4;
			const auto autocompBoxSize = std::min(entries * entrySize + 2.f * style.FramePadding.y, autocompBoxMaxHeight);

			if (ImGui::BeginChildFrame(ImGui::GetID("Console Autocomplete"), { 0, autocompBoxSize }))
			{
				const char* typeMap[] = {
					"Int",
					"Float",
					"Function"
				};

				for (const auto cvar : cvarMatches)
				{
					const auto lineStart = ImGui::GetCursorPosX();
					ImGui::Text(cvar.first->name.c_str());
					ImGui::SameLine();

					const auto cvarName = cvar.first->name.c_str();
					const auto cvarSize = cvar.first->name.size();

					switch (cvar.first->type)
					{
					case Cvar::CvarType::Int:
					{
						if (auto cvarValue = CvarManager::Get().GetVariable<int>(entt::hashed_string::value(cvarName, cvarSize)); cvarValue)
						{
							std::stringstream valueStream;
							valueStream << *cvarValue;
							ImGui::TextDisabled("= %s", valueStream.str().c_str());
							ImGui::SameLine();
						}
						break;
					}
					case Cvar::CvarType::Float:
					{
						if (auto cvarValue = CvarManager::Get().GetVariable<float>(entt::hashed_string::value(cvarName, cvarSize)); cvarValue)
						{
							std::stringstream valueStream;
							valueStream << *cvarValue;
							ImGui::TextDisabled("= %s", valueStream.str().c_str());
							ImGui::SameLine();
						}
						break;
					}
					case Cvar::CvarType::Function:
					{
						if (auto cvarValue = CvarManager::Get().GetVariable<CvarCallableType>(entt::hashed_string::value(cvarName, cvarSize)); cvarValue)
						{
							ImGui::TextDisabled("= <function>");
							ImGui::SameLine();
						}
						break;
					}
					}
					
					ImGui::SetCursorPosX(lineStart + 350.f);
					ImGui::TextDisabled(typeMap[(uint32_t)cvar.first->type]);
					ImGui::SameLine();
					ImGui::SetCursorPosX(lineStart + 430.f);
					ImGui::TextDisabled(cvar.first->description.c_str());
				}
			}

			ImGui::EndChildFrame();
		}

		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}
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
		const bool shouldReacquireControl = consoleClosedThisFrame && consoleInputFocus;
		if ((ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered(ImGuiHoveredFlags_None)) || shouldReacquireControl)
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

		ImGui::SetCursorPos(viewportMin);
		DrawConsole(registry, sceneViewportMin, sceneViewportMax);
	}

	ImGui::End();

	ImGui::PopStyleVar();
}

void EditorUI::DrawControls(RenderDevice* device)
{
	if (controlsOpen)
	{
		if (ImGui::Begin("Controls", &controlsOpen))
		{
			if (ImGui::Button("Reload Shaders"))
			{
				Renderer::Get().ReloadShaderPipelines();
			}
		}

		ImGui::End();
	}
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

			else
			{
				const auto windowWidth = ImGui::GetWindowSize().x;
				const auto text = "No entity selected.";
				const auto textWidth = ImGui::CalcTextSize(text).x;

				ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);
				ImGui::TextDisabled(text);
			}
		}

		ImGui::End();
	}
}

void EditorUI::DrawMetrics(RenderDevice* device, float frameTimeMs)
{
	frameTimes.push_back(frameTimeMs);

	while (frameTimes.size() > frameTimeHistoryCount)
	{
		frameTimes.pop_front();
	}

	if (metricsOpen)
	{
		if (ImGui::Begin("Metrics", &metricsOpen))
		{
			DrawFrameTimeHistory();

			const auto memoryInfo = device->GetResourceManager().QueryMemoryInfo();

			ImGui::Separator();
			ImGui::Text("GPU Memory");

			ImGui::Text("Buffers (%u objects): %.2f MB", memoryInfo.bufferCount, memoryInfo.bufferBytes / (1024.f * 1024.f));
			ImGui::Text("Textures (%u objects): %.2f MB", memoryInfo.textureCount, memoryInfo.textureBytes / (1024.f * 1024.f));
		}

		ImGui::End();
	}
}

void EditorUI::DrawRenderGraph(RenderDevice* device, RenderGraphResourceManager& resourceManager, TextureHandle depthStencil, TextureHandle scene)
{
	if (renderGraphOpen)
	{
		if (ImGui::Begin("Render Graph", &renderGraphOpen))
		{
			if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Linearize depth", &linearizeDepth);
				ImGui::Checkbox("Allow transient resource reuse", &resourceManager.transientReuse);
			}

			if (linearizeDepth)
			{
				ImGui::GetWindowDrawList()->AddCallback([](auto* list, auto& state)
				{
					state.linearizeDepth = true;
				}, nullptr);
			}

			ImGui::Image(device, depthStencil, { 0.25f, 0.25f });

			if (linearizeDepth)
			{
				ImGui::GetWindowDrawList()->AddCallback([](auto* list, auto& state)
				{
					state.linearizeDepth = false;
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
				case RenderOverlay::HiZ: *output = "Hierarchical Depth Pyramid"; break;
				default: return false;
				}

				return true;
			}, nullptr, 3);  // Note: Make sure to update the hardcoded count when new overlays are added.

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
			case RenderOverlay::HiZ:
			{
				const auto sceneViewportSize = sceneViewportMax - sceneViewportMin;
				char viewText[64];
				ImFormatString(viewText, std::size(viewText), "Viewing Depth Pyramid Level: %d", *CvarGet("hiZPyramidLevels", int));
				const auto viewTextSize = ImGui::CalcTextSize(viewText);

				ImGui::SetCursorPos({ sceneViewportMin.x + sceneViewportSize.x * 0.5f - viewTextSize.x * 0.5f, sceneViewportMin.y + sceneViewportSize.y - 80.f });
				ImGui::Text(viewText);

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

void EditorUI::AddConsoleMessage(const std::string& message)
{
	consoleMessages.push_back(message);

	if (consoleFullyScrolled)
	{
		needsScrollUpdate = true;
	}
}