// Copyright (c) 2019-2022 Andrew Depke

#include <Editor/EditorUI.h>
#include <Rendering/Device.h>
#include <Rendering/Renderer.h>
#include <Rendering/RenderGraphResourceManager.h>
#include <Core/CoreComponents.h>
#include <Rendering/RenderComponents.h>
#include <Editor/EntityReflection.h>
#include <Editor/ImGuiExtensions.h>
#include <Editor/CvarHelpers.h>
#include <Editor/Picking.h>
#include <Rendering/Base.h>
#include <Rendering/Atmosphere.h>
#include <Rendering/Clouds.h>
#include <Rendering/Bloom.h>
#include <Rendering/ClusteredLightCulling.h>
#include <Rendering/DebugDraw.h>
#include <Rendering/CommandList.h>
#include <Rendering/Resource.h>
#include <Core/Config.h>
#include <Utility/Math.h>
#include <Utility/StringTools.h>

#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <IconsFontAwesome5.h>
#include <stb_image_write.h>

#include <algorithm>
#include <numeric>
#include <string>
#include <sstream>
#include <optional>
#include <cstring>

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

	const auto frameWidth = ImGui::GetContentRegionAvail().x - window->WindowPadding.x - ImGui::CalcTextSize("Mean: 00.000").x;
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

void EditorUI::DrawRenderOverlayTools(RenderDevice* device, const ImVec2& min, const ImVec2& max)
{
	const auto toolsWindowFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoScrollWithMouse |
		//ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		//ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoDocking;

	enum class ToolPosition
	{
		Bottom,
		Right
	};

	ImVec2 toolWindowSize = { 100, 100 };
	ToolPosition position = ToolPosition::Bottom;

	switch (activeOverlay)
	{
	case RenderOverlay::Clusters:
		toolWindowSize = { 480, 50 };
		position = ToolPosition::Bottom;
		break;
	case RenderOverlay::HiZ:
		toolWindowSize = { 70, 300 };
		position = ToolPosition::Right;
		break;
	}

	const auto padding = 15.f;
	const auto windowBase = ImGui::GetWindowPos();  // Not sure why we need this, oh well.

	switch (position)
	{
	case ToolPosition::Bottom:
		ImGui::SetNextWindowPos({ windowBase.x + (max.x - min.x - toolWindowSize.x) * 0.5f, max.y - toolWindowSize.y - padding });
		break;
	case ToolPosition::Right:
		ImGui::SetNextWindowPos({ max.x - toolWindowSize.x - padding, windowBase.y + (max.y - min.y - toolWindowSize.y) * 0.5f });
		break;
	}

	if (ImGui::BeginChildFrame(ImGui::GetID("Render Overlay Tools"), toolWindowSize, toolsWindowFlags))
	{
		auto style = ImGui::GetStyle();

		switch (activeOverlay)
		{
		case RenderOverlay::Clusters:
		{
			// Color scale.

			const char* titleText = "Cluster froxel bins light count";
			const char* leftText = "0";
			char rightText[8];
			ImFormatString(rightText, std::size(rightText), "%i", *CvarGet("maxLightsPerFroxel", int));

			const auto titleSize = ImGui::CalcTextSize(titleText);
			const auto leftSize = ImGui::CalcTextSize(leftText);
			const auto rightSize = ImGui::CalcTextSize(rightText);

			ImGui::SetCursorPosX((toolWindowSize.x - titleSize.x) * 0.5f);
			ImGui::Text(titleText);

			const auto sceneViewportSize = max - min;
			const auto colorScaleSize = ImVec2{ toolWindowSize.x - std::max(leftSize.x, rightSize.x) * 2.f - style.FramePadding.x * 2.f - 4.f, 20.f };
			auto colorScalePosMin = ImGui::GetWindowPos();
			colorScalePosMin += { (toolWindowSize.x - colorScaleSize.x) * 0.5f, ImGui::GetCursorPosY() };
			auto* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilledMultiColor(colorScalePosMin, colorScalePosMin + colorScaleSize, IM_COL32(0, 255, 0, 255), IM_COL32(255, 0, 0, 255), IM_COL32(255, 0, 0, 255), IM_COL32(0, 255, 0, 255));

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f);
			ImGui::Text(leftText);

			ImGui::SameLine();
			ImGui::SetCursorPosX(toolWindowSize.x - rightSize.x - style.FramePadding.x);
			ImGui::Text(rightText);

			break;
		}

		case RenderOverlay::HiZ:
		{
			// Mip selector.

			const auto sceneViewportSize = sceneViewportMax - sceneViewportMin;
			char viewText[32];
			ImFormatString(viewText, std::size(viewText), "Depth\nPyramid\nLevel");
			const auto viewTextSize = ImGui::CalcTextSize(viewText);

			ImGui::Text(viewText);

			const auto& overlayComponent = device->GetResourceManager().Get(overlayTexture);
			const auto maxMip = std::min((int)std::floor(std::log2(std::max(overlayComponent.description.width, overlayComponent.description.height))) + 1, *CvarGet("hiZPyramidLevels", int));
			const auto sliderPad = 10.f;
			const auto sliderSize = ImVec2{ toolWindowSize.x - (style.FramePadding.x + sliderPad) * 2.f, toolWindowSize.y - viewTextSize.y - style.FramePadding.y * 2.f - style.ItemSpacing.y - 4.f };

			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + sliderPad);
			ImGui::VSliderInt("", sliderSize, &hiZOverlayMip, 0, maxMip - 1);

			break;
		}

		default: break;
		}
	}

	ImGui::EndChild();

	// Render the remove overlay button.

	const char* buttonText = "Remove render overlay";
	const auto removePadding = ImGui::GetStyle().WindowPadding + ImGui::GetStyle().FramePadding;
	const auto overlayRemoveSize = ImGui::CalcTextSize(buttonText) + removePadding * 2.f + ImVec2{ 8.f, 8.f };

	ImGui::SetNextWindowPos(max - overlayRemoveSize - ImVec2{ 18, 18 });
	if (ImGui::BeginChildFrame(ImGui::GetID("Render Overlay Remove"), overlayRemoveSize, toolsWindowFlags))
	{
		auto style = ImGui::GetStyle();

		if (ImGui::Button(buttonText))
		{
			renderOverlayOnScene = false;
		}
	}

	ImGui::EndChildFrame();
}

