# Engine library - our new code (stubs, host logic, XML parser, pathfinding, renderer)
add_library(engine STATIC
    stubs/StubUtilityIFace.cpp
    stubs/StubLinkFixes.cpp
    xml/XmlParser.cpp
    xml/pugixml.cpp
    pathfinding/FAStar.cpp
    pathfinding/RealFAStarIFace.cpp
    Renderer.cpp
    GLRenderer.cpp
    ShaderProgram.cpp
    FPKArchive.cpp
    DDSLoader.cpp
    AssetManager.cpp
    NifLoader.cpp
    Mesh.cpp
)

# glad (OpenGL loader) - compiled as C
add_library(glad STATIC
    "${CMAKE_SOURCE_DIR}/third_party/glad/glad.c"
)
target_include_directories(glad PUBLIC "${CMAKE_SOURCE_DIR}/third_party/glad")
if(MSVC)
    target_compile_options(glad PRIVATE /W0)
endif()

target_include_directories(engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/xml
    ${CMAKE_CURRENT_SOURCE_DIR}/pathfinding
)

# Engine needs to see gamecore headers (for CvGlobals, interface base classes, etc.)
target_link_libraries(engine PUBLIC gamecore glad)

# SDL2 + SDL2_ttf headers for Renderer/GLRenderer
target_include_directories(engine PRIVATE "${SDL2_INCLUDE_DIR}" "${SDL2_TTF_INCLUDE_DIR}")

# glad headers
target_include_directories(engine PRIVATE "${CMAKE_SOURCE_DIR}/third_party/glad")

# Python 2.4 headers needed transitively by CvDLLPythonIFaceBase.h
set(PYTHON24_ROOT "C:/Civ4SDK/beyond-the-sword-sdk-develop/CvGameCoreDLL/Python24")
target_include_directories(engine SYSTEM PRIVATE "${PYTHON24_ROOT}/include")

# Engine code includes CvGameCoreDLL.h which needs the same defines as gamecore
target_compile_definitions(engine PRIVATE
    OPENCIV4
    _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
    Py_BUILD_CORE       # Prevent Python.h from auto-linking python24.lib
)

# Suppress warnings in stub code that includes SDK headers
if(MSVC)
    target_compile_options(engine PRIVATE /W0 /Zc:forScope-)
endif()
