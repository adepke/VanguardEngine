project "assimp"
	language "C++"
	kind "StaticLib"
	
	-- Always apply optimizations, we don't want to wait 5x longer for mesh loading when we likely never need to debug assimp.
	optimize "Speed"
	omitframepointer "On"
	exceptionhandling "Off"
	flags "NoRuntimeChecks"
	
	location "../../../Build/ThirdParty/assimp/Generated"
	buildlog "../../../Build/Logs/AssimpBuildLog.log"
	objdir "../../../Build/ThirdParty/assimp/Intermediate/%{cfg.platform}_%{cfg.buildcfg}"
	targetdir "../../../Build/ThirdParty/assimp/Bin/%{cfg.platform}_%{cfg.buildcfg}"
	
	targetname "assimp"
	
	defines {
		"ASSIMP_BUILD_NO_X_IMPORTER",
		"ASSIMP_BUILD_NO_AMF_IMPORTER",
		--"ASSIMP_BUILD_NO_3DS_IMPORTER",
		"ASSIMP_BUILD_NO_MD3_IMPORTER",
		"ASSIMP_BUILD_NO_MDL_IMPORTER",
		"ASSIMP_BUILD_NO_MD2_IMPORTER",
		"ASSIMP_BUILD_NO_PLY_IMPORTER",
		"ASSIMP_BUILD_NO_ASE_IMPORTER",
		--"ASSIMP_BUILD_NO_OBJ_IMPORTER",
		"ASSIMP_BUILD_NO_HMP_IMPORTER",
		"ASSIMP_BUILD_NO_SMD_IMPORTER",
		"ASSIMP_BUILD_NO_MDC_IMPORTER",
		"ASSIMP_BUILD_NO_MD5_IMPORTER",
		"ASSIMP_BUILD_NO_STL_IMPORTER",
		"ASSIMP_BUILD_NO_LWO_IMPORTER",
		"ASSIMP_BUILD_NO_DXF_IMPORTER",
		"ASSIMP_BUILD_NO_NFF_IMPORTER",
		"ASSIMP_BUILD_NO_RAW_IMPORTER",
		"ASSIMP_BUILD_NO_SIB_IMPORTER",
		"ASSIMP_BUILD_NO_OFF_IMPORTER",
		"ASSIMP_BUILD_NO_AC_IMPORTER",
		"ASSIMP_BUILD_NO_BVH_IMPORTER",
		"ASSIMP_BUILD_NO_IRRMESH_IMPORTER",
		"ASSIMP_BUILD_NO_IRR_IMPORTER",
		"ASSIMP_BUILD_NO_Q3D_IMPORTER",
		"ASSIMP_BUILD_NO_B3D_IMPORTER",
		"ASSIMP_BUILD_NO_COLLADA_IMPORTER",
		"ASSIMP_BUILD_NO_TERRAGEN_IMPORTER",
		"ASSIMP_BUILD_NO_CSM_IMPORTER",
		"ASSIMP_BUILD_NO_3D_IMPORTER",
		"ASSIMP_BUILD_NO_LWS_IMPORTER",
		"ASSIMP_BUILD_NO_OGRE_IMPORTER",
		"ASSIMP_BUILD_NO_OPENGEX_IMPORTER",
		"ASSIMP_BUILD_NO_MS3D_IMPORTER",
		"ASSIMP_BUILD_NO_COB_IMPORTER",
		"ASSIMP_BUILD_NO_BLEND_IMPORTER",
		"ASSIMP_BUILD_NO_Q3BSP_IMPORTER",
		"ASSIMP_BUILD_NO_NDO_IMPORTER",
		"ASSIMP_BUILD_NO_IFC_IMPORTER",
		"ASSIMP_BUILD_NO_XGL_IMPORTER",
		--"ASSIMP_BUILD_NO_FBX_IMPORTER",
		"ASSIMP_BUILD_NO_ASSBIN_IMPORTER",
		"ASSIMP_BUILD_NO_GLTF_IMPORTER",
		"ASSIMP_BUILD_NO_C4D_IMPORTER",
		"ASSIMP_BUILD_NO_3MF_IMPORTER",
		"ASSIMP_BUILD_NO_X3D_IMPORTER",
		"ASSIMP_BUILD_NO_MMD_IMPORTER",
		"ASSIMP_BUILD_NO_STEP_IMPORTER"
	}
	
	includedirs { "include", "code", "" }
	
	-- Third Party Includes
	
	includedirs {
		"contrib/irrXML",
		"contrib/rapidjson/include",
		"contrib/unzip",
		"contrib/zlib"
	}
	
	-- Core Files
	
	files {
		"*.h",
		"include/assimp/*.h", "include/assimp/*.hpp", "include/assimp/*.c", "include/assimp/*.cpp",
		"code/Common/*",
		"code/PostProcessing/*",
		"code/Material/*"
	}
	
	-- Loaders
	
	files {
		"code/FBX/*",
		"code/3DS/*",
		"code/Obj/*"
	}
	
	-- Third Party
	
	files {
		"contrib/irrXML/*.h",
		"contrib/irrXML/*.cpp",
		"contrib/rapidjson/include/rapidjson/*",
		"contrib/rapidjson/include/rapidjson/**/*",
		"contrib/unzip/*",
		"contrib/zlib/*.h",
		"contrib/zlib/*.c"
	}