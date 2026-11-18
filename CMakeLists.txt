cmake_minimum_required(VERSION 3.16)
project(warp_menu CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(FetchContent)

# Pin raylib 5.5 — reproducible, no system raylib needed.
set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(raylib)

# raylib's bundled miniaudio PulseAudio backend fails to open a playback device
# on PipeWire systems (pipewire-pulse). Force the ALSA backend instead — it
# routes cleanly through the pipewire-alsa bridge. (verified: aplay/speaker-test
# work via ALSA "default", pulse path does not init under miniaudio here.)
target_compile_definitions(raylib PRIVATE MA_NO_PULSEAUDIO MA_NO_JACK)

add_executable(warp-menu
    src/main.cpp
    src/warp.cpp
    src/ui.cpp
    src/audio.cpp
    src/town.cpp
)
target_link_libraries(warp-menu PRIVATE raylib)
target_compile_options(warp-menu PRIVATE -Wall -Wextra)

# threads (std::thread) + math
find_package(Threads REQUIRED)
target_link_libraries(warp-menu PRIVATE Threads::Threads m)

# Asset path baked in so the binary finds assets/ next to the source tree.
target_compile_definitions(warp-menu PRIVATE
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
