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
		
	filter {}
		defines { "PLATFORM_WINDOWS=0", "BUILD_DEBUG=0", "BUILD_DEVELOPMENT=0", "BUILD_RELEASE=0" }
		flags { "FatalWarnings", "NoPCH", "MultiProcessorCompile" }
		clr "Off"
		rtti "Off"
		characterset "Unicode"
		
	filter { "platforms:Win64" }
		system "Windows"
		defines { "PLATFORM_WINDOWS=1" }
		
	filter { "configurations:Debug" }
		defines { "BUILD_DEBUG=1", "DEBUG", "D3DCOMPILE_DEBUG=1" }
		flags { }
		symbols "On"
		optimize "Off"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Development" }
		defines { "BUILD_DEVELOPMENT=1", "DEBUG", "JOBS_DISABLE_LOGGING" }
		flags { "LinkTimeOptimization" }
		symbols "On"
		optimize "Debug"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Release" }
		defines { "BUILD_RELEASE=1", "NDEBUG", "JOBS_DISABLE_LOGGING" }
		flags { "LinkTimeOptimization", "NoRuntimeChecks" }
		symbols "Off"
		optimize "Full"
		omitframepointer "On"
		exceptionhandling "Off"
		
	filter {}
		files { "VanguardEngine/Source/Core/*.h", "VanguardEngine/Source/Core/*.cpp" }
		files { "VanguardEngine/Source/Debug/*.h", "VanguardEngine/Source/Debug/*.cpp" }
		files { "VanguardEngine/Source/Editor/*.h", "VanguardEngine/Source/Editor/*.cpp" }
		files { "VanguardEngine/Source/Rendering/*.h", "VanguardEngine/Source/Rendering/*.cpp" }
		files { "VanguardEngine/Source/Threading/*.h", "VanguardEngine/Source/Threading/*.cpp" }
		files { "VanguardEngine/Source/Utility/*.h", "VanguardEngine/Source/Utility/*.cpp" }
		
	filter { "platforms:Win64" }
		files { "VanguardEngine/Source/Core/Windows/*.h", "VanguardEngine/Source/Core/Windows/*.cpp" }
		files { "VanguardEngine/Source/Debug/Windows/*.h", "VanguardEngine/Source/Debug/Windows/*.cpp" }
		files { "VanguardEngine/Source/Editor/Windows/*.h", "VanguardEngine/Source/Editor/Windows/*.cpp" }
		files { "VanguardEngine/Source/Rendering/Windows/*.h", "VanguardEngine/Source/Rendering/Windows/*.cpp" }
		files { "VanguardEngine/Source/Threading/Windows/*.h", "VanguardEngine/Source/Threading/Windows/*.cpp" }
		files { "VanguardEngine/Source/Utility/Windows/*.h", "VanguardEngine/Source/Utility/Windows/*.cpp" }