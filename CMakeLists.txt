cmake_minimum_required(VERSION 2.6)
project(SenBoy)

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Debug)
endif()

set(SenBoy_VERSION_MAJOR 0)
set(SenBoy_VERSION_MINOR 9)
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/ext/Gb_Snd_Emu-0.1.4")

SET(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} --std=c++11 -Wall")
SET(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-2")
SET(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} -O3 -Ofast")

# Define sources and executable
set(EXECUTABLE_NAME "SenBoy")
set(SOURCES
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Multi_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Oscs.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Apu.cpp

	src/Common.cpp
	src/Cartridge.cpp
	src/MMU.cpp
	src/GPU.cpp
	src/LR35902InstrData.cpp
	src/LR35902.cpp
	src/main.cpp
)
add_executable(${EXECUTABLE_NAME} ${SOURCES})

# Detect and add SFML
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
#Find any version 2.X of SFML
#See the FindSFML.cmake file for additional details and instructions
find_package(SFML 2 REQUIRED system window graphics audio)
if(SFML_FOUND)
  include_directories(${SFML_INCLUDE_DIR})
  target_link_libraries(${EXECUTABLE_NAME} ${SFML_LIBRARIES})
endif()

target_link_libraries(${EXECUTABLE_NAME})
  
# Install target
install(TARGETS ${EXECUTABLE_NAME} DESTINATION bin)

add_custom_target(run
    COMMAND SenBoy
    DEPENDS SenBoy
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)