void EditorUI::DrawRenderOverlayProxy(RenderDevice* device, const ImVec2& min, const ImVec2& max)
{
	if (renderOverlayOnScene && activeOverlay != RenderOverlay::None)
	{
		auto& style = ImGui::GetStyle();

		const auto proxyWindowFlags =
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoNav |
			ImGuiWindowFlags_NoInputs |
			ImGuiWindowFlags_NoDocking;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0, 0 });
		ImGui::BeginChildFrame(ImGui::GetID("Render Overlay Proxy"), { 0, 0 }, proxyWindowFlags);
		ImGui::PopStyleVar();  // Don't affect the tools window.

		auto* window = ImGui::GetCurrentWindow();

		ImGui::Image(device, overlayTexture, { 1.f, 1.f }, { sceneWidthUV, sceneHeightUV }, { 1.f + sceneWidthUV, 1.f + sceneHeightUV }, { 1.f, 1.f, 1.f, overlayAlpha });

		DrawRenderOverlayTools(device, min, max);

		ImGui::EndChildFrame();
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
	if (ImGui::IsKeyPressed(ImGuiKey_F2))
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

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

		// Limit the height.
		constexpr auto heightMax = 220.f;
		const auto height = std::min(max.y - min.y, heightMax);
		windowMax.y = windowMin.y + height;

		constexpr auto frameColor = IM_COL32(20, 20, 20, 238);
		constexpr auto frameColorDark = IM_COL32(20, 20, 20, 242);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0, 0 });
		ImGui::PushStyleColor(ImGuiCol_FrameBg, frameColor);
		ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, IM_COL32(0, 0, 0, 0));
		
		if (ImGui::BeginChildFrame(ImGui::GetID("Console History"), { windowMax.x - windowMin.x, height }, ImGuiWindowFlags_NoMove))
		{
			auto* window = ImGui::GetCurrentWindow();

			ImGui::SetWindowFontScale(0.8f);
			for (const auto& message : consoleMessages)
			{
				ImGui::Text("%s", message.c_str());
			}
			ImGui::SetWindowFontScale(1.f);

			if (needsScrollUpdate)
			{
				ImGui::SetScrollHereY(1.f);
				needsScrollUpdate = false;
			}

			consoleFullyScrolled = ImGui::GetCursorPosY() - ImGui::GetScrollY() < 300.f;  // Near the bottom, autoscroll.
		}

		ImGui::EndChildFrame();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();

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

		if (ImGui::BeginChildFrame(ImGui::GetID("Console Input"), { windowMax.x - windowMin.x, inputBoxSize }, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
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

						// If the cvar is a function, add (), otherwise add a space.

						auto matchIt = std::find_if(rawMatches->begin(), rawMatches->end(), [&matches](auto it)
						{
							return it.first->name == matches[0];
						});
						VGAssert(matchIt != rawMatches->end(), "Failed to find Cvar match in autocomplete.");
						const auto match = matchIt->first;

						if (match->type == Cvar::CvarType::Function)
						{
							data->InsertChars(data->CursorPos, "()");
						}
						else
						{
							data->InsertChars(data->CursorPos, " ");
						}
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

			const float hintSpacing = style.ItemSpacing.x + 25.f;
			ImGui::SetCursorPosX(hintSpacing);
			
			if (ImGui::IsWindowAppearing() || ImGui::IsItemDeactivatedAfterEdit())
			{
				registry.clear<ControlComponent>();
				ImGui::SetKeyboardFocusHere();
				consoleInputFocus = true;
			}

			ImGui::SetItemDefaultFocus();
			
			const auto inputFlags = ImGuiInputTextFlags_AutoSelectAll |
				ImGuiInputTextFlags_EnterReturnsTrue |
				ImGuiInputTextFlags_CallbackCompletion |
				ImGuiInputTextFlags_CallbackHistory;
			if (ImGui::InputTextEx("##", "", buffer, std::size(buffer), { windowMax.x - windowMin.x - hintSpacing, 0 }, inputFlags, textCallback, (void*)&cvarMatches))
			{
				if (ExecuteCommand(buffer))
				{
					buffer[0] = '\0';  // Clear the field.
					needsScrollUpdate = true;
				}
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

void EditorUI::Update(RenderDevice& device, entt::registry& registry)
{
	if (fullscreen != Renderer::Get().window->IsFullscreen())
	{
		const auto [width, height] = Renderer::Get().GetResolution();
		Renderer::Get().window->SetSize(width, height, fullscreen);
	}

	FlushPendingSave(device, registry);
}

void EditorUI::CaptureThumbnail(RenderDevice& device, CommandList& list, TextureHandle ldr)
{
	// Nothing to do unless a save is pending and we haven't already queued the copy this
	// frame.
	if (!pendingSavePath.has_value() || pendingSaveCaptureEnqueued)
	{
		return;
	}

	auto& resourceManager = device.GetResourceManager();
	if (!resourceManager.Valid(ldr))
	{
		return;
	}

	auto& ldrComponent = resourceManager.Get(ldr);

	// Walk the D3D12 footprint to learn the row pitch that the GPU will use when copying
	// into a buffer. This is typically aligned up to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT
	// (256 bytes), so the readback buffer is usually larger than width * height * bpp.
	const auto resourceDesc = ldrComponent.Native()->GetDesc();
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
	uint64_t requiredSize = 0;
	device.Native()->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &footprint, nullptr, nullptr, &requiredSize);

	pendingSaveWidth = ldrComponent.description.width;
	pendingSaveHeight = ldrComponent.description.height;
	pendingSaveRowPitch = footprint.Footprint.RowPitch;

	BufferDescription readbackDesc{};
	readbackDesc.updateRate = ResourceFrequency::Readback;
	readbackDesc.bindFlags = 0;
	readbackDesc.accessFlags = AccessFlag::CPURead;
	readbackDesc.size = static_cast<size_t>(requiredSize);
	readbackDesc.stride = 1;
	pendingSaveReadback = resourceManager.Create(readbackDesc, VGText("Editor Thumbnail"));

	// Transition the texture to COPY_SOURCE, all other UI work must be completed before this.
	list.TransitionBarrier(ldr, D3D12_RESOURCE_STATE_COPY_SOURCE);
	list.FlushBarriers();

	auto& readbackComponent = resourceManager.Get(pendingSaveReadback);

	D3D12_TEXTURE_COPY_LOCATION sourceCopy{};
	sourceCopy.pResource = ldrComponent.Native();
	sourceCopy.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	sourceCopy.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION destCopy{};
	destCopy.pResource = readbackComponent.Native();
	destCopy.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	destCopy.PlacedFootprint = footprint;

	list.Native()->CopyTextureRegion(&destCopy, 0, 0, 0, &sourceCopy, nullptr);

	pendingSaveCaptureEnqueued = true;
}

void EditorUI::FlushPendingSave(RenderDevice& device, entt::registry& registry)
{
	// Only do real work once the copy has actually been recorded into a GPU command list.
	if (!pendingSavePath.has_value() || !pendingSaveCaptureEnqueued)
	{
		return;
	}

	// Full sync on the GPU to ensure the readback was populated. Pretty terrible but thumbnails
	// don't get saved often so good enough for now.
	device.Synchronize();

	auto& resourceManager = device.GetResourceManager();

	std::vector<uint8_t> rawBytes;
	resourceManager.Read(pendingSaveReadback, rawBytes);

	std::vector<uint8_t> packed;
	if (!rawBytes.empty() && pendingSaveWidth > 0 && pendingSaveHeight > 0)
	{
		// Strip the GPU row pitch padding so what we hand to stb_image_write is a tightly
		// packed RGBA8 buffer matching the texture's logical width.
		const size_t tightRowBytes = static_cast<size_t>(pendingSaveWidth) * 4;
		packed.resize(tightRowBytes * pendingSaveHeight);
		for (uint32_t row = 0; row < pendingSaveHeight; ++row)
		{
			std::memcpy(
				packed.data() + row * tightRowBytes,
				rawBytes.data() + static_cast<size_t>(row) * pendingSaveRowPitch,
				tightRowBytes);
		}
	}

	std::vector<uint8_t> pngBytes;
	if (!packed.empty())
	{
		const auto writeFunction = [](void* context, void* data, int size)
		{
			auto* output = static_cast<std::vector<uint8_t>*>(context);
			const auto* bytes = static_cast<const uint8_t*>(data);
			output->insert(output->end(), bytes, bytes + size);
		};

		stbi_write_png_to_func(
			writeFunction, &pngBytes,
			static_cast<int>(pendingSaveWidth),
			static_cast<int>(pendingSaveHeight),
			4,
			packed.data(),
			static_cast<int>(pendingSaveWidth * 4));
	}

	Scene::Save(registry, *pendingSavePath, pngBytes);

	// Tear down the readback buffer and reset the state machine. Also invalidate the
	// cached scene list so the newly-saved scene (and its thumbnail) appear in the
	// selector on the next frame.
	resourceManager.Destroy(pendingSaveReadback);
	pendingSavePath.reset();
	pendingSaveCaptureEnqueued = false;
	pendingSaveWidth = 0;
	pendingSaveHeight = 0;
	pendingSaveRowPitch = 0;

	refreshScenes = true;
}

void EditorUI::RefreshScenes(RenderDevice& device)
{
	// Mark any existing loaded scene thumbnails for cleanup.
	auto& resourceManager = device.GetResourceManager();
	for (auto& scene : loadedScenes)
	{
		if (resourceManager.Valid(scene.thumbnail))
		{
			resourceManager.AddFrameResource(device.GetFrameIndex(), scene.thumbnail);
		}
	}

	loadedScenes = Scene::List(device);
	refreshScenes = false;
}

std::filesystem::path EditorUI::PickNextNewScenePath() const
{
	// Find the next scene file name, of the format "new-N.scene".
	const auto baseDir = Config::scenesPath;
	auto candidate = baseDir / "new.scene";
	std::error_code ec;
	if (!std::filesystem::exists(candidate, ec))
	{
		return candidate;
	}
	for (int i = 1; i < 10000; ++i)
	{
		candidate = baseDir / ("new-" + std::to_string(i) + ".scene");
		if (!std::filesystem::exists(candidate, ec))
		{
			return candidate;
		}
	}
	return baseDir / "new.scene";
}

void EditorUI::DrawLayout()
{
	auto* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
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

	// Build the default dock layout if the user hasn't overriden it themselves.
	if (!ImGui::DockBuilderGetNode(dockSpaceId))
	{
		ImGui::DockBuilderRemoveNode(dockSpaceId);
		ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_None);

		ImGuiID sceneDockId = 0;
		ImGuiID controlsDockId = 0;
		ImGuiID entitiesDockId = 0;
		ImGuiID propertiesDockId = 0;
		ImGuiID metricsDockId = 0;
		
		sceneDockId = ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, 0.75f, nullptr, &controlsDockId);
		entitiesDockId = ImGui::DockBuilderSplitNode(controlsDockId, ImGuiDir_Up, 0.4f, nullptr, &propertiesDockId);
		controlsDockId = ImGui::DockBuilderSplitNode(entitiesDockId, ImGuiDir_Up, 0.19f, nullptr, &entitiesDockId);
		propertiesDockId = ImGui::DockBuilderSplitNode(propertiesDockId, ImGuiDir_Up, 0.8f, nullptr, &metricsDockId);

		ImGui::DockBuilderDockWindow("Scene", sceneDockId);
		ImGui::DockBuilderDockWindow("Controls", controlsDockId);
		ImGui::DockBuilderDockWindow("Entity Hierarchy", entitiesDockId);
		ImGui::DockBuilderDockWindow("Property Viewer", propertiesDockId);
		ImGui::DockBuilderDockWindow("Metrics", metricsDockId);
		ImGui::DockBuilderDockWindow("Render Graph", propertiesDockId);
		ImGui::DockBuilderDockWindow("Sky Atmosphere", entitiesDockId);
		ImGui::DockBuilderDockWindow("Bloom", entitiesDockId);
		ImGui::DockBuilderDockWindow("Render Visualizer", propertiesDockId);
		ImGui::DockBuilderDockWindow("Dear ImGui Demo", sceneDockId);
		
		ImGui::DockBuilderFinish(dockSpaceId);
	}

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

void EditorUI::DrawSelectionGizmo(entt::registry& registry)
{
	if (hierarchySelectedEntity == entt::null
		|| !registry.valid(hierarchySelectedEntity)
		|| !registry.all_of<TransformComponent>(hierarchySelectedEntity))
	{
		return;
	}

	// Hotkeys for switching gizmo operation/mode. Only honored when the scene window is focused
	// and a camera isn't being controlled.
	if (ImGui::IsWindowFocused() && registry.view<const ControlComponent>().size() == 0)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_1))
			gizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_2))
			gizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_3))
			gizmoOperation = ImGuizmo::SCALE;
		if (ImGui::IsKeyPressed(ImGuiKey_X))
			gizmoMode = (gizmoMode == ImGuizmo::WORLD) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
	}

	auto& transform = registry.get<TransformComponent>(hierarchySelectedEntity);
	const auto translation = XMVectorSet(transform.translation.x, transform.translation.y, transform.translation.z, 0.f);

	// Draw a debug bounding sphere around the selected entity whenever it has mesh geometry.
	// Placed before the behind-camera early-out so the sphere is always rendered while something
	// is selected, even if the entity drifts behind the camera (DebugShapes is world-space and
	// handled by the renderer, unlike the ImGuizmo overlay which has its own camera limits).
	if (registry.all_of<MeshComponent>(hierarchySelectedEntity))
	{
		const auto& mesh = registry.get<MeshComponent>(hierarchySelectedEntity);
		if (!mesh.subsets.empty())
		{
			// Gather the largest bounding sphere radius across all subsets so a single sphere
			// fully encloses the whole mesh regardless of how many material regions it has.
			float maxRadius = 0.f;
			for (const auto& subset : mesh.subsets)
			{
				maxRadius = std::max(maxRadius, subset.boundingSphereRadius);
			}

			// Scale the local-space radius up by the largest axis scale to approximate the
			// world-space bounding sphere under non-uniform scaling.
			const float maxScale = std::max({ transform.scale.x, transform.scale.y, transform.scale.z });

			// Render without depth so the sphere is always visible.
			Draw::Sphere(transform.translation, maxRadius * maxScale, { 0.f, 1.f, 0.f, 1.f }, false);
		}
	}

	// Behind-camera early-out. ImGuizmo has its own check, but it doesn't work with reverse Z.
	// Do the test in view space, where the projection quirk doesn't apply: in RH view space,
	// in-front points have z < 0. Allow an in-progress drag to keep updating even if the entity
	// briefly crosses behind the camera.
	const XMVECTOR worldPos = XMVectorSetW(translation, 1.f);
	const XMVECTOR viewPos = XMVector4Transform(worldPos, globalViewMatrix);
	if (XMVectorGetZ(viewPos) >= 0.f && !ImGuizmo::IsUsing())
	{
		return;
	}

	ImGuizmo::SetDrawlist();
	ImGuizmo::SetOrthographic(false);

	const float visibleWidth = sceneViewportMax.x - sceneViewportMin.x;
	const float visibleHeight = sceneViewportMax.y - sceneViewportMin.y;
	ImGuizmo::SetRect(sceneViewportMin.x, sceneViewportMin.y, visibleWidth, visibleHeight);

	const auto scaling = XMVectorSet(transform.scale.x, transform.scale.y, transform.scale.z, 0.f);
	const auto scalingMat = XMMatrixScalingFromVector(scaling);
	const auto rotationMat = XMMatrixRotationX(-transform.rotation.x) * XMMatrixRotationY(-transform.rotation.y) * XMMatrixRotationZ(-transform.rotation.z);
	const auto translationMat = XMMatrixTranslationFromVector(translation);

	XMFLOAT4X4 model;
	XMStoreFloat4x4(&model, scalingMat * rotationMat * translationMat);

	// The scene image is a UV-cropped view of a render target that may be larger than the visible
	// window: the renderer projects to the FULL render target's NDC range, but the user only sees
	// the central [sceneWidthUV, 1 - sceneWidthUV] slice. Since SetRect is locked to the visible
	// rect (so the hard clip above doesn't escape the scene window), we instead compose a scale
	// onto the projection that "zooms" the visible NDC slice to fill [-1, 1]. The math:
	//   visible NDC range = [-1 + 2*uv, 1 - 2*uv] → width 2*(1 - 2*uv).
	//   To map that to [-1, 1] (full NDC for ImGuizmo's SetRect), scale by 1/(1 - 2*uv).
	// This produces the same screen position the renderer does, and works the same for ImGuizmo's
	// inverse-projection mouse picking on gizmo handles.
	const float zoomX = 1.f / std::max(1.f - 2.f * sceneWidthUV, 1e-4f);
	const float zoomY = 1.f / std::max(1.f - 2.f * sceneHeightUV, 1e-4f);
	const auto cropScale = XMMatrixScaling(zoomX, zoomY, 1.f);

	XMFLOAT4X4 view, projection;
	XMStoreFloat4x4(&view, globalViewMatrix);
	XMStoreFloat4x4(&projection, globalProjectionMatrix * cropScale);

	const auto op = static_cast<ImGuizmo::OPERATION>(gizmoOperation);
	const auto mode = static_cast<ImGuizmo::MODE>(gizmoMode);

	if (ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], op, mode, &model.m[0][0]))
	{
		// Write the updated components back to the entity.
		float t[3], r[3], s[3];
		ImGuizmo::DecomposeMatrixToComponents(&model.m[0][0], t, r, s);

		transform.translation = { t[0], t[1], t[2] };
		transform.rotation = {
			-XMConvertToRadians(r[0]),
			-XMConvertToRadians(r[1]),
			-XMConvertToRadians(r[2]),
		};
		transform.scale = { s[0], s[1], s[2] };
	}
}

