cmake_minimum_required(VERSION 3.4.1)

project(nunchuk-android)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message("ANDROID_ABI::${ANDROID_ABI}")

add_library(nunchuk-android SHARED
        initializer.cpp
        deserializer.cpp
        serializer.cpp
        nfc.cpp
        nunchukprovider.cpp
        initializer-jni.cpp
        wallet-jni.cpp
        signer-jni.cpp
        room-wallet-jni.cpp
        room-jni.cpp
        room-transaction-jni.cpp
        transaction-jni.cpp
        util-jni.cpp
        modelprovider.cpp
        sync-jni.cpp
        tap-protocol-jni.cpp
        satscard-jni.cpp
        ledger-jni.cpp
        bitbox-jni.cpp
        jade-jni.cpp
        mk4.cpp
        string-wrapper.h
        group-wallet-jni.cpp
)

set(NUNCHUK_LIBNUNCHUK_DIR "" CACHE PATH "Path to the libnunchuk source tree")
if(NOT NUNCHUK_LIBNUNCHUK_DIR AND DEFINED ENV{NUNCHUK_LIBNUNCHUK_DIR})
        set(NUNCHUK_LIBNUNCHUK_DIR "$ENV{NUNCHUK_LIBNUNCHUK_DIR}")
endif()
if(NOT NUNCHUK_LIBNUNCHUK_DIR)
        set(NUNCHUK_LIBNUNCHUK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libnunchuk")
endif()

add_subdirectory("${NUNCHUK_LIBNUNCHUK_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/libnunchuk-build")

if(TARGET event_core_static AND NOT TARGET libevent::core)
        add_library(libevent::core ALIAS event_core_static)
endif()
if(TARGET event_extra_static AND NOT TARGET libevent::extra)
        add_library(libevent::extra ALIAS event_extra_static)
endif()

target_link_libraries(nunchuk-android nunchuk)

target_include_directories(nunchuk-android PUBLIC
        "${PROJECT_SOURCE_DIR}"
        "${NUNCHUK_LIBNUNCHUK_DIR}/include"
        "${NUNCHUK_LIBNUNCHUK_DIR}/src"
)
