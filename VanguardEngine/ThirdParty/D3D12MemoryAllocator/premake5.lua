project "D3D12MemoryAllocator"
	language "C++"
	kind "StaticLib"
	
	location "../../../Build/ThirdParty/D3D12MemoryAllocator/Generated"
	buildlog "../../../Build/Logs/D3D12MemoryAllocatorBuildLog.log"
	objdir "../../../Build/ThirdParty/D3D12MemoryAllocator/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/D3D12MemoryAllocator/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "D3D12MemoryAllocator"
	
	includedirs "include"
	files { "include/*.h", "src/*.h", "src/*.cpp" }
	
	-- Don't build the sample or tests.
	filter "files:**Sample.*"
		buildaction "None"
	filter "files:**Tests.*"
		buildaction "None"