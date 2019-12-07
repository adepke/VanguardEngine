workspace "Vanguard"
	architecture "x86_64"
	platforms { "Win64" }
	configurations { "Debug", "Development", "Release" }
	
project "Engine"
	language "C++"
	cppdialect "C++17"
	kind "SharedLib"
	
	location "Build/Generated"
	basedir "../../"
	objdir "Build/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "Build/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "Vanguard"
	
	filter { "platforms:Win64" }
		system "Windows"
		
	filter {}
		flags { "FatalWarnings", "NoPCH", "MultiProcessorCompile" }
		clr "Off"
		rtti "Off"
		characterset "Unicode"
		
	filter { "configurations:Debug" }
		defines { "DEBUG", "D3DCOMPILE_DEBUG=1" }
		flags { }
		symbols "On"
		optimize "Off"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Development" }
		defines { "DEBUG", "JOBS_DISABLE_LOGGING" }
		flags { "LinkTimeOptimization" }
		symbols "On"
		optimize "Debug"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Release" }
		defines { "NDEBUG", "JOBS_DISABLE_LOGGING" }
		flags { "LinkTimeOptimization", "NoRuntimeChecks" }
		symbols "Off"
		optimize "Full"
		omitframepointer "On"
		exceptionhandling "Off"
		
	filter {}
		files { "VanguardEngine/**.h", "VanguardEngine/**.cpp" }