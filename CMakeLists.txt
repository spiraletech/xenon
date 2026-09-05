cmake_minimum_required(VERSION 3.25)
project(XENON VERSION 1.0.0 DESCRIPTION "XENON native AI Music OS" LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)

option(XENON_ENABLE_VST3_SDK "Enable Steinberg VST3 SDK native hosting" ON)
if(XENON_ENABLE_VST3_SDK)
 include(FetchContent)
 set(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES OFF CACHE BOOL "" FORCE)
 set(SMTG_ENABLE_VST3_HOSTING_EXAMPLES OFF CACHE BOOL "" FORCE)
 set(SMTG_ENABLE_VSTGUI_SUPPORT OFF CACHE BOOL "" FORCE)
 set(SMTG_CREATE_PLUGIN_LINK OFF CACHE BOOL "" FORCE)
 FetchContent_Declare(vst3sdk
  GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
  GIT_TAG 9fad9770f2ae8542ab1a548a68c1ad1ac690abe0
  GIT_SUBMODULES_RECURSE TRUE
 )
 FetchContent_MakeAvailable(vst3sdk)
endif()

add_library(xenon_core STATIC
 src/engine.cpp src/wav_renderer.cpp src/spectrum_analyzer.cpp src/media_analyzer.cpp src/player_engine.cpp src/cortex.cpp src/cortex_critic.cpp src/organic.cpp src/organic_music_memory.cpp src/producer.cpp src/backend_policy.cpp src/model_router.cpp src/ether_control.cpp src/ether_composer.cpp src/composer_agent.cpp src/musician_agents.cpp src/realtime_jam_engine.cpp src/vst_host.cpp src/vst3_native_host.cpp src/midi_intelligence.cpp src/native_midi_input.cpp src/trinity_device_adapter.cpp src/runtime_backend_manager.cpp src/mix_engine.cpp src/mastering_engine.cpp src/project_graph.cpp src/session_os.cpp src/trinity_device_bus.cpp src/autonomous_producer.cpp src/music_os.cpp src/sovereign_runtime.cpp src/sovereign_runtime_config.cpp src/ether_dna.cpp src/ether_dna_store.cpp src/generation_pipeline.cpp src/synesthesia_scorer.cpp src/synesthesia_engine.cpp src/etherhue_dsp.cpp src/revision_compiler.cpp src/self_listening_loop.cpp src/originality_guard.cpp src/candidate_ranker.cpp src/candidate_swarm_engine.cpp src/stem_types.cpp src/stem_analyzer.cpp src/stem_engine.cpp src/surgical_revision.cpp src/surgical_revision_engine.cpp src/backends/native_preview_backend.cpp)
