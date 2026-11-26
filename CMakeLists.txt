cmake_minimum_required(VERSION 3.21)
project(Nightingale VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
find_package(SQLite3 REQUIRED)
find_package(TagLib REQUIRED)
find_package(Threads REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)
qt_policy(SET QTP0004 NEW)

qt_add_executable(Nightingale
    src/main.cpp
    src/AppController.hpp
    src/AppController.cpp
    src/presentation/LibraryModels.hpp

    src/persistence/ConfigManager.cpp
    src/persistence/DatabaseManager.cpp
    src/persistence/repositories/TrackRepository.cpp
    src/persistence/repositories/AlbumRepository.cpp
    src/persistence/repositories/ArtistRepository.cpp
    src/persistence/repositories/PlaylistRepository.cpp

    src/scanner/FileScanner.cpp
    src/scanner/TagReader.cpp

    src/audio/miniaudio_impl.cpp
    src/audio/Effects/AudioFX.cpp
    src/audio/Playback/PlaybackController.cpp
    src/audio/Queue/QueueManager.cpp
    src/audio/Sources/AudioDevice.cpp
    src/audio/Sources/AudioSource.cpp

    src/services/LibraryService.cpp
    src/services/PlaybackService.cpp
    src/services/PlaylistService.cpp
)

qt_add_qml_module(Nightingale
    URI NightingaleUI
    VERSION 1.0
    QML_FILES
        ui/Main.qml
        ui/components/NerdIcon.qml
    RESOURCES
        ui/fonts/SymbolsNerdFont-Regular.ttf
)

target_include_directories(Nightingale PRIVATE
    src
    src/third_party
)

target_link_libraries(Nightingale
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        SQLite3::SQLite3
        TagLib::TagLib
        Threads::Threads
)

target_compile_definitions(Nightingale PRIVATE
    UI_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/ui"
)

if(UNIX AND NOT APPLE)
    target_link_libraries(Nightingale PRIVATE dl m)
endif()