void EditorUI::DrawSceneToolbar(const ImVec2& viewportMin, const ImVec2& viewportMax)
{
	const float buttonSize = 28.f;
	const float buttonSpacing = 4.f;     // Spacing between adjacent buttons in the same group.
	const float groupSpacing = 10.f;     // Wider gap between the grid button and the gizmo cluster.
	const float padding = 12.f;          // Distance from the scene viewport edges.
	const float fadedOpacity = 0.25f;    // Baseline opacity when the cursor is far away.
	const float proximityRadius = 80.f;  // Distance at which the toolbar starts to fade in.

	// 1 grid button + 3 gizmo mode buttons.
	const float toolbarWidth = buttonSize * 4.f + buttonSpacing * 3.f + groupSpacing;
	const float toolbarHeight = buttonSize;

	const ImVec2 toolbarPos = {
		viewportMax.x - toolbarWidth - padding,
		viewportMin.y + padding,
	};
	const ImVec2 toolbarRectMax = toolbarPos + ImVec2{ toolbarWidth, toolbarHeight };

	// Track the toolbar bounds so other code can use it, like Picking.
	sceneToolbarMin = toolbarPos;
	sceneToolbarMax = toolbarRectMax;

	// Drive the fade from the cursor's distance to the toolbar rect. If the cursor is inside
	// the rect the distance is zero. Outside, we use the closest-point distance and fade in as
	// it approaches the edge so the buttons "wake up" before the cursor reaches them.
	const ImVec2 mouse = ImGui::GetIO().MousePos;
	const float dx = std::max({ toolbarPos.x - mouse.x, 0.f, mouse.x - toolbarRectMax.x });
	const float dy = std::max({ toolbarPos.y - mouse.y, 0.f, mouse.y - toolbarRectMax.y });
	const float distance = std::sqrt(dx * dx + dy * dy);
	const float proximity = std::clamp(1.f - distance / proximityRadius, 0.f, 1.f);
	const float targetOpacity = std::lerp(fadedOpacity, 1.f, proximity);

	// Exponential smoothing toward the target so the fade animates rather than snaps.
	const float dt = std::max(ImGui::GetIO().DeltaTime, 0.f);
	const float smoothingRate = 14.f;
	const float blend = 1.f - std::exp(-dt * smoothingRate);
	sceneToolbarOpacity = std::lerp(sceneToolbarOpacity, targetOpacity, blend);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { buttonSpacing, 0.f });

	// SetCursorPos expects window-local coordinates. The scene window does not scroll, so
	// converting the screen-space toolbar position with GetWindowPos is sufficient.
	bool gridEnabled = *CvarGet("referenceGridEnabled", int) != 0;
	ImGui::SetCursorPos(ImVec2{ toolbarPos.x - ImGui::GetWindowPos().x, toolbarPos.y - ImGui::GetWindowPos().y });
	if (ImGui::FloatingIconButton((char*)ICON_FA_BORDER_ALL, "Toggle reference grid", gridEnabled, sceneToolbarOpacity, { buttonSize, buttonSize }))
	{
		CvarSet("referenceGridEnabled", gridEnabled ? 0 : 1);
	}

	ImGui::SameLine(0.f, groupSpacing);
	if (ImGui::FloatingIconButton((char*)ICON_FA_ARROWS_ALT, "Translate (1)", gizmoOperation == ImGuizmo::TRANSLATE, sceneToolbarOpacity, { buttonSize, buttonSize }))
	{
		gizmoOperation = ImGuizmo::TRANSLATE;
	}
	ImGui::SameLine(0.f, buttonSpacing);
	if (ImGui::FloatingIconButton((char*)ICON_FA_SYNC_ALT, "Rotate (2)", gizmoOperation == ImGuizmo::ROTATE, sceneToolbarOpacity, { buttonSize, buttonSize }))
	{
		gizmoOperation = ImGuizmo::ROTATE;
	}
	ImGui::SameLine(0.f, buttonSpacing);
	if (ImGui::FloatingIconButton((char*)ICON_FA_EXPAND_ALT, "Scale (3)", gizmoOperation == ImGuizmo::SCALE, sceneToolbarOpacity, { buttonSize, buttonSize }))
	{
		gizmoOperation = ImGuizmo::SCALE;
	}

	ImGui::PopStyleVar();
}

