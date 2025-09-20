cmake_minimum_required(VERSION 3.20)

project(sonar VERSION 0.1.0 LANGUAGES CXX)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  message(WARNING "This project is designed for clang; configure with CXX=clang++ if you hit build issues.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
foreach(CONFIG IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CONFIG} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
endforeach()

add_subdirectory(thirdparty/replxx)
add_subdirectory(thirdparty/argparse)
add_subdirectory(thirdparty/googletest)

file(GLOB_RECURSE SONAR_HEADERS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/include/sonar/*.hpp)
file(GLOB_RECURSE SONAR_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.cpp)

add_library(sonar_core ${SONAR_HEADERS} ${SONAR_SOURCES})
add_library(sonar::core ALIAS sonar_core)

target_include_directories(sonar_core
  PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(sonar_core
  PUBLIC
    argparse::argparse
)

if(MSVC)
  target_compile_options(sonar_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(sonar_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

add_executable(sonar app/main.cpp)

target_link_libraries(sonar
  PRIVATE
    sonar::core
    replxx::replxx
)

target_compile_definitions(sonar PRIVATE SONAR_VERSION="${PROJECT_VERSION}")

install(TARGETS sonar RUNTIME DESTINATION bin)

enable_testing()

add_executable(sonar_tests
  test/pretty_printer_test.cpp
)

target_link_libraries(sonar_tests
  PRIVATE
    sonar::core
    gtest_main
)

add_test(NAME sonar_tests COMMAND sonar_tests)
