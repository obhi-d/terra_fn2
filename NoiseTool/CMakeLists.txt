
CPMAddPackage(
    NAME corrade
    GITHUB_REPOSITORY mosra/corrade
    GIT_TAG e6b4d952e757ab99a0b67e516235b7f91904dca0
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "WITH_INTERCONNECT OFF"
        "WITH_TESTSUITE OFF"
)


CPMAddPackage(
    NAME SDL2
    GITHUB_REPOSITORY libsdl-org/SDL
    GIT_TAG release-2.24.0
    EXCLUDE_FROM_ALL YES
    OPTIONS
      "SDL_ATOMIC OFF"
      "SDL_CPUINFO OFF"
      "SDL_FILE OFF"
      "SDL_FILESYSTEM OFF"
      "SDL_HAPTIC OFF"
      "SDL_LOCALE OFF"
      "SDL_POWER OFF"
      "SDL_RENDER OFF"
      "SDL_SENSOR OFF"
      "SDL_THREADS ON"
      "SDL_STATIC ON"
      "SDL_SHARED OFF"
)

CPMAddPackage(
    NAME magnum
    GITHUB_REPOSITORY mosra/magnum
    GIT_TAG fa00122fdb84eab23064d83c36d50757fdf899c1
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "BUILD_PLUGINS_STATIC ON"
        "WITH_SDL2APPLICATION ON"
        "WITH_GLFWAPPLICATION OFF"
        "WITH_MESHTOOLS OFF"
        "WITH_TRADE ON"
        "WITH_TEXT OFF"
        "WITH_TEXTURETOOLS OFF"
)
   
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG aceab9a877de0258d19d29a5d87a51b63a8999bf
    EXCLUDE_FROM_ALL YES
)
# Set dir for find_package(ImGui)
set(IMGUI_DIR ${imgui_SOURCE_DIR})

CPMAddPackage(
    NAME magnum-integration
    GITHUB_REPOSITORY mosra/magnum-integration
    GIT_TAG 2c2737621f3f7bb17bba4e5342f68e68476a5677
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "MAGNUM_WITH_IMGUI ON"
)

CPMAddPackage(
    NAME magnum-plugins
    GITHUB_REPOSITORY mosra/magnum-plugins
    GIT_TAG 1c9d360aa21a3788f32b2d94ad45c39841702a39
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "MAGNUM_WITH_STBIMAGEIMPORTER ON"
        "MAGNUM_WITH_DDSIMPORTER ON"
)

CPMAddPackage(
    NAME imnodes
    GITHUB_REPOSITORY Auburn/imnodes
    GIT_TAG 277fdd7a912e622a2476981fea14c68abcb6b3c8
    GIT_SUBMODULES ".github"
    EXCLUDE_FROM_ALL YES
    DOWNLOAD_ONLY YES
)
  
CPMAddPackage(
    NAME robinhoodhashing
    GITHUB_REPOSITORY martinus/robin-hood-hashing
    GIT_TAG 3.11.5    
    EXCLUDE_FROM_ALL YES
)

CPMAddPackage(
    NAME zlib
    GITHUB_REPOSITORY madler/zlib
    GIT_TAG 21767c654d31d2dccdde4330529775c6c5fd5389
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
)

set(ZLIB_INCLUDE_DIRS ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
set(ZLIB_INCLUDE_DIR ${zlib_SOURCE_DIR} ${zlib_BINARY_DIR})
set(PNG_BUILD_ZLIB OFF CACHE BOOL "Custom zlib Location, else find_package is used")
include_directories(ZLIB_INCLUDE_DIRS)

CPMAddPackage(
    NAME libpng
    GITHUB_REPOSITORY glennrp/libpng
    GIT_TAG 763c77e628e2c78b4c422bb7e26327d28ee54ad4
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "PNG_STATIC ON"
        "PNG_SHARED OFF"
        "PNG_EXECUTABLES OFF"
        "PNG_TESTS OFF"
)

# Use modules from magnum-integration since it has everything we need
set(CMAKE_MODULE_PATH "${magnum-integration_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

find_package(Magnum REQUIRED GL Sdl2Application Trade)
find_package(MagnumIntegration REQUIRED ImGui)
find_package(ImGui REQUIRED SourcesMiscCpp)
find_package(libpng REQUIRED)


# Ensure FastNoise.dll is built into the same dir as NoiseTool.exe
set_target_properties(FastNoise
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
)

# Bundle a better font
# Configure resource file for imgui source dir variable
set(NoiseTool_RESOURCES_DIR ${CMAKE_CURRENT_SOURCE_DIR})
configure_file("resources.conf" "${CMAKE_CURRENT_BINARY_DIR}/resources.conf")
corrade_add_resource(NoiseTool_RESOURCES "${CMAKE_CURRENT_BINARY_DIR}/resources.conf")

add_executable(NoiseTool
    "NoiseToolApp.cpp"
    "FastNoiseNodeEditor.cpp"
    
    "NoiseTexture.cpp"
    "${imnodes_SOURCE_DIR}/imnodes.cpp"
    ${NoiseTool_RESOURCES}
    "${NoiseTool_RESOURCES_DIR}/icon.rc"
    "ImGuiFileDialog.cpp"
 "EditableHeightmap.cpp" "ToolWindow.cpp") 

# Include imnodes in NoiseTool project since setting up separate cmake library with imgui dependancy was causing issues
target_include_directories(NoiseTool PRIVATE 
    "${imnodes_SOURCE_DIR}"
    "${libpng_SOURCE_DIR}"
    "${libpng_BINARY_DIR}")

target_link_libraries(NoiseTool PRIVATE
    FastNoise
    Magnum::Application
    Magnum::Shaders
    Magnum::SceneGraph
    Magnum::Trade
    MagnumIntegration::ImGui
    ImGui::SourcesMiscCpp
    robin_hood
    zlib
    png_static
    SDL2::SDL2
)
add_dependencies(NoiseTool MagnumPlugins::StbImageImporter MagnumPlugins::DdsImporter)
# Windows HiDPI support
if(CORRADE_TARGET_WINDOWS)
    target_sources(NoiseTool PRIVATE WindowsHiDPI.manifest)
endif()

if (UNIX)
    target_link_options(NoiseTool PRIVATE -pthread)

    if(APPLE)
        set_property(TARGET NoiseTool PROPERTY
            INSTALL_RPATH "@loader_path/../lib")
    else()
        set_property(TARGET NoiseTool PROPERTY
            INSTALL_RPATH "\$ORIGIN/../lib")
    endif()
endif()

if (MSVC)
    target_compile_definitions(NoiseTool PRIVATE _CRT_SECURE_NO_WARNINGS=1)
endif()

set(install_targets ${install_targets} NoiseTool PARENT_SCOPE)

# Make the executable a default target to build & run in Visual Studio
set_property(DIRECTORY ${PROJECT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NoiseTool)
