project "imgui"
	language "C++"
	kind "StaticLib"
	
	location "../../../Build/ThirdParty/imgui/Generated"
	buildlog "../../../Build/Logs/ImguiBuildLog.log"
	objdir "../../../Build/ThirdParty/imgui/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/imgui/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "imgui"
	
	files { "imconfig.h", "imgui.h", "imgui.cpp", "imgui_draw.cpp", "imgui_widgets.cpp", "imgui_demo.cpp" }