function IncludeThirdParty()
	includedirs "VanguardEngine/ThirdParty/entt/single_include"
	includedirs "VanguardEngine/ThirdParty/json/single_include/nlohmann"
	includedirs "VanguardEngine/ThirdParty/imgui"
	includedirs "VanguardEngine/ThirdParty/Jobs/Jobs/Include"
	includedirs "VanguardEngine/ThirdParty/Tracy"
end

function LinkThirdParty()
	libdirs "Build/ThirdParty/Jobs"
	libdirs "Build/ThirdParty/Tracy"
	
	links "Jobs"
	links "Tracy"
end

function RunThirdParty()
	group "ThirdParty"
	include "VanguardEngine/ThirdParty/Jobs"
	include "VanguardEngine/ThirdParty/Tracy"
	include "VanguardEngine/ThirdParty/Tracy/server"
end

workspace "Vanguard"
	architecture "x86_64"
	platforms { "Win64" }
	configurations { "Debug", "Development", "Release" }
	startproject "Engine"  -- #TODO: Set to empty project, similar to UE4's BlankProgram
	buildlog "Build/Logs/Log"
	
project "Engine"
	language "C++"
	cppdialect "C++17"
	kind "WindowedApp"
	
	location "Build/Generated"
	basedir "../../"
	objdir "Build/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "Build/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "Vanguard"
	
	includedirs { "VanguardEngine/Source" }
	
	-- General Build
		
	filter {}
		defines { "PLATFORM_WINDOWS=0", "BUILD_DEBUG=0", "BUILD_DEVELOPMENT=0", "BUILD_RELEASE=0" }
		flags { "FatalWarnings", "NoPCH", "MultiProcessorCompile" }
		clr "Off"
		rtti "Off"
		characterset "Unicode"
		warnings "Extra"
		disablewarnings { "4324", "4127" }
		
	-- Specific Build
		
	filter { "platforms:Win64" }
		system "Windows"
		defines { "PLATFORM_WINDOWS=1", "NOMINMAX" }
		
	-- Configurations
		
	filter { "configurations:Debug" }
		defines { "BUILD_DEBUG=1", "DEBUG", "D3DCOMPILE_DEBUG=1" }
		flags { }
		symbols "On"
		optimize "Off"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Development" }
		defines { "BUILD_DEVELOPMENT=1", "DEBUG" }
		flags { "LinkTimeOptimization" }
		symbols "On"
		optimize "Debug"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Release" }
		defines { "BUILD_RELEASE=1", "NDEBUG", "ENTT_DISABLE_ASSERT" }
		flags { "LinkTimeOptimization", "NoRuntimeChecks" }
		symbols "Off"
		optimize "Speed"  -- /O2 instead of /Ox on MSVS with "Full"
		omitframepointer "On"
		exceptionhandling "Off"
		
	filter {}
		
	-- General Files
		
	filter {}
		files { "VanguardEngine/Source/Core/*.h", "VanguardEngine/Source/Core/*.cpp" }
		files { "VanguardEngine/Source/Debug/*.h", "VanguardEngine/Source/Debug/*.cpp" }
		files { "VanguardEngine/Source/Editor/*.h", "VanguardEngine/Source/Editor/*.cpp" }
		files { "VanguardEngine/Source/Rendering/*.h", "VanguardEngine/Source/Rendering/*.cpp" }
		files { "VanguardEngine/Source/Threading/*.h", "VanguardEngine/Source/Threading/*.cpp" }
		files { "VanguardEngine/Source/Utility/*.h", "VanguardEngine/Source/Utility/*.cpp" }
		
	-- Specific Files
		
	filter { "platforms:Win64" }
		files { "VanguardEngine/Source/Core/Windows/*.h", "VanguardEngine/Source/Core/Windows/*.cpp" }
		files { "VanguardEngine/Source/Debug/Windows/*.h", "VanguardEngine/Source/Debug/Windows/*.cpp" }
		files { "VanguardEngine/Source/Editor/Windows/*.h", "VanguardEngine/Source/Editor/Windows/*.cpp" }
		files { "VanguardEngine/Source/Rendering/Windows/*.h", "VanguardEngine/Source/Rendering/Windows/*.cpp" }
		files { "VanguardEngine/Source/Threading/Windows/*.h", "VanguardEngine/Source/Threading/Windows/*.cpp" }
		files { "VanguardEngine/Source/Utility/Windows/*.h", "VanguardEngine/Source/Utility/Windows/*.cpp" }
		
	filter {}
	
	-- Third Party Files
	
	IncludeThirdParty()
	
	-- General Links
	
	libdirs {}
	
	links { "d3d12", "dxgi", "d3dcompiler", "xinput" }
	
	-- Specific Links
	
	filter { "platforms:Win64" }
		links { "Synchronization" }
		
	filter { "platforms:not Win64" }
		links { "pthread" }
		
	filter {}
	
	-- Third Party Links
	
	LinkThirdParty()
	
	-- Run Third Party Build Scripts
	
	RunThirdParty()