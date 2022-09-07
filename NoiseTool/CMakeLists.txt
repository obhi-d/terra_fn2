CPMAddPackage(
    NAME corrade
    GITHUB_REPOSITORY mosra/corrade
    GIT_TAG 61d1b58cbcb159837bca506b5336a810da67f0a7
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "WITH_INTERCONNECT OFF"
        "WITH_TESTSUITE OFF"
)

CPMAddPackage(
    NAME GLFW
    GITHUB_REPOSITORY glfw/glfw
    GIT_TAG 3.3.7
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_SHARED_LIBS OFF"
        "GLFW_INSTALL OFF"
        "GLFW_BUILD_TESTS OFF"
        "GLFW_BUILD_EXAMPLES OFF"
        "GLFW_BUILD_DOCS OFF"
)

CPMAddPackage(
    NAME magnum
    GITHUB_REPOSITORY mosra/magnum
    GIT_TAG fac6f4da261930c7035bf51f66091e350905720b
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "BUILD_PLUGINS_STATIC ON"
        "WITH_GLFWAPPLICATION ON"
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
    GIT_TAG a6c469de651032746a965c5aa83c243ca6a097a6
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "WITH_IMGUI ON"
)

CPMAddPackage(
    NAME magnum-plugins
    GITHUB_REPOSITORY mosra/magnum-plugins
    GIT_TAG b0f2818b781eb0fb6a8a76095a6529fc85d442f3
    GIT_SUBMODULES "src"
    EXCLUDE_FROM_ALL YES
    OPTIONS
        "BUILD_STATIC ON"
        "WITH_STBIMAGEIMPORTER ON"
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

# Use modules from magnum-integration since it has everything we need
set(CMAKE_MODULE_PATH "${magnum-integration_SOURCE_DIR}/modules" ${CMAKE_MODULE_PATH})

find_package(Magnum REQUIRED GL GlfwApplication Trade)
find_package(MagnumIntegration REQUIRED ImGui)
find_package(ImGui REQUIRED SourcesMiscCpp)

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
    "MeshNoisePreview.cpp"
    "NoiseTexture.cpp"
    "${imnodes_SOURCE_DIR}/imnodes.cpp"
    ${NoiseTool_RESOURCES}
    "${NoiseTool_RESOURCES_DIR}/icon.rc"
    "ImGuiFileDialog.cpp"
) 

# Include imnodes in NoiseTool project since setting up separate cmake library with imgui dependancy was causing issues
target_include_directories(NoiseTool PRIVATE 
    "${imnodes_SOURCE_DIR}")

target_link_libraries(NoiseTool PRIVATE
    FastNoise
    Magnum::Application
    Magnum::Shaders
    Magnum::SceneGraph
    Magnum::Trade
    MagnumIntegration::ImGui
    ImGui::SourcesMiscCpp
    robin_hood
)
add_dependencies(NoiseTool MagnumPlugins::StbImageImporter)
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
