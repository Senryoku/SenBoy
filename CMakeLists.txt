cmake_minimum_required(VERSION 2.6)
project(SenBoy)

if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE Release)
endif()

set(SenBoy_VERSION_MAJOR 0)
set(SenBoy_VERSION_MINOR 91)
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${CMAKE_SOURCE_DIR}/src")
include_directories("${CMAKE_SOURCE_DIR}/src/Core")
include_directories("${CMAKE_SOURCE_DIR}/src/Tools")
include_directories("${CMAKE_SOURCE_DIR}/ext/Gb_Snd_Emu-0.1.4")

set(CMAKE_CXX_FLAGS			"${CMAKE_CXX_FLAGS} --std=c++11 -Wall")
set(CMAKE_CXX_FLAGS_DEBUG	"${CMAKE_CXX_FLAGS_DEBUG} -gdwarf-2")
set(CMAKE_CXX_FLAGS_RELEASE	"${CMAKE_CXX_FLAGS_RELEASE} -O3 -Ofast")

# Define sources and executable
set(EXECUTABLE_NAME "SenBoy")
set(EXT_SOURCES
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Multi_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Blip_Buffer.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Oscs.cpp
	ext/Gb_Snd_Emu-0.1.4/gb_apu/Gb_Apu.cpp
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
