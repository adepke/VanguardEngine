project "imguizmo"
	language "C++"
	kind "StaticLib"

	location "../../../Build/ThirdParty/imguizmo/Generated"
	buildlog "../../../Build/Logs/ImguizmoBuildLog.log"
	objdir "../../../Build/ThirdParty/imguizmo/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/imguizmo/Bin/%{cfg.platform}_%{cfg.buildcfg}"

	targetname "imguizmo"

	defines "IMGUI_DEFINE_MATH_OPERATORS"

	includedirs { "../imgui" }

	files { "src/ImGuizmo.h", "src/ImGuizmo.cpp" }
