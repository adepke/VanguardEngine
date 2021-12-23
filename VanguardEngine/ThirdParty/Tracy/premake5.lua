function InstallVcpkg()
	prebuildcommands {
		"cd ../../../../VanguardEngine/ThirdParty/Tracy/vcpkg",  -- Use the proper directory.
		"install_vcpkg_dependencies.bat"  -- Install the required packages.
	}
end

project "TracyClient"
	language "C++"
	kind "SharedLib"
	
	location "../../../Build/ThirdParty/TracyClient/Generated"
	buildlog "../../../Build/Logs/TracyClientBuildLog.log"
	objdir "../../../Build/ThirdParty/TracyClient/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/TracyClient/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "TracyClient"
	
	includedirs { "profiler/libs/gl3w", "imgui", "vcpkg/vcpkg/installed/x64-windows-static/include" }
	libdirs { "lib/glfw/bin/win32" }
	
	staticruntime "Off"
	
	defines {
		"_CRT_SECURE_NO_DEPRECATE",
		"_CRT_NONSTDC_NO_DEPRECATE",
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"_USE_MATH_DEFINES",
		"TRACY_EXPORTS"  -- Used for multi-DLL projects
	}
	
	if EnableProfiling then
		defines "TRACY_ENABLE"
	end
	
	files "TracyClient.cpp"
	
	buildoptions "/sdl"  -- Security development lifecycle checks
	
	InstallVcpkg()
	
project "TracyServer"
	language "C++"
	kind "WindowedApp"
	
	location "../../../Build/Tools/TracyServer/Generated"
	buildlog "../../../Build/Logs/TracyServerBuildLog.log"
	objdir "../../../Build/Tools/TracyServer/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/Tools/TracyServer/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "TracyServer"
	
	-- Only build the server in release.
	configmap {
		["Debug"] = "Release",
		["Development"] = "Release"
	}
	
	characterset "MBCS"
	floatingpoint "Fast"
	staticruntime "Off"
	warnings "Extra"
	vectorextensions "AVX2"
	
	buildoptions {
		"/diagnostics:caret",  -- Diagnostics format
		"/permissive-"  -- Enable conformance mode
	}
	
	functionlevellinking "On"
	buildoptions {
		"/GL",  -- Enable whole program optimization
		"/Oi"  -- Enable intrinsic functions
	}
	
	includedirs { "profiler/libs/gl3w", "imgui", "vcpkg/vcpkg/installed/x64-windows-static/include", "vcpkg/vcpkg/installed/x64-windows-static/include/capstone" }
	libdirs "vcpkg/vcpkg/installed/x64-windows-static/lib"
	
	defines {
		"_CRT_SECURE_NO_DEPRECATE",
		"_CRT_NONSTDC_NO_DEPRECATE",
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"_USE_MATH_DEFINES",
		"TRACY_FILESELECTOR",
		"TRACY_EXTENDED_FONT",
		"TRACY_ROOT_WINDOW"
	}
	
	files {
		"common/*.h",
		"common/*.hpp",
		"common/*.cpp",
		"imgui/*.h",
		"imgui/*.cpp",
		"nfd/*.h",
		"nfd/nfd_common.c",
		"server/*.h",
		"server/*.hpp",
		"server/*.cpp",
		"profiler/src/*.h",
		"profiler/src/*.hpp",
		"profiler/src/*.cpp",
		"profiler/libs/gl3w/**.*",
		"zstd/**.h",
		"zstd/**.c"
	}
	
	filter { "system:windows" }
		files "nfd/nfd_win.cpp"
	
	filter { "system:not windows" }
		files "nfd/nfd_gtk.c"
		removefiles "profiler/src/winmain*.*"
		
	filter {}
	
	buildoptions "/sdl"  -- Security development lifecycle checks
	
	links {
		"brotlicommon-static",
		"brotlidec-static",
		"ws2_32",
		"opengl32",
		"glfw3",
		"capstone",
		"freetype",
		"libpng16",
		"zlib",
		"bz2"
	}
	
	InstallVcpkg()