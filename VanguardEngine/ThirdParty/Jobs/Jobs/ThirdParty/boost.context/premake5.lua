-- Normally this would be separated into a dedicated project, however, premake5 is currently not able to automatically link
-- with custom build outputs generated from one project, in another. https://github.com/premake/premake-core/issues/1561

linkbuildoutputs "On"

filter { "system:windows", "architecture:x86_64" }
	files { "src/asm/jump_x86_64_ms_pe_masm.*", "src/asm/make_x86_64_ms_pe_masm.*" }

filter { "system:linux", "architecture:x86_64" }
	files { "src/asm/jump_x86_64_sysv_elf_gas.*", "src/asm/make_x86_64_sysv_elf_gas.*" }

filter { "files:**.asm", "system:windows" }
	buildmessage "Assembling boost.context fiber routine: %{file.name}"
	buildcommands "ml64.exe /c /Fo %{cfg.objdir}/%{file.basename} /D BOOST_CONTEXT_EXPORT=EXPORT %{file.relpath}"
	buildoutputs "%{cfg.objdir}/%{file.basename}.obj"

filter { "files:**.[sS]", "system:linux" }
	buildmessage "Assembling boost.context fiber routine: %{file.name}"
	buildcommands "gcc -c %{file.relpath}"
	buildoutputs "%{cfg.objdir}/%{file.basename}.o"

filter {}