void EditorUI::DrawSceneIcon(RenderDevice* device, entt::registry& registry, const SceneMetadata& scene)
{
	// Card geometry. The thumbnail occupies the upper square region, the name is rendered below
	// it, and the whole rect is a single click target so the user can target either the image
	// or the label.
	constexpr float thumbnailSize = 96.f;
	constexpr float padding = 6.f;
	const float textHeight = ImGui::GetTextLineHeight();
	const ImVec2 cardSize{ thumbnailSize + padding * 2.f, thumbnailSize + textHeight + padding * 3.f };

	ImGui::PushID(scene.path.generic_string().c_str());

	const ImVec2 cardMin = ImGui::GetCursorScreenPos();
	const bool clicked = ImGui::InvisibleButton("##scene_card", cardSize);
	const bool hovered = ImGui::IsItemHovered();
	const bool held = ImGui::IsItemActive();
	const ImVec2 cardMax = cardMin + cardSize;

	// Right-click context menu. All filesystem work is deferred.
	if (ImGui::BeginPopupContextItem("##scene_card_context"))
	{
		if (ImGui::MenuItem("Rename"))
		{
			renamingScenePath = scene.path;
			const auto stem = scene.path.stem().string();
			const auto copyLen = std::min(stem.size(), sizeof(renameBuffer) - 1);
			std::memcpy(renameBuffer, stem.data(), copyLen);
			renameBuffer[copyLen] = '\0';
		}
		if (ImGui::MenuItem("Delete"))
		{
			pendingDeletePath = scene.path;
		}
		ImGui::EndPopup();
	}

	auto* drawList = ImGui::GetWindowDrawList();
	const auto& style = ImGui::GetStyle();
	const float rounding = style.FrameRounding;

	// Card background reacts to hover/press so the user gets standard button feedback even
	// though the contents are drawn directly to the draw list.
	const ImU32 backgroundColor = ImGui::GetColorU32(
		held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
	drawList->AddRectFilled(cardMin, cardMax, backgroundColor, rounding);
	drawList->AddRect(cardMin, cardMax, ImGui::GetColorU32(ImGuiCol_Border), rounding, 0, 1.f);

	// Thumbnail region — image when a valid texture is available, otherwise a sunken panel
	// with a centered placeholder label.
	const ImVec2 thumbMin = cardMin + ImVec2{ padding, padding };
	const ImVec2 thumbMax = thumbMin + ImVec2{ thumbnailSize, thumbnailSize };

	if (device && device->GetResourceManager().Valid(scene.thumbnail))
	{
		const auto& textureComponent = device->GetResourceManager().Get(scene.thumbnail);
		drawList->AddImageRounded(
			(ImTextureID)textureComponent.SRV->bindlessIndex,
			thumbMin, thumbMax,
			{ 0.f, 0.f }, { 1.f, 1.f },
			IM_COL32_WHITE,
			rounding);
	}

	else
	{
		drawList->AddRectFilled(thumbMin, thumbMax, ImGui::GetColorU32(ImGuiCol_FrameBg), rounding);

		const char* placeholder = "No Thumbnail";
		const ImVec2 placeholderSize = ImGui::CalcTextSize(placeholder);
		const ImVec2 placeholderPos{
			thumbMin.x + (thumbnailSize - placeholderSize.x) * 0.5f,
			thumbMin.y + (thumbnailSize - placeholderSize.y) * 0.5f
		};
		drawList->AddText(placeholderPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), placeholder);
	}

	// Centered, hard-clipped name strip directly under the thumbnail. Clipping prevents long
	// scene names from spilling outside the card footprint.
	const ImVec2 nameSize = ImGui::CalcTextSize(scene.name.c_str());
	const ImVec2 nameClipMin{ thumbMin.x, thumbMax.y + padding };
	const ImVec2 nameClipMax{ thumbMax.x, nameClipMin.y + textHeight };
	const ImVec2 namePos{
		thumbMin.x + std::max(0.f, (thumbnailSize - nameSize.x) * 0.5f),
		nameClipMin.y
	};
	drawList->PushClipRect(nameClipMin, nameClipMax, true);
	drawList->AddText(namePos, ImGui::GetColorU32(ImGuiCol_Text), scene.name.c_str());
	drawList->PopClipRect();

	// Tooltip shows the full scene path on hover.
	if (hovered)
	{
		ImGui::SetTooltip("%s", scene.path.generic_string().c_str());
	}

	if (clicked)
	{
		Scene::Load(registry, scene.path);
	}

	ImGui::PopID();
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

		// Draw the manipulation gizmo before click handling to account for gizmo hover.
		DrawSelectionGizmo(registry);

		// Only draw the toolbar if not in control.
		if (registry.view<const ControlComponent>().size() == 0)
		{
			DrawSceneToolbar(sceneViewportMin, sceneViewportMax);
		}

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

		// Single click tries to select an entity in the scene.
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
			!ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
			ImGui::IsWindowHovered(ImGuiHoveredFlags_None) &&
			!ImGuizmo::IsOver() &&
			!ImGuizmo::IsUsing() &&
			!ImGui::IsMouseHoveringRect(sceneToolbarMin, sceneToolbarMax, false) &&
			registry.view<const ControlComponent>().size() == 0)
		{
			const ImVec2 mouseLocal = ImGui::GetMousePos() - sceneViewportMin;
			const ImVec2 viewportPixels = sceneViewportMax - sceneViewportMin;

			XMVECTOR rayOrigin, rayDirection;
			Picking::ProjectUIToWorld(mouseLocal.x, mouseLocal.y, viewportPixels.x, viewportPixels.y, sceneWidthUV, sceneHeightUV,
				globalViewMatrix, globalProjectionMatrix, rayOrigin, rayDirection);
			hierarchySelectedEntity = Picking::Pick(registry, rayOrigin, rayDirection);
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
		DrawRenderOverlayProxy(device, sceneViewportMin, sceneViewportMax);

		if (showFps && frameTimes.size() > 0)
		{
			auto& style = ImGui::GetStyle();

			ImGui::SetWindowFontScale(1.5f);

			const auto fpsTextSize = ImGui::CalcTextSize("FPS: 000.0");
			const auto fpsTextPosition = ImVec2{ viewportMax.x - fpsTextSize.x - 40.f, viewportMin.y + 40.f };
			ImGui::SetCursorPos(fpsTextPosition);

			const auto border = 2.f;
			const auto offset = 2.f;
			const auto screenOffset = ImGui::GetWindowPos();
			const auto frameMin = ImVec2{ fpsTextPosition.x - border - 4.f, fpsTextPosition.y - border - offset };
			const auto frameMax = ImVec2{ fpsTextPosition.x + fpsTextSize.x + border + 4.f, fpsTextPosition.y + fpsTextSize.y + border - offset };
			auto frameColor = ImGui::GetColorU32(ImGuiCol_FrameBg, 0.85f);
			ImGui::RenderFrame(screenOffset + frameMin, screenOffset + frameMax, frameColor, true);

			const auto fps = 1000000.f / frameTimes.back();
			auto textColor = IM_COL32(0, 255, 0, 255);
			if (fps < 30.f)
				textColor = IM_COL32(255, 0, 0, 255);
			else if (fps < 60.f)
				textColor = IM_COL32(252, 86, 3, 255);
			ImGui::PushStyleColor(ImGuiCol_Text, textColor);
			ImGui::Text("FPS: %.1f", fps);
			ImGui::PopStyleColor();
			ImGui::SetWindowFontScale(1.f);
		}

		ImGui::SetCursorPos(viewportMin);
		DrawConsole(registry, sceneViewportMin, sceneViewportMax);
	}

	ImGui::End();

	ImGui::PopStyleVar();
}

