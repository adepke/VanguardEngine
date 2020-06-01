function IncludeThirdParty()
	includedirs "VanguardEngine/ThirdParty/entt/single_include"
	includedirs "VanguardEngine/ThirdParty/json/single_include/nlohmann"
	includedirs "VanguardEngine/ThirdParty/imgui"
	includedirs "VanguardEngine/ThirdParty/Jobs/Jobs/Include"
	includedirs "VanguardEngine/ThirdParty/Tracy"
	includedirs "VanguardEngine/ThirdParty/D3D12MemoryAllocator/src"
	includedirs "VanguardEngine/ThirdParty/DirectXShaderCompiler"
	includedirs "VanguardEngine/ThirdParty/assimp/Include"
	includedirs "VanguardEngine/ThirdParty/stb"
end

function LinkThirdParty()
	libdirs "Build/ThirdParty/imgui/Bin/*"
	libdirs "Build/ThirdParty/Jobs/Bin/*"
	libdirs "Build/ThirdParty/Tracy/Bin/*"
	libdirs "Build/ThirdParty/D3D12MemoryAllocator/Bin/*"
	libdirs "VanguardEngine/ThirdParty/DirectXShaderCompiler"
	libdirs "VanguardEngine/ThirdParty/assimp/Bin/*"
	
	links "imgui"
	links "Jobs"
	links "Tracy"
	links "D3D12MemoryAllocator"
	links "dxcompiler"
	links "assimp"
end

function RunThirdParty()
	group "ThirdParty"
	include "VanguardEngine/ThirdParty/imgui"
	include "VanguardEngine/ThirdParty/Jobs"
	include "VanguardEngine/ThirdParty/Tracy"
	include "VanguardEngine/ThirdParty/Tracy/server"
	include "VanguardEngine/ThirdParty/D3D12MemoryAllocator"
	include "VanguardEngine/ThirdParty/assimp"
end

workspace "Vanguard"
	architecture "x86_64"
	platforms { "Win64" }
	configurations { "Debug", "Development", "Release" }
	startproject "Engine"  -- #TODO: Set to empty project, similar to UE4's BlankProgram.
	
	-- General Settings
	
	EnableLogging = true
	EnableProfiling = true
	EnableEditor = true
	
	cppdialect "C++17"
	
	-- Global Platform
	
	flags { "MultiProcessorCompile" }
	
	filter { "platforms:Win64" }
		defines { "WIN32", "_WIN32" }
		systemversion "latest"
	
	-- Global Configurations
	
	filter { "configurations:Debug" }
		defines { "DEBUG", "_DEBUG" }
		symbols "On"
		optimize "Off"
		
	filter { "configurations:Development" }
		defines { "DEBUG", "_DEBUG" }
		symbols "On"
		optimize "Debug"
	
	filter { "configurations:Release" }
		defines { "NDEBUG" }
		symbols "Off"
		optimize "Speed"
	
