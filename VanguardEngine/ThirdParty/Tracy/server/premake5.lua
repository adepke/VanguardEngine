project "TracyServer"
	language "C++"
	kind "WindowedApp"
	
	-- Only build the server in release.
	configmap {
		["Debug"] = "Release",
		["Development"] = "Release"
	}
	
	location "../../../../Build/Tools/TracyServer/Generated"
	buildlog "../../../../Build/Logs/TracyServerBuildLog.log"
	objdir "../../../../Build/Tools/TracyServer/Intermediate/%{cfg.platform}"
	targetdir "../../../../Build/Tools/TracyServer/Bin/%{cfg.platform}"
	
	targetname "TracyServer"
	
	includedirs { "../profiler/libs/gl3w", "../imgui", "../lib/freetype/include", "../lib/glfw/include" }
	libdirs { "../lib/bzip2/bin/win32", "../lib/freetype/bin/win32", "../lib/glfw/bin/win32", "../lib/libpng/bin/win32", "../lib/zlib/bin/win32" }
	
	optimize "Speed"
	characterset "MBCS"
	floatingpoint "Fast"
	staticruntime "Off"
	flags { "MultiProcessorCompile" }
		
	filter {}
	
	defines {
		"TRACY_FILESELECTOR",
		"TRACY_EXTENDED_FONT",
		"TRACY_ROOT_WINDOW",
		"_CRT_SECURE_NO_DEPRECATE",
		"_CRT_NONSTDC_NO_DEPRECATE",
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"_USE_MATH_DEFINES",
		"NDEBUG"
	}
	
	files {
		"../common/TracyAlign.hpp",
		"../common/TracyForceInline.hpp",
		"../common/TracyMutex.hpp",
		"../common/TracyProtocol.hpp",
		"../common/TracyQueue.hpp",
		"../common/TracySocket.hpp",
		"../common/TracySystem.hpp",
		"../common/tracy_benaphore.h",
		"../common/tracy_lz4.hpp",
		"../common/tracy_lz4hc.hpp",
		"../common/tracy_sema.h",
		"../common/*.cpp",
		"../imgui/*.*",
		"../imguicolortextedit/*.*",
		"../nfd/*.h",
		"../nfd/nfd_common.c",
		"../nfd/nfd_win.cpp",
		"*.hpp",
		"*.cpp",
		"IconsFontAwesome5.h",
		"tracy_pdqsort.h",
		"tracy_xxh3.h",
		"../profiler/libs/**.*",
		"../profiler/src/*.*",
	}
		
	filter {}
		
	links { "ws2_32", "opengl32", "bz2", "freetype", "glfw3", "libpng16", "zlib" }