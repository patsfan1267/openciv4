# Gamecore library — forked CvGameCoreDLL source, compiled as static lib for 64-bit
#
# The original SDK was built as a 32-bit DLL with MSVC 2003 + Boost 1.32 + Python 2.4.
# We compile it as a static library linked into our host exe, reusing the original
# Boost/Python headers but NOT linking their 32-bit libs (no Python bindings needed).

set(BOOST_ROOT "C:/Civ4SDK/beyond-the-sword-sdk-develop/CvGameCoreDLL/Boost-1.32.0")
set(PYTHON24_ROOT "C:/Civ4SDK/beyond-the-sword-sdk-develop/CvGameCoreDLL/Python24")

# Collect all .cpp files EXCEPT the ones we explicitly skip
file(GLOB GAMECORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Remove files we don't want to compile:
#  - CvDLLPython.cpp / CvDllPythonEvents.cpp — calls into Python runtime (not needed headless)
#  - CvEventReporter.cpp — fires Python callbacks (stub instead)
#  - CvGameCoreDLL.cpp — DLL entry point (DllMain), not needed for static lib
#  - CvGameInterface.cpp — UI interaction callbacks (not needed headless)
#  - CvDllTranslator.cpp — text translation via DLL (stub instead)
#  - FDialogTemplate.cpp — Windows dialog UI (not needed headless)
#  - FAssert.cpp — may reference DLL-specific assertion handlers
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "CvDLLPython\\.cpp$")
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "CvDllPythonEvents\\.cpp$")
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "CvEventReporter\\.cpp$")
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "CvGameCoreDLL\\.cpp$")
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "CvGameInterface\\.cpp$")
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "CvDllTranslator\\.cpp$")
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "FDialogTemplate\\.cpp$")
list(FILTER GAMECORE_SOURCES EXCLUDE REGEX "FAssert\\.cpp$")

add_library(gamecore STATIC ${GAMECORE_SOURCES})

target_include_directories(gamecore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Boost.Python is stubbed out for headless build (OPENCIV4 define).
# Python 2.4 headers still needed by CvDLLPythonIFaceBase.h and CyArgsList.h
target_include_directories(gamecore SYSTEM PRIVATE
    "${PYTHON24_ROOT}/include"
)

# _USRDLL: Makes MAX_CIV_PLAYERS a compile-time constant (18) instead of runtime lookup
# OPENCIV4: Our own flag for conditional compilation in compatibility patches
target_compile_definitions(gamecore PRIVATE
    _USRDLL
    OPENCIV4
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
    _CRT_SECURE_NO_WARNINGS
    Py_BUILD_CORE       # Prevent Python.h from auto-linking python24.lib via pragma
)

# The old SDK code has many warnings with modern MSVC — suppress the noisy ones
if(MSVC)
    target_compile_options(gamecore PRIVATE
        /W0           # Suppress all warnings for SDK code (too noisy to fix en masse)
        /wd4996       # Deprecated functions (strcpy, etc.)
        /bigobj       # Some translation units are huge
        /EHsc         # C++ exception handling
        /Zc:wchar_t   # wchar_t as built-in type
        /Zc:forScope- # Old MSVC behavior: for-loop variables visible after loop (SDK uses this)
    )
endif()
