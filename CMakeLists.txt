cmake_minimum_required(VERSION 3.16)
project(warp_menu VERSION 1.0.0 DESCRIPTION "TOTVS WARP village menu" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)

# Pin raylib 5.5 — reproducible, no system raylib needed.
set(FETCHCONTENT_QUIET OFF)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

# Link the classic libGL.so.1 (present on every desktop) instead of GLVND's
# libOpenGL.so.0, which some distros don't install by default.
set(OpenGL_GL_PREFERENCE LEGACY)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW ON
    PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/patch-raylib.cmake
)
FetchContent_MakeAvailable(raylib)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # raylib's bundled miniaudio PulseAudio backend fails to open a playback
    # device on PipeWire systems (pipewire-pulse). Force the ALSA backend
    # instead — it routes cleanly through the pipewire-alsa bridge.
    target_compile_definitions(raylib PRIVATE MA_NO_PULSEAUDIO MA_NO_JACK)
endif()

set(WARP_SOURCES
    src/main.cpp
    src/warp.cpp
    src/ui.cpp
    src/audio.cpp
    src/town.cpp
)

if(APPLE)
    # warp-menu.app; assets are copied into Contents/Resources/assets below.
    set(MAC_ICON ${CMAKE_SOURCE_DIR}/packaging/macos/warp-menu.icns)
    set_source_files_properties(${MAC_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(warp-menu MACOSX_BUNDLE ${WARP_SOURCES} ${MAC_ICON})
    set_target_properties(warp-menu PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "TOTVS WARP"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.totvs.warp-menu"
        MACOSX_BUNDLE_ICON_FILE "warp-menu.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    add_custom_command(TARGET warp-menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets
                "$<TARGET_BUNDLE_CONTENT_DIR:warp-menu>/Resources/assets")
elseif(WIN32)
    # GUI subsystem: no console window behind the game.
    add_executable(warp-menu WIN32 ${WARP_SOURCES} packaging/windows/warp-menu.rc)
    if(MSVC)
        target_link_options(warp-menu PRIVATE /ENTRY:mainCRTStartup)
    endif()
    add_custom_command(TARGET warp-menu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets
                "$<TARGET_FILE_DIR:warp-menu>/assets")
else()
    add_executable(warp-menu ${WARP_SOURCES})
    # Ship one binary that runs on older distros: only glibc stays dynamic.
    target_link_options(warp-menu PRIVATE -static-libstdc++ -static-libgcc)
endif()

target_link_libraries(warp-menu PRIVATE raylib)
if(MSVC)
    target_compile_options(warp-menu PRIVATE /W3)
    target_compile_definitions(warp-menu PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(warp-menu PRIVATE -Wall -Wextra)
endif()

find_package(Threads REQUIRED)
target_link_libraries(warp-menu PRIVATE Threads::Threads)
if(NOT WIN32)
    target_link_libraries(warp-menu PRIVATE m)
endif()

# Dev fallback only: the source-tree assets/. Packaged builds find assets
# relative to the executable at runtime (see findAssets() in main.cpp).
target_compile_definitions(warp-menu PRIVATE
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

# ---------------------------------------------------------------------------
# Linux install layout + .deb / .rpm via CPack
#   cmake --build build && (cd build && cpack -G "DEB;RPM")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    install(TARGETS warp-menu RUNTIME DESTINATION bin COMPONENT app)
    install(DIRECTORY assets/ DESTINATION share/warp-menu/assets COMPONENT app)
    install(FILES packaging/linux/warp-menu.desktop DESTINATION share/applications
            COMPONENT app)
    install(FILES assets/icon.png DESTINATION share/icons/hicolor/256x256/apps
            RENAME warp-menu.png COMPONENT app)

    # Package only our files, not raylib's own install rules (headers, .a).
    set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};warp_menu;app;/")

    set(CPACK_PACKAGE_NAME "warp-menu")
    set(CPACK_PACKAGE_VENDOR "TOTVS")
    set(CPACK_PACKAGE_CONTACT "Cesar Menegatti")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "TOTVS WARP: 16-bit village menu for Cloudflare WARP")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/chmenegatti/warp-menu")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_STRIP_FILES ON)

    # GLFW and miniaudio dlopen() X11/GL/ALSA at runtime, so the linker
    # can't see them: declare them by hand.
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "net")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libc6 (>= 2.35), libgl1, libx11-6, libxrandr2, libxi6, libxcursor1, libxinerama1, libasound2t64 | libasound2")
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "cloudflare-warp")

    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_LICENSE "Proprietary")
    set(CPACK_RPM_PACKAGE_GROUP "Applications/Internet")
    set(CPACK_RPM_PACKAGE_REQUIRES
        "libGL.so.1()(64bit), libX11.so.6()(64bit), libXrandr.so.2()(64bit), libXi.so.6()(64bit), libXcursor.so.1()(64bit), libXinerama.so.1()(64bit), libasound.so.2()(64bit)")
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        /usr/share/applications /usr/share/icons /usr/share/icons/hicolor
        /usr/share/icons/hicolor/256x256 /usr/share/icons/hicolor/256x256/apps)

    include(CPack)
endif()
