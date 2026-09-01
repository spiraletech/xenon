cmake_minimum_required(VERSION 3.25)

project(XENON
    VERSION 0.4.0
    DESCRIPTION "XENON fused native AI music workstation"
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
    src/media_analyzer.cpp
    src/player_engine.cpp
    src/cortex.cpp
    src/organic.cpp
    src/producer.cpp
    src/model_router.cpp
    src/ether_control.cpp
    src/ether_composer.cpp
    src/ether_dna.cpp
    src/generation_pipeline.cpp
)

target_include_directories(xenon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(xenon_core PUBLIC XENON_VERSION="${PROJECT_VERSION}")

if(WIN32)
    target_link_libraries(xenon_core PUBLIC
        ole32
        oleaut32
        mfplat
        mfplay
        mfreadwrite
        mfuuid
        propsys
    )
endif()

if(MSVC)
    target_compile_options(xenon_core PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(xenon_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_executable(XENON WIN32 src/win32_app.cpp)
    target_link_libraries(XENON PRIVATE xenon_core comdlg32 gdi32 user32)
else()
    add_executable(XENON src/main.cpp)
    target_link_libraries(XENON PRIVATE xenon_core)
endif()

if(MSVC)
    target_compile_options(XENON PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(XENON PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(xenon_core_smoke tests/CoreSmoke.cpp)
    target_link_libraries(xenon_core_smoke PRIVATE xenon_core)
    add_test(NAME xenon_core_smoke COMMAND xenon_core_smoke)

    add_executable(xenon_model_router_smoke tests/ModelRouterSmoke.cpp)
    target_link_libraries(xenon_model_router_smoke PRIVATE xenon_core)
    add_test(NAME xenon_model_router_smoke COMMAND xenon_model_router_smoke)

    add_executable(xenon_generation_pipeline_smoke tests/GenerationPipelineSmoke.cpp)
    target_link_libraries(xenon_generation_pipeline_smoke PRIVATE xenon_core)
    add_test(NAME xenon_generation_pipeline_smoke COMMAND xenon_generation_pipeline_smoke)
endif()