target_include_directories(xenon_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(xenon_core PUBLIC XENON_VERSION="${PROJECT_VERSION}")
if(XENON_ENABLE_VST3_SDK)
 target_compile_definitions(xenon_core PUBLIC XENON_VST3_SDK_ENABLED=1)
 target_sources(xenon_core PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/plugprovider.cpp)
 if(SMTG_WIN)
  set(XENON_VST3_PLATFORM_SOURCE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_win32.cpp)
  target_sources(xenon_core PRIVATE ${XENON_VST3_PLATFORM_SOURCE})
  if(MSVC)
   set_source_files_properties(${XENON_VST3_PLATFORM_SOURCE} PROPERTIES COMPILE_OPTIONS "/Zc:char8_t-")
  endif()
 elseif(SMTG_MAC)
  target_sources(xenon_core PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_mac.mm)
 elseif(SMTG_LINUX)
  target_sources(xenon_core PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/hosting/module_linux.cpp)
 endif()
 target_link_libraries(xenon_core PUBLIC sdk_hosting)
endif()
if(WIN32)
 target_link_libraries(xenon_core PUBLIC ole32 oleaut32 mfplat mfplay mfreadwrite mfuuid propsys winhttp winmm)
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
 foreach(test_pair
 "core;CoreSmoke.cpp" "model_router;ModelRouterSmoke.cpp" "generation_pipeline;GenerationPipelineSmoke.cpp" "native_preview_backend;NativePreviewBackendSmoke.cpp" "backend_policy;BackendPolicySmoke.cpp" "self_listening_loop;SelfListeningLoopSmoke.cpp" "candidate_swarm;CandidateSwarmSmoke.cpp" "originality_guard;OriginalityGuardSmoke.cpp" "stem_intelligence;StemIntelligenceSmoke.cpp" "surgical_revision;SurgicalRevisionSmoke.cpp" "ether_dna_v2;EtherDNAV2Smoke.cpp" "organic_music_memory;OrganicMusicMemorySmoke.cpp" "synesthesia_engine_v2;SynesthesiaEngineV2Smoke.cpp" "etherhue_dsp;EtherHueDSPSmoke.cpp" "composer_agent;ComposerAgentSmoke.cpp" "musician_agents;MusicianAgentsSmoke.cpp" "realtime_jam_engine;RealtimeJamEngineSmoke.cpp" "vst_host;VstHostSmoke.cpp" "midi_intelligence;MidiIntelligenceSmoke.cpp" "mix_engine;MixEngineSmoke.cpp" "mastering_engine;MasteringEngineSmoke.cpp" "project_graph;ProjectGraphSmoke.cpp" "trinity_device_bus;TrinityDeviceBusSmoke.cpp" "autonomous_producer;AutonomousProducerSmoke.cpp" "music_os;MusicOSSmoke.cpp" "sovereign_runtime;SovereignRuntimeSmoke.cpp" "runtime_systems;RuntimeSystemsSmoke.cpp")
  list(GET test_pair 0 test_name)
  list(GET test_pair 1 test_file)
  add_executable(xenon_${test_name}_smoke tests/${test_file})
  target_link_libraries(xenon_${test_name}_smoke PRIVATE xenon_core)
  add_test(NAME xenon_${test_name}_smoke COMMAND xenon_${test_name}_smoke)
 endforeach()

 if(XENON_ENABLE_VST3_SDK)
  set(XENON_ADELAY_DIR ${vst3sdk_SOURCE_DIR}/public.sdk/samples/vst/adelay)
  set(XENON_ADELAY_GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/l41_adelay_generated)
  file(MAKE_DIRECTORY ${XENON_ADELAY_GENERATED_DIR})
  file(WRITE ${XENON_ADELAY_GENERATED_DIR}/projectversion.h
"#pragma once\n#define MAJOR_VERSION_INT 1\n#define SUB_VERSION_INT 0\n#define RELEASE_NUMBER_INT 0\n#define BUILD_NUMBER_INT 0\n#define MAJOR_VERSION_STR \"1\"\n#define SUB_VERSION_STR \"0\"\n#define RELEASE_NUMBER_STR \"0\"\n#define BUILD_NUMBER_STR \"0\"\n#define FULL_VERSION_STR \"1.0.0.0\"\n")
  smtg_add_vst3plugin(xenon_l41_adelay
   ${XENON_ADELAY_DIR}/source/adelaycontroller.cpp
   ${XENON_ADELAY_DIR}/source/adelaycontroller.h
   ${XENON_ADELAY_DIR}/source/adelayids.h
   ${XENON_ADELAY_DIR}/source/adelayprocessor.cpp
   ${XENON_ADELAY_DIR}/source/adelayprocessor.h
   ${XENON_ADELAY_DIR}/source/exampletest.cpp
   ${XENON_ADELAY_DIR}/source/factory.cpp
   ${XENON_ADELAY_DIR}/source/version.h
   ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/test/ringbuffertest.cpp
   ${vst3sdk_SOURCE_DIR}/public.sdk/source/vst/utility/test/versionparsertest.cpp
  )
  target_include_directories(xenon_l41_adelay PRIVATE ${vst3sdk_SOURCE_DIR} ${XENON_ADELAY_GENERATED_DIR})
  target_link_libraries(xenon_l41_adelay PRIVATE sdk)
  if(SMTG_WIN)
   target_sources(xenon_l41_adelay PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/dllmain.cpp)
  elseif(SMTG_MAC)
   target_sources(xenon_l41_adelay PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/macmain.cpp)
  elseif(SMTG_LINUX)
   target_sources(xenon_l41_adelay PRIVATE ${vst3sdk_SOURCE_DIR}/public.sdk/source/main/linuxmain.cpp)
  endif()

  add_executable(xenon_vst3_native_host_smoke tests/Vst3NativeHostSmoke.cpp)
  target_link_libraries(xenon_vst3_native_host_smoke PRIVATE xenon_core)
  target_compile_definitions(xenon_vst3_native_host_smoke PRIVATE XENON_TEST_VST3_PATH="$<TARGET_FILE:xenon_l41_adelay>")
  add_dependencies(xenon_vst3_native_host_smoke xenon_l41_adelay)
  add_test(NAME xenon_vst3_native_host_smoke COMMAND xenon_vst3_native_host_smoke)
 endif()
endif()
