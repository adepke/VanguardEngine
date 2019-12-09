project "Jobs"
	language "C++"
	cppdialect "C++17"
	kind "StaticLib"
	
	location "../../../Build/ThirdParty/Jobs/Generated"
	buildlog "../../../Build/Logs/JobsBuildLog.log"
	objdir "../../../Build/ThirdParty/Jobs/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/Jobs/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "Jobs"
	
	includedirs { "Jobs/Include", "Jobs/Source", "Jobs/ThirdParty" }
	files { "**.h", "**.cpp" }
	
	disablewarnings { "5051" }