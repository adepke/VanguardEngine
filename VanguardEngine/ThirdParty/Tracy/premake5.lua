project "TracyClient"
	language "C++"
	kind "SharedLib"

	location "../../../Build/ThirdParty/TracyClient/Generated"
	buildlog "../../../Build/Logs/TracyClientBuildLog.log"
	objdir "../../../Build/ThirdParty/TracyClient/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/TracyClient/Bin/%{cfg.platform}_%{cfg.buildcfg}"

	targetname "TracyClient"

	includedirs "public"

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

	editandcontinue "Off"  -- Disable edit and continue, since Tracy needs __LINE__ to be a constant.

	files "public/TracyClient.cpp"

	buildoptions "/sdl"  -- Security development lifecycle checks

-- Note: no longer self-contained as Tracy dropped support for vcpkg. Requires CMake >= 3.25 on PATH.
project "TracyServer"
	kind "Makefile"

	location "../../../Build/Tools/TracyServer/Generated"
	targetdir "../../../Build/Tools/TracyServer/Bin"
	targetname "TracyServer"

	-- CMake source dirs for each tool.
	local srcProfiler  = "../../../../VanguardEngine/ThirdParty/Tracy/profiler"
	local srcCapture   = "../../../../VanguardEngine/ThirdParty/Tracy/capture"
	local srcCsvexport = "../../../../VanguardEngine/ThirdParty/Tracy/csvexport"

	-- Separate out-of-source build trees so the three CMake projects don't clobber each other.
	local buildProfiler  = "../CMakeBuild"
	local buildCapture   = "../CMakeBuildCapture"
	local buildCsvexport = "../CMakeBuildCsvexport"
	local bin = "../Bin"

	-- The GUI profiler, the headless capture tool, and the CSV exporter all live in Bin/ together.
	buildcommands {
		"cmake -B " .. buildProfiler  .. " -S " .. srcProfiler  .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. buildProfiler  .. " --config Release --parallel",
		"cmake -B " .. buildCapture   .. " -S " .. srcCapture   .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. buildCapture   .. " --config Release --parallel",
		"cmake -B " .. buildCsvexport .. " -S " .. srcCsvexport .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. buildCsvexport .. " --config Release --parallel",
		"{MKDIR} " .. bin,
		"{COPY} " .. buildProfiler  .. "/Release/tracy-profiler.exe "  .. bin .. "/",
		"{COPY} " .. buildCapture   .. "/Release/tracy-capture.exe "   .. bin .. "/",
		"{COPY} " .. buildCsvexport .. "/Release/tracy-csvexport.exe " .. bin .. "/"
	}

	rebuildcommands {
		"cmake -B " .. buildProfiler  .. " -S " .. srcProfiler  .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. buildProfiler  .. " --config Release --parallel --clean-first",
		"cmake -B " .. buildCapture   .. " -S " .. srcCapture   .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. buildCapture   .. " --config Release --parallel --clean-first",
		"cmake -B " .. buildCsvexport .. " -S " .. srcCsvexport .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. buildCsvexport .. " --config Release --parallel --clean-first",
		"{MKDIR} " .. bin,
		"{COPY} " .. buildProfiler  .. "/Release/tracy-profiler.exe "  .. bin .. "/",
		"{COPY} " .. buildCapture   .. "/Release/tracy-capture.exe "   .. bin .. "/",
		"{COPY} " .. buildCsvexport .. "/Release/tracy-csvexport.exe " .. bin .. "/"
	}

	cleancommands {
		"{RMDIR} " .. buildProfiler,
		"{RMDIR} " .. buildCapture,
		"{RMDIR} " .. buildCsvexport,
		"{RMDIR} " .. bin
	}

	buildoutputs {
		bin .. "/tracy-profiler.exe",
		bin .. "/tracy-capture.exe",
		bin .. "/tracy-csvexport.exe"
	}