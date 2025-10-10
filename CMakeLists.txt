cmake_minimum_required(VERSION 3.5)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# build output directories

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY $<0:>${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<0:>${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY $<0:>${CMAKE_BINARY_DIR}/lib)
set(CMAKE_PDB_OUTPUT_DIRECTORY $<0:>${CMAKE_BINARY_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH $<0:>${CMAKE_BINARY_DIR}/bin)

project(Kasino LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
set(WINDOW_BACKEND "glfw" CACHE STRING "Window backend: glfw or native")
option(USE_FECTH_GLFW "Fetch GLFW via FetchContent" ON)

if(WINDOW_BACKEND STREQUAL "glfw")
  file(GLOB_RECURSE nativeFile)
  file(GLOB_RECURSE androidFile)
  file(GLOB_RECURSE iosFile)
  file(GLOB_RECURSE glfwFile ${CMAKE_CURRENT_SOURCE_DIR}/src/window/glfw/*.cpp)
elseif(WINDOW_BACKEND STREQUAL "native")
  file(GLOB_RECURSE nativeFile ${CMAKE_CURRENT_SOURCE_DIR}/src/window/native/*.cpp)
  file(GLOB_RECURSE glfwFile)
  file(GLOB_RECURSE androidFile)
  file(GLOB_RECURSE iosFile)
elseif(WINDOW_BACKEND STREQUAL "android")
  file(GLOB_RECURSE nativeFile)
  file(GLOB_RECURSE glfwFile)
  file(GLOB_RECURSE androidFile  ${CMAKE_CURRENT_SOURCE_DIR}/src/window/android/*.cpp)
  file(GLOB_RECURSE iosFile)
elseif(WINDOW_BACKEND STREQUAL "ios")
  file(GLOB_RECURSE nativeFile)
  file(GLOB_RECURSE glfwFile)
  file(GLOB_RECURSE androidFile)
  file(GLOB_RECURSE iosFile  ${CMAKE_CURRENT_SOURCE_DIR}/src/window/ios/*.cpp)
else()
  message(FATAL_ERROR "Unknown WINDOW_BACKEND: ${WINDOW_BACKEND}")
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/spdlog)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio)

file(GLOB_RECURSE source ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp)
file(GLOB_RECURSE headers ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h)
file(GLOB gladFile ${CMAKE_CURRENT_SOURCE_DIR}/src/glad/*.cpp)
file(GLOB_RECURSE miniaudioFile ${CMAKE_CURRENT_SOURCE_DIR}/src/miniaudio/*.cpp)

add_library(engine
    ${source} ${headers}
    ${glfwFile}
    ${nativeFile}
    ${iosFile}
    ${androidFile}
    ${gladFile}
    ${miniaudioFile}
    )

target_include_directories(engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/include
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/glm
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb_image
  ${CMAKE_SOURCE_DIR}/vendor/spdlog/include
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor/miniaudio
)

# Backend selection
if (WINDOW_BACKEND STREQUAL "glfw")
    target_compile_definitions(engine PUBLIC WINDOW_BACKEND_GLFW=1)
    if (USE_FETCH_GLFW)
        include(FetchContent)
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
        )
        FetchContent_MakeAvailable(glfw)
        target_link_libraries(engine PUBLIC
	  glfw
	  glad
	  glm
	  spdlog
    miniaudio
	)
    else()
        find_package(glfw3 REQUIRED)
        target_link_libraries(engine PUBLIC
	  glfw
	  glad
	  glm
	  spdlog
    miniaudio
	)
    endif()
elseif (WINDOW_BACKEND STREQUAL "native")
  target_compile_definitions(engine PUBLIC WINDOW_BACKEND_NATIVE=1)
  target_link_libraries(engine PUBLIC
    glad
    glm
    spdlog
    miniaudio
  )
elseif(WINDOW_BACKEND STREQUAL "android")
  target_compile_definitions(engine PUBLIC WINDOW_BACKEND_ANDROID=1)
  target_link_libraries(engine PUBLIC
    glad
    glm
    spdlog
  )
elseif(WINDOW_BACKEND STREQUAL "ios")
  target_compile_definitions(engine PUBLIC WINDOW_BACKEND_IOS=1)
  target_link_libraries(engine PUBLIC
    glad
    glm
    spdlog
  )
else()
    message(FATAL_ERROR "Unknown WINDOW_BACKEND: ${WINDOW_BACKEND}. Use glfw or native.")
endif()


# Linux specifics (GLFW needs this; safe otherwise)
if (UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(engine PUBLIC Threads::Threads dl X11 Xrandr Xi Xxf86vm Xcursor)
  endif()

file(GLOB_RECURSE Kasino ${CMAKE_CURRENT_SOURCE_DIR}/src/Kasino/*.cpp)  

add_executable(${PROJECT_NAME}
  src/main.cpp
  # src/testing.cpp
  ${Kasino}
)

target_link_libraries(${PROJECT_NAME} PRIVATE engine)


# copy data
if(EXISTS ${CMAKE_SOURCE_DIR}/Data)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND 
        ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_SOURCE_DIR}/Data
        ${EXECUTABLE_OUTPUT_PATH}/Data
    )
else()
    message(WARNING "[WARNING] no data directory!")
  endif()

# copy data
if(EXISTS ${CMAKE_SOURCE_DIR}/Resources)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND 
        ${CMAKE_COMMAND} -E copy_directory 
        ${CMAKE_SOURCE_DIR}/Resources
        # ${EXECUTABLE_OUTPUT_PATH}/Resources
	$<TARGET_FILE_DIR:${PROJECT_NAME}>/Resources
    )
else()
    message(WARNING "[WARNING] no resources directory!")
endif()

