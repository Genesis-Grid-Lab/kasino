@echo off

rem output directory

set target="Targets\%1"

rem generate cmake build files

cmake -S . -B %target% -DCMAKE_BUILD_TYPE=%1 -DCMAKE_CXX_FLAGS_DEBUG="/MTd" -DCMAKE_C_FLAGS_DEBUG="/MTd" -DUSE_FETCH_GLFW="true" -DEMSCRIPTEN="false"

rem compile cmake build files

cmake --build %target% --config %1