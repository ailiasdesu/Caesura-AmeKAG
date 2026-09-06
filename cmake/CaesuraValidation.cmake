# Applies only to first-party targets through CaesuraBuildOptions.
set(CAESURA_SANITIZERS "OFF" CACHE STRING "First-party sanitizers: OFF or address-undefined")
set_property(CACHE CAESURA_SANITIZERS PROPERTY STRINGS OFF address-undefined)

# Strict verification must link the current object code. MSVC's incremental
# link state retained an earlier stub after an interface implementation changed.
if(MSVC AND CAESURA_REQUIRE_TEST_PREREQUISITES)
    target_link_options(CaesuraBuildOptions INTERFACE /INCREMENTAL:NO)
endif()

if(CAESURA_SANITIZERS STREQUAL "address-undefined")
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux"
       OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Clang|GNU)$")
        message(FATAL_ERROR "address-undefined requires a Linux Clang or GCC host")
    endif()
    target_compile_options(CaesuraBuildOptions INTERFACE
        -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
    target_link_options(CaesuraBuildOptions INTERFACE -fsanitize=address,undefined)
elseif(NOT CAESURA_SANITIZERS STREQUAL "OFF")
    message(FATAL_ERROR "Unknown CAESURA_SANITIZERS: ${CAESURA_SANITIZERS}")
endif()
