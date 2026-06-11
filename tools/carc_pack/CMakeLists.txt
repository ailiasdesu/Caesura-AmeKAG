# carc_pack - standalone CARC packing tool
cmake_minimum_required(VERSION 3.25)

set(TOOL_NAME carc_pack)

set(CARC_SOURCES
    ${CMAKE_SOURCE_DIR}/src/archive/CryptoEngine.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CARCWriter.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CARCReader.cpp
    ${CMAKE_SOURCE_DIR}/src/archive/CRLManager.cpp
)

set(ED25519_SOURCES
    ${CMAKE_SOURCE_DIR}/external/ed25519/fe.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/ge.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/keypair.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/sc.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/sha512.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/sign.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/verify.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/seed.c
    ${CMAKE_SOURCE_DIR}/external/ed25519/add_scalar.c
)

add_executable(${TOOL_NAME}
    main.cpp
    ${CARC_SOURCES}
    ${ED25519_SOURCES}
)

target_include_directories(${TOOL_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/external/ed25519
    ${CMAKE_SOURCE_DIR}/external/zstd/lib
)

if(WIN32)
    target_link_libraries(${TOOL_NAME} PRIVATE bcrypt libzstd_static)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${TOOL_NAME} PRIVATE OpenSSL::SSL OpenSSL::Crypto libzstd_static)
endif()

target_compile_definitions(${TOOL_NAME} PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    set_target_properties(${TOOL_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )
    target_compile_options(${TOOL_NAME} PRIVATE /W3 /wd4100 /wd4189 /wd4244 /utf-8 /FS)
endif()

set_target_properties(${TOOL_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
