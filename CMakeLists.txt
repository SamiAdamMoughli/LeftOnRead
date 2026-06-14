cmake_minimum_required(VERSION 3.20)
project(PeekABoo LANGUAGES CXX)

# Windows x64 only — NT API offsets are hardcoded for 64-bit
if(NOT WIN32)
    message(FATAL_ERROR "PeekABoo only builds on Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(PeekABoo
    src/main.cpp
    src/ErrorHandler.cpp
    src/FileGhoster.cpp
    src/PayloadManager.cpp
    src/ProcessExecutor.cpp
    src/SectionManager.cpp
)

target_include_directories(PeekABoo PRIVATE include/peek-a-boo)

target_compile_definitions(PeekABoo PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# Require x64 — 32-bit builds would silently produce wrong PEB offsets
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "PeekABoo must be compiled for x64.")
endif()

target_link_libraries(PeekABoo PRIVATE ntdll)