void EditorUI::DrawSceneSelector(RenderDevice* device, entt::registry& registry)
{
	// Handle scene refresh inside the render pass since thumbnail uploading might happen.
	if (refreshScenes)
	{
		refreshScenes = false;
		loadedScenes = Scene::List(*device);
	}

	if (ImGui::Begin("Scene Selector"))
	{
		if (ImGui::Button("Clear scene"))
		{
			Scene::Clear(registry);

			// Restore a simple spectator camera.
			TransformComponent spectatorTransform{};
			spectatorTransform.translation = { 0.f, 0.f, 100.f };
			spectatorTransform.rotation = { 0.f, 0.f, 0.f };

			const auto spectator = registry.create();
			registry.emplace<NameComponent>(spectator, "Spectator");
			registry.emplace<TransformComponent>(spectator, std::move(spectatorTransform));
			registry.emplace<CameraComponent>(spectator);
		}

		ImGui::SameLine();
		if (ImGui::Button("Save scene"))
		{
			pendingSavePath = PickNextNewScenePath();
			pendingSaveCaptureEnqueued = false;
		}

		ImGui::SameLine();
		ImGui::PushID("refresh_scenes");
		if (ImGui::Button((char*)ICON_FA_SYNC))
		{
			RefreshScenes(*device);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Reload scenes");
		}
		ImGui::PopID();

		ImGui::Separator();

		if (loadedScenes.size() == 0)
		{
			ImGui::TextDisabled("No scenes found.");
		}
		else
		{
			// Keep the card width here in sync with the geometry used by DrawSceneIcon so the
			// wrapping math lines up with what is actually rendered.
			constexpr float thumbnailSize = 96.f;
			constexpr float padding = 6.f;
			const float cardWidth = thumbnailSize + padding * 2.f;

			const float available = ImGui::GetContentRegionAvail().x;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const int columns = std::max(1, static_cast<int>((available + spacing) / (cardWidth + spacing)));

			int column = 0;
			for (const auto& scene : loadedScenes)
			{
				if (column != 0)
				{
					ImGui::SameLine();
				}

				DrawSceneIcon(device, registry, scene);

				if (++column >= columns)
				{
					column = 0;
				}
			}
		}
	}

	ImGui::End();

	// Handle scene mutations after the draw loop is complete.
	if (pendingDeletePath.has_value())
	{
		std::error_code ec;
		std::filesystem::remove(*pendingDeletePath, ec);
		if (ec)
		{
			VGLogWarning(logEditor, "Failed to delete scene '{}': {}",
				pendingDeletePath->generic_wstring(), Str2WideStr(ec.message()));
		}
		pendingDeletePath.reset();
		if (device)
		{
			RefreshScenes(*device);
		}
	}

	if (renamingScenePath.has_value())
	{
		if (!ImGui::IsPopupOpen("Rename Scene"))
		{
			ImGui::OpenPopup("Rename Scene");
		}
	}
	if (ImGui::BeginPopupModal("Rename Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("New name (no extension):");
		if (ImGui::IsWindowAppearing())
		{
			ImGui::SetKeyboardFocusHere();
		}
		const bool committed = ImGui::InputText("##rename_scene_input", renameBuffer, sizeof(renameBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

		const bool okPressed = ImGui::Button("OK") || committed;
		ImGui::SameLine();
		const bool cancelPressed = ImGui::Button("Cancel");

		if (okPressed && renamingScenePath.has_value())
		{
			const std::string newStem = renameBuffer;
			if (!newStem.empty())
			{
				const auto newPath = renamingScenePath->parent_path() / (newStem + ".scene");
				std::error_code ec;
				std::filesystem::rename(*renamingScenePath, newPath, ec);
				if (ec)
				{
					VGLogWarning(logEditor, "Failed to rename scene '{}' to '{}': {}",
						renamingScenePath->generic_wstring(), newPath.generic_wstring(),
						Str2WideStr(ec.message()));
				}
			}
			renamingScenePath.reset();
			ImGui::CloseCurrentPopup();
			if (device)
			{
				RefreshScenes(*device);
			}
		}
		else if (cancelPressed)
		{
			renamingScenePath.reset();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
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

			static const char* toneMappers[] = {
				"Disabled",
				"ACES (Hill)",
				"ACES (Narkowicz)",
				"AgX",
				"Khronos PBR Neutral",
				"Reinhard"
			};
			CvarHelpers::Combo("toneMapper", "Tone mapper", toneMappers, IM_ARRAYSIZE(toneMappers));

			CvarHelpers::Slider("exposure", "Exposure", 0.f, 50.f);
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

						ImGui::PushID(metaID);
						renderFunction(registry, hierarchySelectedEntity);
						ImGui::PopID();

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

void EditorUI::DrawAtmosphereControls(RenderDevice* device, entt::registry& registry, Atmosphere& atmosphere, Clouds& clouds, TextureHandle weather)
{
	if (atmosphereControlsOpen)
	{
		if (ImGui::Begin("Sky Atmosphere", &atmosphereControlsOpen))
		{
			ImGui::Text("General");
			if (registry.valid(atmosphere.sunLight))
			{
				ComponentProperties::RenderTimeOfDayComponent(registry, atmosphere.sunLight);
			}

			ImGui::Separator();

			ImGui::Text("Weather");
			ImGui::DragFloat("Cloud coverage", &clouds.coverage, 0.005f, 0.f, 1.f);
			ImGui::DragFloat("Precipitation", &clouds.precipitation, 0.005f, 0.f, 1.f);
			ImGui::DragFloat("Wind strength", &clouds.windStrength, 0.01f, 0.f, 1.f);
			ImGui::DragFloat2("Wind direction", (float*)&clouds.windDirection, 0.01f, -1.f, 1.f);

			ImGui::Image(device, weather, { 0.1f, 0.1f });

			ImGui::Separator();

			ImGui::Text("Clouds");

			static int rayMarchQuality = *CvarGet("cloudRayMarchQuality", int);
			static int lastRayMarchQuality = rayMarchQuality;
			ImGui::TextDisabled("Ray march quality");
			if (ImGui::Button("Low detail"))
				rayMarchQuality = 0;
			ImGui::SameLine();
			if (ImGui::Button("Normal"))
				rayMarchQuality = 1;
			ImGui::SameLine();
			if (ImGui::Button("Ground truth"))
				rayMarchQuality = 2;

			if (rayMarchQuality != lastRayMarchQuality)
			{
				CvarSet("cloudRayMarchQuality", rayMarchQuality);
				lastRayMarchQuality = rayMarchQuality;
			}
			
			CvarHelpers::Checkbox("renderLightShafts", "Render light shafts");
			CvarHelpers::Slider("cloudRenderScale", "Render scale", 0.1f, 1.f);

			// Debug tools.
			CvarHelpers::Checkbox("cloudDebugMarchCount", "Debug march count");
			CvarHelpers::Checkbox("cloudDebugTransmittance", "Debug transmittance");
			
			ImGui::Separator();

			ImGui::Text("Atmosphere");
			bool dirty = false;
			static float haze = 8;
			static float lastHaze = -1;

			ImGui::TextDisabled("Presets");
			if (ImGui::Button("Clear sky"))
				haze = 0;
			ImGui::SameLine();
			if (ImGui::Button("Light haze"))
				haze = 18;
			ImGui::SameLine();
			if (ImGui::Button("Heavy haze"))
				haze = 80;

			ImGui::DragFloat("Haze", &haze, 0.5f, 0.f, 100.f);

			if (haze != lastHaze)
				dirty = true;
			lastHaze = haze;

			// Only compute model coefficients if we modified the haze factor.
			if (dirty)
			{
				const auto epsilon = 0.00000001f;
				const auto defaultMie = 0.003996f * 1.2f;
				const auto newMie = haze * defaultMie + epsilon;
				atmosphere.model.mieScattering = { newMie, newMie, newMie };
				atmosphere.model.mieExtinction = { 1.11f * newMie, 1.11f * newMie, 1.11f * newMie };
			}

			ImGui::TextDisabled("Model");
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
	// We don't draw the overlay until the next frame, so just save it here.
	// #TODO: Bit of a scuffed solution, and causing a crash sometimes when changing overlays!
	overlayTexture = overlay;

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
					ImGui::Text("Drag the overlay onto the scene to view.");

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

			ImGui::SliderFloat("Overlay alpha", &overlayAlpha, 0.05f, 1.f, "%.2f");
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