cmake_minimum_required(VERSION 3.20)
project(SenBoy)
set(SenBoy_VERSION_MAJOR 1)
set(SenBoy_VERSION_MINOR 0)

option(WITH_DISCORD_RPC "Enable Discord Rich Presence" ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Release)
endif()

set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")

include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/ext/Gb_Snd_Emu-0.1.4")
include_directories("${CMAKE_SOURCE_DIR}/ext/imgui")
include_directories("${CMAKE_SOURCE_DIR}/ext/imgui-sfml")
include_directories("${CMAKE_SOURCE_DIR}/ext/miniz")
if(WITH_DISCORD_RPC)
	add_definitions(-DUSE_DISCORD_RPC)
	add_subdirectory("${CMAKE_SOURCE_DIR}/ext/discord-rpc/")
	include_directories("${CMAKE_SOURCE_DIR}/ext/discord-rpc/include")
endif()

if (MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()
add_compile_definitions(BLARGG_USE_NAMESPACE=1)

# Statically link standard library
set(STATIC_STD_LIBRARIES OFF CACHE BOOL "Statically link STD Libs")
#if(STATIC_STD_LIBRARIES)
#	set(CMAKE_SHARED_LINKER_FLAGS "-static-libgcc -static-libstdc++")
#endif()

# Define sources and executable
set(EXECUTABLE_NAME "SenBoy")
set(APU_SOURCES
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Multi_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Oscs.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Apu.cpp
)
set(IMGUI_SOURCES
	ext/imgui/imgui.cpp
	ext/imgui/imgui_draw.cpp
	ext/imgui/imgui_tables.cpp
	ext/imgui/imgui_widgets.cpp
	ext/imgui-sfml/imgui-SFML.cpp
)
set(MINIZ_SOURCES
	ext/miniz/miniz.c
)
set(SOURCES
	${APU_SOURCES}
	src/Tools/Config.cpp
	src/Core/Cartridge.cpp
	src/Core/MMU.cpp
	src/Core/GPU.cpp
	src/Core/LR35902InstrData.cpp
	src/Core/LR35902.cpp
)
add_executable(${EXECUTABLE_NAME} ${SOURCES} ${IMGUI_SOURCES} ${MINIZ_SOURCES} src/SFMLMain.cpp)

add_executable(PerfTest ${SOURCES} test/PerfTest.cpp)
add_executable(CPUPerfTest ${SOURCES} test/CPUPerfTest.cpp)
add_executable(Screenshot ${SOURCES} ${MINIZ_SOURCES} test/Screenshot.cpp)

set_property(TARGET ${EXECUTABLE_NAME} PROPERTY CXX_STANDARD 20)
set_property(TARGET PerfTest PROPERTY CXX_STANDARD 20)
set_property(TARGET CPUPerfTest PROPERTY CXX_STANDARD 20)
set_property(TARGET Screenshot PROPERTY CXX_STANDARD 20)
set_target_properties(${EXECUTABLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(PerfTest PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(CPUPerfTest PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
set_target_properties(Screenshot PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
	
# Hide console on windows for release build
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND WIN32)
	target_link_libraries(${EXECUTABLE_NAME} "-mwindows")
endif()

# Remove warnings for external libraries
if (MSVC)
set(NO_WARNING "/w")
else()
set(NO_WARNING "-w")
endif()
set_source_files_properties(
  "ext/"
  PROPERTIES
  COMPILE_FLAGS ${NO_WARNING}
)

find_package(OpenGL REQUIRED)
include_directories(${OpenGL_INCLUDE_DIRS})
link_directories(${OpenGL_LIBRARY_DIRS})
add_definitions(${OpenGL_DEFINITIONS})
if(NOT OPENGL_FOUND)
    message(ERROR " OPENGL not found!")
endif(NOT OPENGL_FOUND)
target_link_libraries(${EXECUTABLE_NAME} ${OPENGL_LIBRARIES})

# Detect and add SFML
# SFML_USE_STATIC_STD_LIBS
#set(SFML_STATIC_LIBRARIES OFF CACHE BOOL "Statically link SFML")
# Manually statically link pthreads
#if(SFML_STATIC_LIBRARIES)
#	target_link_libraries(${EXECUTABLE_NAME} -static -pthread)
#endif()
set(BUILD_SHARED_LIBS FALSE)
add_subdirectory("ext/SFML-2.5.1")
include_directories(${SFML_INCLUDE_DIR})
target_link_libraries(${EXECUTABLE_NAME} sfml-system sfml-window sfml-graphics sfml-audio ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})

if(WITH_DISCORD_RPC)
	target_link_libraries(${EXECUTABLE_NAME} discord-rpc)
endif()
  
# Install target
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)

add_custom_target(run
    COMMAND ${EXECUTABLE_NAME}
    DEPENDS ${EXECUTABLE_NAME}
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)
