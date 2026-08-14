cmake_minimum_required(VERSION 3.21)

project(Nightingale VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG v1.17.0
    SOURCE_SUBDIR api/cpp
)
FetchContent_MakeAvailable(Slint)

find_package(SQLite3 REQUIRED)
find_package(TagLib REQUIRED)
find_package(Threads REQUIRED)

add_executable(Nightingale 
    src/main.cpp
    src/core/audio/AudioManager.cpp
    src/core/audio/QueueManager.cpp
    )
slint_target_sources(Nightingale ui/main.slint)

target_link_libraries(Nightingale 
    PRIVATE 
        Slint::Slint
        SQLite3::SQLite3
        TagLib::TagLib
        Threads::Threads
)

if(UNIX AND NOT APPLE)
    target_link_libraries(Nightingale PRIVATE dl m)
endif()
