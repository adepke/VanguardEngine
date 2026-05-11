project "sqlite"
	language "C++"
	kind "StaticLib"

	location "../../../Build/ThirdParty/sqlite/Generated"
	buildlog "../../../Build/Logs/SqliteBuildLog.log"
	objdir "../../../Build/ThirdParty/sqlite/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/sqlite/Bin/%{cfg.platform}_%{cfg.buildcfg}"

	targetname "sqlite"

	staticruntime "Off"

	files { "*.c", "*.h" }