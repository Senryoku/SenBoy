cmake_minimum_required(VERSION 2.6)
project(SenBoy)
set(SenBoy_VERSION_MAJOR 1)
set(SenBoy_VERSION_MINOR 0)

option(WITH_DISCORD_RPC "Enable Discord Rich Presence" ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Release)
endif()

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

set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} --std=c++14 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} -Og -gdwarf-2")
set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} -O2 -s")

# Statically link standard library
set(STATIC_STD_LIBRARIES OFF CACHE BOOL "Statically link STD Libs")
if(STATIC_STD_LIBRARIES)
	set(CMAKE_SHARED_LINKER_FLAGS "-lstdc++fs -static-libgcc -static-libstdc++")
endif()

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

add_executable(CPUPerfTest ${SOURCES} test/CPUPerfTest.cpp)
add_executable(Screenshot ${SOURCES} ${MINIZ_SOURCES} test/Screenshot.cpp)
	
# Hide console on windows for release build
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND WIN32)
	target_link_libraries(${EXECUTABLE_NAME} "-mwindows")
endif()

# Remove warnings for external libraries
set_source_files_properties(
  ${EXT_SOURCES}
  PROPERTIES
  COMPILE_FLAGS "-w"
)

# std::filesystem
if(WIN32)
	target_link_libraries(${EXECUTABLE_NAME} stdc++fs KtmW32)
	target_link_libraries(Screenshot stdc++fs KtmW32)
else()
	target_link_libraries(${EXECUTABLE_NAME} stdc++fs)
	target_link_libraries(Screenshot stdc++fs)
endif()

find_package(OpenGL REQUIRED)
include_directories(${OpenGL_INCLUDE_DIRS})
link_directories(${OpenGL_LIBRARY_DIRS})
add_definitions(${OpenGL_DEFINITIONS})
if(NOT OPENGL_FOUND)
    message(ERROR " OPENGL not found!")
endif(NOT OPENGL_FOUND)
target_link_libraries(${EXECUTABLE_NAME} ${OPENGL_LIBRARIES})

# Detect and add SFML
set(SFML_ROOT "D:/Source/_Others/SFML" CACHE PATH "SFML root folder")
set(SFML_STATIC_LIBRARIES OFF CACHE BOOL "Statically link SFML")
# Manually statically link pthreads
if(SFML_STATIC_LIBRARIES)
	target_link_libraries(${EXECUTABLE_NAME} -static -pthread)
endif()
#Find any version 2.X of SFML
#See the FindSFML.cmake file for additional details and instructions
find_package(SFML 2 REQUIRED system window graphics audio)
if(SFML_FOUND)
  include_directories(${SFML_INCLUDE_DIR})
  target_link_libraries(${EXECUTABLE_NAME} ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})
endif()

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
