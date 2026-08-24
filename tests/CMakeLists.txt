cmake_minimum_required(VERSION 3.25)
find_package(Python3 COMPONENTS Interpreter QUIET)
add_library(doctest INTERFACE)
target_include_directories(doctest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cpp)

set(TEST_SOURCES
    cpp/test_main.cpp
    cpp/test_core.cpp
    cpp/test_debug_manager_edge.cpp
    cpp/test_carc.cpp
    cpp/test_carc_debug.cpp
    cpp/test_system.cpp
    cpp/test_source_encoding.cpp
    cpp/test_resource.cpp
    cpp/test_debug.cpp
    cpp/test_hotreload_integration.cpp
    cpp/test_demo_smoke.cpp
    cpp/test_demo_e2e.cpp
    cpp/test_input.cpp
    cpp/test_gesture_detector.cpp
    cpp/test_async.cpp
    cpp/test_null_jobsystem.cpp
    cpp/test_job_system.cpp
    cpp/test_image_decoder.cpp
    cpp/test_audio.cpp
    cpp/test_audio_integration.cpp
    cpp/test_gpu_monitor.cpp
    cpp/test_rtt_pool.cpp
    cpp/test_text_cache.cpp
    cpp/test_text_renderer.cpp
    cpp/test_quad_batch.cpp
    cpp/test_ndc_math.cpp
    cpp/test_color_filter.cpp
    cpp/test_lua_manager.cpp
    cpp/test_script_bindings.cpp
    cpp/test_script_boundary.cpp
    cpp/test_game_state.cpp
    cpp/test_texture_budget_edge.cpp
    cpp/test_resource_handle.cpp
    cpp/test_error_ui.cpp
    cpp/test_entry.cpp
    cpp/test_di.cpp
    cpp/test_minigame.cpp
    cpp/test_platform.cpp
    cpp/test_storage.cpp
    cpp/test_save_migration.cpp
    cpp/test_save_roundtrip.cpp
    cpp/test_golden_saves.cpp
    cpp/test_mobile_adapter.cpp
    cpp/test_render_device.cpp
    cpp/test_render_pipeline.cpp
    cpp/test_render_integration.cpp
    cpp/test_render_postfx.cpp
    cpp/test_rpc.cpp
    cpp/test_cloud_save.cpp
    cpp/test_mini_collision.cpp
    cpp/test_ai.cpp
    cpp/test_backend_registry.cpp
    cpp/test_mesh_renderer.cpp
    cpp/test_sma_skinner.cpp
    cpp/test_perf_bench.cpp
    cpp/test_layer_manager.cpp
    cpp/test_texture_manager.cpp
    cpp/test_kag_binding.cpp
    cpp/test_kag_integration.cpp
    cpp/test_kag_execution.cpp
    cpp/test_galgame_startup.cpp
    cpp/test_sandbox_quota.cpp
    cpp/test_particle_system.cpp
    cpp/test_engine_lifecycle.cpp
    cpp/test_mini_game.cpp
    cpp/test_steam.cpp
    cpp/test_live2d.cpp
    cpp/test_archive.cpp
    cpp/test_delta_carc.cpp
)

add_executable(CaesuraTests ${TEST_SOURCES})

target_include_directories(CaesuraTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(CaesuraTests PRIVATE
    # CAESURA_SOURCE_DIR provided by CaesuraBuildOptions (interface) since
    # round 133 -- do NOT redefine: duplicate defs with different encodings
    # broke Chinese repo paths on Windows (ACP conversion throws).
)

target_link_libraries(CaesuraTests PRIVATE
    doctest
    Caesura::Engine
    Caesura::Rpc
    lua
)

enable_testing()

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/sync_caesura_test_assets_$<CONFIG>.cmake"
    CONTENT "
file(COPY [[${CMAKE_SOURCE_DIR}/scripts/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraTests>/scripts]])
file(COPY [[${CMAKE_SOURCE_DIR}/tests/audio/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraTests>/tests/audio]])
file(COPY [[${CMAKE_SOURCE_DIR}/demo/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraTests>/demo]])
file(COPY [[${CMAKE_SOURCE_DIR}/scripts/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraAmeKAG>/scripts]])
file(COPY [[${CMAKE_SOURCE_DIR}/demo/]] DESTINATION [[$<TARGET_FILE_DIR:CaesuraAmeKAG>/demo]])
"
)

