@echo off
set curr_dir=%cd%
cd ..
@mkdir gwbuild\dist
cd gwbuild
if "%1" == "test" (
	ctest
) else if "%1" == "pack" (
	cpack -G "%2"
) else if "%1" neq "" (
	echo run example %1.lua
	dist\bin\exlua "%curr_dir%\examples\%1.lua"
) else (
	cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=dist "%curr_dir%"
	cmake --build .
	cmake --build . --target install
)
cd "%curr_dir%"