project "Engine"
	language "C++"
	kind "WindowedApp"
	
	location "Build/Generated"
	buildlog "Build/Logs/EngineBuildLog.log"
	basedir "../../"
	objdir "Build/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "Build/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "Vanguard"
	
	includedirs { "VanguardEngine/Source" }
	
	-- General Build
		
	filter {}
		defines { "PLATFORM_WINDOWS=0", "BUILD_DEBUG=0", "BUILD_DEVELOPMENT=0", "BUILD_RELEASE=0" }
		--flags { "FatalWarnings", "NoPCH" }  Disabled FatalWarnings indefinitely during early development
		flags { "NoPCH" }
		clr "Off"
		rtti "Off"
		characterset "Unicode"
		staticruntime "Off"
		--warnings "Extra"  Disabled Extra warnings indefinitely during early development
		warnings "Default"
		disablewarnings { "4324", "4127" }
		
		if EnableLogging then
			defines { "ENABLE_LOGGING=1" }
		else
			defines { "ENABLE_LOGGING=0", "JOBS_DISABLE_LOGGING" }
		end
		
		if EnableProfiling then
			defines { "ENABLE_PROFILING=1", "TRACY_ENABLE" }
		else
			defines { "ENABLE_PROFILING=0" }
		end
		
		if EnableEditor then
			defines { "ENABLE_EDITOR=1" }
		else
			defines { "ENABLE_EDITOR=0" }
		end
		
	-- Specific Build
		
	filter { "platforms:Win64" }
		system "Windows"
		defines { "PLATFORM_WINDOWS=1" }
		
	-- Configurations
		
	filter { "configurations:Debug" }
		defines { "BUILD_DEBUG=1" }
		flags { }
		symbols "On"
		optimize "Off"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Development" }
		defines { "BUILD_DEVELOPMENT=1" }
		flags { "LinkTimeOptimization" }
		symbols "On"
		optimize "Debug"
		omitframepointer "Off"
		exceptionhandling "On"
		
	filter { "configurations:Release" }
		defines { "BUILD_RELEASE=1", "ENTT_DISABLE_ASSERT" }
		flags { "LinkTimeOptimization", "NoRuntimeChecks" }
		symbols "Off"
		optimize "Speed"  -- /O2 instead of /Ox on MSVS with "Full".
		omitframepointer "On"
		exceptionhandling "Off"
		
	-- General Files
		
	filter {}
		files { "VanguardEngine/Source/Asset/*.h", "VanguardEngine/Source/Asset/*.cpp" }
		files { "VanguardEngine/Source/Core/*.h", "VanguardEngine/Source/Core/*.cpp" }
		files { "VanguardEngine/Source/Debug/*.h", "VanguardEngine/Source/Debug/*.cpp" }
		files { "VanguardEngine/Source/Editor/*.h", "VanguardEngine/Source/Editor/*.cpp" }
		files { "VanguardEngine/Source/Rendering/*.h", "VanguardEngine/Source/Rendering/*.cpp" }
		files { "VanguardEngine/Source/Threading/*.h", "VanguardEngine/Source/Threading/*.cpp" }
		files { "VanguardEngine/Source/Utility/*.h", "VanguardEngine/Source/Utility/*.cpp" }
		files { "VanguardEngine/Source/Window/*.h", "VanguardEngine/Source/Window/*.cpp" }
		
	-- Specific Files
		
	filter { "platforms:Win64" }
		files { "VanguardEngine/Source/Asset/Windows/*.h", "VanguardEngine/Source/Asset/Windows/*.cpp" }
		files { "VanguardEngine/Source/Core/Windows/*.h", "VanguardEngine/Source/Core/Windows/*.cpp" }
		files { "VanguardEngine/Source/Debug/Windows/*.h", "VanguardEngine/Source/Debug/Windows/*.cpp" }
		files { "VanguardEngine/Source/Editor/Windows/*.h", "VanguardEngine/Source/Editor/Windows/*.cpp" }
		files { "VanguardEngine/Source/Rendering/Windows/*.h", "VanguardEngine/Source/Rendering/Windows/*.cpp" }
		files { "VanguardEngine/Source/Threading/Windows/*.h", "VanguardEngine/Source/Threading/Windows/*.cpp" }
		files { "VanguardEngine/Source/Utility/Windows/*.h", "VanguardEngine/Source/Utility/Windows/*.cpp" }
		files { "VanguardEngine/Source/Window/Windows/*.h", "VanguardEngine/Source/Window/Windows/*.cpp" }

	filter {}
	
	-- Third Party Files
	
	IncludeThirdParty()
	
	-- General Links
	
	libdirs {}
	
	links { "d3d12", "dxgi", "xinput", "Shcore" }
	
	-- Specific Links
	
	filter { "platforms:Win64" }
		links { "Synchronization" }
		
	filter { "platforms:not Win64" }
		links { "pthread" }
		
	filter {}
	
	postbuildcommands {
		"{COPY} ../../VanguardEngine/ThirdParty/DirectXShaderCompiler/*.dll ../Bin/%{cfg.platform}_%{cfg.buildcfg}/"
	}
	
	-- Third Party Links
	
	LinkThirdParty()
	
	-- Run Third Party Build Scripts
	
	RunThirdParty()