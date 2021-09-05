project "spdlog"
	language "C++"
	kind "StaticLib"
	
	location "../../../Build/ThirdParty/spdlog/Generated"
	buildlog "../../../Build/Logs/SpdlogBuildLog.log"
	objdir "../../../Build/ThirdParty/spdlog/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/spdlog/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "spdlog"
	
	includedirs { "include" }
	
	staticruntime "Off"
	characterset "Unicode"
	
	defines { "SPDLOG_COMPILED_LIB", "SPDLOG_WCHAR_TO_UTF8_SUPPORT", "SPDLOG_NO_EXCEPTIONS" }
	
	files { "include/**.h", "src/*.cpp" }