add_test(
    NAME CaesuraSyncTestAssets
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_BINARY_DIR}/sync_caesura_test_assets_$<CONFIG>.cmake"
)
set_tests_properties(CaesuraSyncTestAssets PROPERTIES
    FIXTURES_SETUP caesura-test-assets
    LABELS "setup;assets"
)

function(add_caesura_doctest name)
    set(options RUN_SERIAL)
    set(oneValueArgs RESOURCE_LOCK)
    set(multiValueArgs LABELS ARGS)
    cmake_parse_arguments(CAESURA_TEST "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_test(NAME ${name} COMMAND $<TARGET_FILE:CaesuraTests> ${CAESURA_TEST_ARGS})
    set_tests_properties(${name} PROPERTIES
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraTests>"
        FIXTURES_REQUIRED caesura-test-assets
    )
    if(CAESURA_TEST_LABELS)
        set_tests_properties(${name} PROPERTIES LABELS "${CAESURA_TEST_LABELS}")
    endif()
    if(CAESURA_TEST_RUN_SERIAL)
        set_tests_properties(${name} PROPERTIES RUN_SERIAL TRUE)
    endif()
    if(CAESURA_TEST_RESOURCE_LOCK)
        set_tests_properties(${name} PROPERTIES RESOURCE_LOCK "${CAESURA_TEST_RESOURCE_LOCK}")
    endif()
endfunction()

add_caesura_doctest(CaesuraUnitTests
    LABELS full regression
    RUN_SERIAL
    RESOURCE_LOCK global-singletons
)
add_caesura_doctest(CaesuraEntryTests
    LABELS entry lifecycle
    RESOURCE_LOCK global-singletons
    ARGS --source-file=*test_entry.cpp,*test_engine_lifecycle.cpp,*test_galgame_startup.cpp
)
add_caesura_doctest(CaesuraStorageTests
    LABELS storage filesystem
    RESOURCE_LOCK save-files
    ARGS --source-file=*test_storage.cpp,*test_system.cpp,*test_save_migration.cpp,*test_save_roundtrip.cpp,*test_golden_saves.cpp
)
add_caesura_doctest(CaesuraScriptTests
    LABELS script lua
    RESOURCE_LOCK global-singletons
    ARGS --source-file=*test_lua_manager.cpp,*test_script_bindings.cpp,*test_script_boundary.cpp,*test_kag_binding.cpp,*test_kag_integration.cpp,*test_kag_execution.cpp,*test_game_state.cpp
)
add_caesura_doctest(CaesuraRenderResourceTests
    LABELS render resource
    RESOURCE_LOCK global-singletons
    ARGS --source-file=*test_render_device.cpp,*test_render_pipeline.cpp,*test_render_integration.cpp,*test_texture_manager.cpp,*test_texture_budget_edge.cpp,*test_resource.cpp,*test_resource_handle.cpp,*test_async.cpp,*test_image_decoder.cpp,*test_gpu_monitor.cpp,*test_rtt_pool.cpp,*test_text_cache.cpp,*test_text_renderer.cpp,*test_quad_batch.cpp,*test_ndc_math.cpp,*test_color_filter.cpp,*test_layer_manager.cpp,*test_particle_system.cpp
)
add_caesura_doctest(CaesuraAudioTests
    LABELS audio
    RUN_SERIAL
    RESOURCE_LOCK audio-device
    ARGS --source-file=*test_audio.cpp,*test_audio_integration.cpp
)

add_test(
    NAME CaesuraHeadlessCliSmoke
    COMMAND $<TARGET_FILE:CaesuraAmeKAG> --headless
)
set_tests_properties(CaesuraHeadlessCliSmoke PROPERTIES
    LABELS "entry;headless;cli"
    RUN_SERIAL TRUE
    RESOURCE_LOCK "global-singletons"
    FIXTURES_REQUIRED caesura-test-assets
    PASS_REGULAR_EXPRESSION "Backends ready: render=NullRender audio=NullAudio platform=NullPlatform"
    FAIL_REGULAR_EXPRESSION "Backends ready: render=bgfx audio=soloud platform=sdl3|\\[SDL3\\] Platform backend initialized|\\[BgfxRenderDevice\\] nwh=|\\[Audio\\] SoLoud initialized"
    TIMEOUT 15
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraAmeKAG>"
)

if(Python3_EXECUTABLE)
    add_test(
        NAME CaesuraHeadlessRpcSmoke
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/headless_rpc_smoke.py
                $<TARGET_FILE:CaesuraAmeKAG>
    )
    set_tests_properties(CaesuraHeadlessRpcSmoke PROPERTIES
        LABELS "entry;headless;cli;rpc"
        RUN_SERIAL TRUE
        RESOURCE_LOCK "global-singletons"
        FIXTURES_REQUIRED caesura-test-assets
        PASS_REGULAR_EXPRESSION "ALL HEADLESS RPC TESTS PASSED"
        TIMEOUT 120
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraAmeKAG>"
    )
    add_test(
        NAME CaesuraHeadlessHttpSmoke
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/headless_http_smoke.py
                $<TARGET_FILE:CaesuraAmeKAG>
    )
    set_tests_properties(CaesuraHeadlessHttpSmoke PROPERTIES
        LABELS "entry;headless;cli;rpc;http"
        RUN_SERIAL TRUE
        RESOURCE_LOCK "global-singletons"
        FIXTURES_REQUIRED caesura-test-assets
        PASS_REGULAR_EXPRESSION "ALL HEADLESS HTTP SMOKE TESTS PASSED"
        SKIP_RETURN_CODE 77
        TIMEOUT 90
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraAmeKAG>"
    )
    # Real-Ollama AI smoke: runs the REAL engine binary against a REAL local
    # Ollama server when one is reachable (sync + async AI.query); exits 77
    # (ctest SKIP) on machines without Ollama so CI stays green.
    add_test(
        NAME CaesuraHeadlessAiSmoke
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/headless_ai_smoke.py
                $<TARGET_FILE:CaesuraAmeKAG>
    )
    set_tests_properties(CaesuraHeadlessAiSmoke PROPERTIES
        LABELS "entry;headless;cli;rpc;ai"
        RUN_SERIAL TRUE
        RESOURCE_LOCK "global-singletons"
        FIXTURES_REQUIRED caesura-test-assets
        PASS_REGULAR_EXPRESSION "ALL HEADLESS AI SMOKE TESTS PASSED"
        SKIP_RETURN_CODE 77
        TIMEOUT 300
        WORKING_DIRECTORY "$<TARGET_FILE_DIR:CaesuraAmeKAG>"
    )
endif()

# Copy scripts to test output directory so sandbox.lua et al are found
add_custom_command(TARGET CaesuraTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/scripts"
    "$<TARGET_FILE_DIR:CaesuraTests>/scripts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/tests/audio"
    "$<TARGET_FILE_DIR:CaesuraTests>/tests/audio"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/demo"
    "$<TARGET_FILE_DIR:CaesuraTests>/demo"
    COMMENT "Copying test scripts and audio files..."
)

# CaesuraTests links production modules, including the SDL-backed platform
# implementation. Keep the runtime beside the fresh test binary instead of
# relying on a DLL left behind by another build directory.
if(TARGET SDL3::SDL3)
    get_target_property(_sdl3_tests_type SDL3::SDL3 TYPE)
    if(_sdl3_tests_type MATCHES "SHARED_LIBRARY|STATIC_LIBRARY|MODULE_LIBRARY")
        add_custom_command(TARGET CaesuraTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE_DIR:CaesuraTests>/$<TARGET_FILE_NAME:SDL3::SDL3>"
            COMMENT "Copying SDL3 runtime to test output directory..."
            VERBATIM
        )
    endif()
endif()
# Copy FFmpeg DLLs to test output so VideoPlayer can load them
if(CAESURA_FFMPEG_FOUND AND WIN32)
add_custom_command(TARGET CaesuraTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${FFMPEG_BIN_DIR}"
        "$<TARGET_FILE_DIR:CaesuraTests>"
        COMMENT "Copying FFmpeg DLLs to test output directory..."
    )
endif()

# Copy the Steamworks runtime DLL to test output so CaesuraTests (which links
# caesura_steam) can start at all; mirrors the root steam_api64.dll handling
# and the SDL3-runtime-beside-tests rule above.
if(CAESURA_HAS_STEAM AND WIN32)
    get_filename_component(STEAM_SDK_BIN_DIR "${STEAM_LIBRARY}" DIRECTORY)
    add_custom_command(TARGET CaesuraTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${STEAM_SDK_BIN_DIR}/steam_api64.dll"
        "$<TARGET_FILE_DIR:CaesuraTests>/steam_api64.dll"
        COMMENT "Copying Steamworks runtime to test output directory..."
    )
endif()