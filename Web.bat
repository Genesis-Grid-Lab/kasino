@echo off

rem output directory

set target="Targets-web\%1"

rem generate cmake build files

emcmake cmake -S . -B %target% -DCMAKE_BUILD_TYPE=%1 -DEMSCRIPTEN="true" -G Ninja

rem compile cmake build files

cmake --build %target% -j