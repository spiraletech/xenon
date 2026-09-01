cmake_minimum_required(VERSION 3.25)

project(XENON
    VERSION 0.2.0
    DESCRIPTION "XENON native music application"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(xenon_core STATIC
    src/engine.cpp
    src/wav_renderer.cpp
    src/spectrum_analyzer.cpp
    src/cortex.cpp
    src/organic.cpp
    src/producer.cpp
)

target_include_directories(xenon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(xenon_core PUBLIC XENON_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(xenon_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(xenon_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(XENON src/main.cpp)
target_link_libraries(XENON PRIVATE xenon_core)

if(MSVC)
    target_compile_options(XENON PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(XENON PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(xenon_core_smoke tests/CoreSmoke.cpp)
    target_link_libraries(xenon_core_smoke PRIVATE xenon_core)
    add_test(NAME xenon_core_smoke COMMAND xenon_core_smoke)
endif()
