@echo off
set curr_dir=%cd%
cd ..
@mkdir vcbuild\dist
cd vcbuild
if "%1" == "test" (
	ctest
) else if "%1" == "pack" (
	cpack -G "%2"
) else if "%1" neq "" (
	echo run example %1.lua
	dist\bin\lua "%curr_dir%\examples\%1.lua"
) else (
	cmake -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=dist "%curr_dir%"
	nmake install
)
cd "%curr_dir%"
