cmake_minimum_required(VERSION 3.25)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(CaesuraCryptoAllocation LANGUAGES C CXX)
    enable_testing()
endif()

if(WIN32)
    message(FATAL_ERROR "This probe checks the OpenSSL implementation on non-Windows hosts")
endif()
find_package(OpenSSL REQUIRED)
get_filename_component(_crypto_root "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
file(GLOB _crypto_ed25519 CONFIGURE_DEPENDS "${_crypto_root}/external/ed25519/*.c")
add_library(CaesuraCryptoAllocationImpl STATIC
    "${_crypto_root}/src/archive/CryptoEngine.cpp"
    ${_crypto_ed25519}
)
target_compile_features(CaesuraCryptoAllocationImpl PRIVATE cxx_std_20)
target_compile_definitions(CaesuraCryptoAllocationImpl PRIVATE
    EVP_CIPHER_CTX_new=caesura_test_cipher_new
    EVP_CIPHER_CTX_free=caesura_test_cipher_free
)
target_link_libraries(CaesuraCryptoAllocationImpl PRIVATE OpenSSL::Crypto)
add_executable(CryptoAllocationProbe main.cpp)
target_compile_features(CryptoAllocationProbe PRIVATE cxx_std_20)
target_include_directories(CryptoAllocationProbe PRIVATE "${_crypto_root}/src")
target_link_libraries(CryptoAllocationProbe PRIVATE CaesuraCryptoAllocationImpl OpenSSL::Crypto)
if(TARGET Caesura::BuildOptions)
    target_link_libraries(CaesuraCryptoAllocationImpl PRIVATE Caesura::BuildOptions)
    target_link_libraries(CryptoAllocationProbe PRIVATE Caesura::BuildOptions)
endif()
add_test(NAME CaesuraCryptoAllocation COMMAND CryptoAllocationProbe)
set_tests_properties(CaesuraCryptoAllocation PROPERTIES TIMEOUT 30 LABELS "archive;allocation;openssl")
