cmake_minimum_required(VERSION 2.6)
project(SenBoy)

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Release)
endif()

set(SenBoy_VERSION_MAJOR 0)
set(SenBoy_VERSION_MINOR 92)
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/ext/Gb_Snd_Emu-0.1.4")
include_directories("${CMAKE_SOURCE_DIR}/ext/imgui")
include_directories("${CMAKE_SOURCE_DIR}/ext/imgui-sfml")

set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} --std=c++14 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-2")
set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} -O2")

set(CMAKE_SHARED_LINKER_FLAGS "-lstdc++fs")
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND WIN32)
	set(CMAKE_EXE_LINKER_FLAGS "-mwindows")
endif()

# Define sources and executable
set(EXECUTABLE_NAME "SenBoy")
set(EXT_SOURCES
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Multi_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Oscs.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Apu.cpp
	
	ext/imgui/imgui.cpp
	ext/imgui/imgui_draw.cpp
	ext/imgui-sfml/imgui-SFML.cpp
)
set(SOURCES
	${EXT_SOURCES}
	src/Tools/Config.cpp
	src/Core/Cartridge.cpp
	src/Core/MMU.cpp
	src/Core/GPU.cpp
	src/Core/LR35902InstrData.cpp
	src/Core/LR35902.cpp
	src/SFMLMain.cpp
)
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Remove warnings for external libraries
set_source_files_properties(
  ${EXT_SOURCES}
  PROPERTIES
  COMPILE_FLAGS "-w"
)

# std::filesystem
if(WIN32)
	target_link_libraries(${EXECUTABLE_NAME} stdc++fs KtmW32)
else()
	target_link_libraries(${EXECUTABLE_NAME} stdc++fs)
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
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
#Find any version 2.X of SFML
#See the FindSFML.cmake file for additional details and instructions
find_package(SFML 2 REQUIRED system window graphics audio)
if(SFML_FOUND)
  include_directories(${SFML_INCLUDE_DIR})
  target_link_libraries(${EXECUTABLE_NAME} ${SFML_LIBRARIES})
endif()

# Install target
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)

add_custom_target(run
    COMMAND SenBoy
    DEPENDS SenBoy
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)
