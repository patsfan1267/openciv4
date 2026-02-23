cmake_minimum_required(VERSION 3.20)
project(OpenCiv4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Platform check - we only support 64-bit
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "OpenCiv4 requires a 64-bit build")
endif()

# Path to BTS game installation (for loading XML assets at runtime)
set(BTS_INSTALL_DIR "C:/Program Files (x86)/Steam/steamapps/common/Sid Meier's Civilization IV Beyond the Sword/Beyond the Sword" CACHE PATH "Path to BTS installation")

# Pass BTS path as a compile definition so code can find assets
add_compile_definitions(BTS_INSTALL_DIR="${BTS_INSTALL_DIR}")

# Suppress MSVC warnings that flood the old SDK code
if(MSVC)
    add_compile_options(
        /W3          # Warning level 3 (not 4, too noisy for SDK code)
        /wd4996      # Deprecated functions (sprintf, etc.)
        /wd4244      # Conversion warnings (int/float narrowing)
        /wd4267      # size_t to int conversion
        /wd4100      # Unreferenced formal parameter
        /wd4127      # Conditional expression is constant
        /wd4245      # Signed/unsigned mismatch in initialization
        /wd4505      # Unreferenced local function removed
        /EHsc        # C++ exception handling
        /bigobj      # Some SDK files have huge object files
    )
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _USRDLL      # SDK code checks this for gDLL macro variant
    )
endif()

# --- SDL2 ---
set(SDL2_DIR "${CMAKE_SOURCE_DIR}/third_party/SDL2")
set(SDL2_INCLUDE_DIR "${SDL2_DIR}/include")
set(SDL2_LIB_DIR "${SDL2_DIR}/lib/x64")

# --- Engine host (our code) ---
add_subdirectory(src/engine)

# --- Game core (forked SDK source) ---
add_subdirectory(src/gamecore)

# --- Main executable ---
add_executable(openciv4 src/engine/main.cpp)
target_link_libraries(openciv4 PRIVATE engine gamecore winmm
    "${SDL2_LIB_DIR}/SDL2.lib"
    "${SDL2_LIB_DIR}/SDL2main.lib"
)
target_include_directories(openciv4 PRIVATE "${SDL2_INCLUDE_DIR}")
target_link_options(openciv4 PRIVATE /MAP /SUBSYSTEM:CONSOLE)

# main.cpp includes CvGameCoreDLL.h which needs these defines
target_compile_definitions(openciv4 PRIVATE
    OPENCIV4
    _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
    Py_BUILD_CORE       # Prevent Python.h from auto-linking python24.lib
    SDL_MAIN_HANDLED    # We define our own main(), not SDL_main
)
# Python 2.4 headers needed transitively by CvDLLPythonIFaceBase.h
target_include_directories(openciv4 SYSTEM PRIVATE
    "C:/Civ4SDK/beyond-the-sword-sdk-develop/CvGameCoreDLL/Python24/include"
)
if(MSVC)
    target_compile_options(openciv4 PRIVATE /W0 /Zc:forScope-)
endif()

# Copy SDL2.dll next to the executable
add_custom_command(TARGET openciv4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_LIB_DIR}/SDL2.dll"
        $<TARGET_FILE_DIR:openciv4>
)
