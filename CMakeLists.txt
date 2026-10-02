cmake_minimum_required(VERSION 3.20)
project(RetroAmp LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Static CRT -> RetroAmp.exe runs on any Windows 10/11 machine without redistributables.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(RetroAmp WIN32
    src/main.cpp
    src/app.cpp
    src/common.cpp
    src/gfx.cpp
    src/zip.cpp
    src/skin.cpp
    src/settings.cpp
    src/playlist.cpp
    src/audio/decoder.cpp
    src/audio/dsp.cpp
    src/audio/player.cpp
    src/ui/skinwnd.cpp
    src/ui/mainwnd.cpp
    src/ui/eqwnd.cpp
    src/ui/plwnd.cpp
    src/ui/dspwnd.cpp
    src/ui/viswnd.cpp
    src/vis/visengine.cpp
    res/retroamp.rc
)

target_compile_definitions(RetroAmp PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
target_compile_options(RetroAmp PRIVATE /W3 /utf-8 /MP)
target_link_options(RetroAmp PRIVATE /MANIFEST:NO)
target_link_libraries(RetroAmp PRIVATE
    mfplat mfreadwrite mfuuid propsys shlwapi comctl32 gdiplus ole32 uuid normaliz)

# Console self-test (decoders, DSP response, skin archives)
add_executable(retroamp_selftest
    tests/selftest.cpp
    src/common.cpp
    src/gfx.cpp
    src/zip.cpp
    src/skin.cpp
    src/audio/decoder.cpp
    src/audio/dsp.cpp
)
target_compile_definitions(retroamp_selftest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
target_compile_options(retroamp_selftest PRIVATE /W3 /utf-8)
target_link_libraries(retroamp_selftest PRIVATE mfplat mfreadwrite mfuuid propsys shlwapi gdiplus ole32 normaliz)

# Ship the bundled skins next to the executable
file(GLOB RETROAMP_SKINS ${CMAKE_SOURCE_DIR}/skins/*.wsz)
add_custom_command(TARGET RetroAmp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:RetroAmp>/Skins
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RETROAMP_SKINS} $<TARGET_FILE_DIR:RetroAmp>/Skins/)
