project "Tracy"
	language "C++"
	kind "StaticLib"
	
	location "../../../Build/ThirdParty/Tracy/Generated"
	buildlog "../../../Build/Logs/TracyBuildLog.log"
	objdir "../../../Build/ThirdParty/Tracy/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/Tracy/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "Tracy"
	
	includedirs { "profiler/libs/gl3w", "imgui", "lib/freetype/include", "lib/glfw/include" }
	libdirs { "lib/glfw/bin/win32" }
	
	files "TracyClient.cpp"
	
	defines {
		"_CRT_SECURE_NO_DEPRECATE",
		"_CRT_NONSTDC_NO_DEPRECATE",
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"_USE_MATH_DEFINES",
	}
	
	filter { "configurations:Debug or configurations:Development" }
		defines { "TRACY_ENABLE" }
		
	filter {}
	
	links { "ws2_32", "opengl32", "glfw3" }