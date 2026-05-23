cmake_minimum_required(VERSION 3.20)

project(DigitalRainScreensaver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)

set(PROJECT_GLFW_TARGET "")

find_package(glfw3 CONFIG QUIET)
if(TARGET glfw)
  set(PROJECT_GLFW_TARGET glfw)
elseif(TARGET glfw3)
  set(PROJECT_GLFW_TARGET glfw3)
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
    set(PROJECT_GLFW_TARGET PkgConfig::GLFW3)
  else()
    message(FATAL_ERROR "GLFW was not found. Install GLFW dev files or provide a CMake package.")
  endif()
endif()

add_executable(digital_rain_screensaver
  src/main.cpp
)

set_target_properties(digital_rain_screensaver PROPERTIES
  OUTPUT_NAME "digital-rain-screensaver"
)

if(WIN32)
  set_target_properties(digital_rain_screensaver PROPERTIES
    SUFFIX ".scr"
    WIN32_EXECUTABLE ON
  )
endif()

target_link_libraries(digital_rain_screensaver
  PRIVATE
    ${PROJECT_GLFW_TARGET}
    OpenGL::GL
)

if(WIN32)
  target_link_libraries(digital_rain_screensaver
    PRIVATE
      shell32
      user32
  )
endif()

target_compile_features(digital_rain_screensaver PRIVATE cxx_std_20)

if(MSVC)
  target_compile_options(digital_rain_screensaver PRIVATE /W4 /permissive-)
else()
  target_compile_options(digital_rain_screensaver PRIVATE -Wall -Wextra -Wpedantic)
endif()
