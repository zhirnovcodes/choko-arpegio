cmake_minimum_required(VERSION 3.25)

# macOS: one universal build for Apple Silicon and Intel. Must be set before project(). Other platforms ignore these.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(ChocoArpegio VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The core does not throw or use RTTI. JUCE does, so these flags stay on core targets only.
function(ca_core_flags target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /GR- /EHs-c-)
    else()
        target_compile_options(${target} PRIVATE -fno-exceptions -fno-rtti)
    endif()
endfunction()

# params.json is the one parameter list. CMake writes it into a header the engine reads
# (defaults, ranges, kinds). Editing params.json re-runs configure.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/params.json)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/params.json CA_PARAMS_JSON)
string(JSON ca_param_count LENGTH "${CA_PARAMS_JSON}" params)
math(EXPR ca_param_last "${ca_param_count} - 1")
set(ca_param_rows "")
foreach(index RANGE ${ca_param_last})
    string(JSON param_id GET "${CA_PARAMS_JSON}" params ${index} id)
    string(JSON param_kind GET "${CA_PARAMS_JSON}" params ${index} kind)
    string(JSON param_min GET "${CA_PARAMS_JSON}" params ${index} min)
    string(JSON param_max GET "${CA_PARAMS_JSON}" params ${index} max)
    string(JSON param_default GET "${CA_PARAMS_JSON}" params ${index} default)
    string(JSON param_display ERROR_VARIABLE param_display_error GET "${CA_PARAMS_JSON}" params ${index} display)
    if(param_display_error)
        set(param_pow2 false)
    elseif(param_display STREQUAL "pow2")
        set(param_pow2 true)
    else()
        set(param_pow2 false)
    endif()
    string(APPEND ca_param_rows "    {\"${param_id}\", \"${param_kind}\", ${param_min}, ${param_max}, ${param_default}, ${param_pow2}},\n")
endforeach()

file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/generated/ParamsJson.h CONTENT
"#pragma once
// Written by CMake from params.json. Do not edit.

namespace ca {

struct ParamSpec {
    const char* id;
    const char* kind;
    double min;
    double max;
    double def;
    bool pow2;      // value is an exponent n, meaning 2^n beats (or loops)
};

inline constexpr ParamSpec kParams[] = {
${ca_param_rows}};

} // namespace ca
" @ONLY)

add_library(ca_domain
    core/domain/Sequences.cpp
    core/domain/TimeTable.cpp
    core/domain/Chaos.cpp
)
target_include_directories(ca_domain PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core/domain)
ca_core_flags(ca_domain)

add_library(ca_engine core/engine/Engine.cpp)
target_include_directories(ca_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core/engine
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_link_libraries(ca_engine PUBLIC ca_domain)
ca_core_flags(ca_engine)

add_executable(ca_tests
    tests/main.cpp
    tests/sequences_tests.cpp
    tests/timetable_tests.cpp
    tests/chaos_tests.cpp
    tests/engine_tests.cpp
)
target_link_libraries(ca_tests PRIVATE ca_engine)
ca_core_flags(ca_tests)

if(EMSCRIPTEN)
    # Core tests run under Node in the wasm build: node build-wasm/ca_tests.js
    set_target_properties(ca_tests PROPERTIES SUFFIX ".js")
    # Tests keep Engines on the stack (~60 KB each); emscripten's default stack is 64 KB.
    target_link_options(ca_tests PRIVATE -sSTACK_SIZE=8MB)

    # Standalone module: instantiated directly, no Emscripten JS glue.
    add_executable(choko-arpegio hosts/web/boundary/api.cpp)
    target_link_libraries(choko-arpegio PRIVATE ca_engine)
    ca_core_flags(choko-arpegio)
    set_target_properties(choko-arpegio PROPERTIES SUFFIX ".wasm")
    target_link_options(choko-arpegio PRIVATE
        -O2 --no-entry -sSTANDALONE_WASM -sALLOW_MEMORY_GROWTH -sFILESYSTEM=0
        -sEXPORTED_FUNCTIONS=_ca_alloc,_ca_free,_ca_create,_ca_destroy,_ca_set_param,_ca_param,_ca_transport,_ca_midi,_ca_all_off,_ca_process,_ca_out_count,_ca_out_frame,_ca_out_kind,_ca_out_bytes,_ca_chaos_changes,_ca_seed,_ca_set_seed,_ca_slot_count,_ca_slot_start,_ca_slot_end,_ca_sound_end,_ca_note_value,_ca_time_origin
    )
endif()
