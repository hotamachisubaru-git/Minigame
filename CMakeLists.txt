cmake_minimum_required(VERSION 3.20)

project(PticketGetter LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "PticketGetter requires Windows (Win32 API / GDI+).")
endif()

if (MSVC AND CMAKE_GENERATOR MATCHES "Visual Studio")
    if (CMAKE_VS_PLATFORM_NAME AND NOT CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
        message(FATAL_ERROR "PticketGetter is x64-only. Configure with: cmake -S . -B build -A x64")
    endif()
endif()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "PticketGetter is x64-only. Current pointer size is ${CMAKE_SIZEOF_VOID_P} bytes.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(PticketGetter WIN32
    src/asset_catalog.cpp
    src/game_app.cpp
    src/main.cpp
    src/shooter_game.cpp
    src/stage_book.cpp
)

target_compile_definitions(PticketGetter PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
)

if (MSVC)
    target_compile_options(PticketGetter PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

target_link_libraries(PticketGetter PRIVATE
    gdiplus
    msimg32
    winmm
)
