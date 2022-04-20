project "meshoptimizer"
	language "C++"
	kind "StaticLib"
	
	location "../../../Build/ThirdParty/meshoptimizer/Generated"
	buildlog "../../../Build/Logs/MeshoptimizerBuildLog.log"
	objdir "../../../Build/ThirdParty/meshoptimizer/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/meshoptimizer/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "meshoptimizer"
	
	staticruntime "Off"
	
	files { "src/*.cpp", "src/*.h" }