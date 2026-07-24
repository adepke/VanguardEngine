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

	local src = "../../../../VanguardEngine/ThirdParty/Tracy/profiler"
	local build = "../CMakeBuild"
	local bin = "../Bin"

	buildcommands {
		"cmake -B " .. build .. " -S " .. src .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. build .. " --config Release --parallel",
		"{MKDIR} " .. bin,
		"{COPY} " .. build .. "/Release/tracy-profiler.exe " .. bin .. "/"
	}

	rebuildcommands {
		"cmake -B " .. build .. " -S " .. src .. " -DCMAKE_BUILD_TYPE=Release",
		"cmake --build " .. build .. " --config Release --parallel --clean-first",
		"{MKDIR} " .. bin,
		"{COPY} " .. build .. "/Release/tracy-profiler.exe " .. bin .. "/"
	}

	cleancommands {
		"{RMDIR} " .. build,
		"{RMDIR} " .. bin
	}

	buildoutputs { bin .. "/tracy-profiler.exe" }