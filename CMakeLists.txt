cmake_minimum_required(VERSION 3.28)

project(pocket-editor VERSION 0.1.0)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY, "${CMAKE_SOURCE_DIR}/bin")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Useful SDL extensions.
set(SDLIMAGE_JPG ON)
set(SDLIMAGE_PNG ON)

# Disable dependency on XScreenSaver for compilation on WSL.
set(SDL_X11_XSCRNSAVER OFF)

include(FetchContent)

# Pull in Corrosion build tool for compiling with Rust.
FetchContent_Declare(
  Corrosion
  GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
  GIT_TAG v0.6 # Optionally specify a commit hash, version tag or branch here
)
FetchContent_MakeAvailable(Corrosion)

# Add external dependencies via Git submodules.
add_subdirectory(lib/SDL EXCLUDE_FROM_ALL)
add_subdirectory(lib/SDL_image EXCLUDE_FROM_ALL)

# Import targets defined in the Pocket Compiler library.
corrosion_import_crate(MANIFEST_PATH lib/pocket-compiler/Cargo.toml)

# Compile the editor executable.
add_executable(
  pocket-editor
  src/editor.cpp
  src/compiler.cpp
  src/editor.hpp
  include/compiler.hpp
  # Dear ImGui
  lib/imgui/backends/imgui_impl_sdl3.cpp
  lib/imgui/backends/imgui_impl_sdl3.h
  lib/imgui/backends/imgui_impl_sdlrenderer3.cpp
  lib/imgui/backends/imgui_impl_sdlrenderer3.h
  lib/imgui/imgui.h
  lib/imgui/imgui.cpp
  lib/imgui/imgui_draw.cpp
  lib/imgui/imgui_tables.cpp
  lib/imgui/imgui_widgets.cpp
  lib/imgui/misc/cpp/imgui_stdlib.cpp
  lib/imgui/misc/cpp/imgui_stdlib.h
  # ImGuiColorTextEdit
  lib/imguicolortextedit/TextEditor.cpp
  lib/imguicolortextedit/TextEditor.h
        src/editor.hpp)

# Include Pocket Compiler bindings for C++.
target_include_directories(pocket-editor PRIVATE include)

# Include the Dear ImGui headers.
target_include_directories(pocket-editor PRIVATE lib/imgui)

# Include the ImguiColorTextEdit headers.
target_include_directories(pocket-editor PRIVATE lib/imguicolortextedit)

# Link to the actual SDL3 libraries.
target_link_libraries(pocket-editor PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)

# Link against the Pocket Compiler.
target_link_libraries(pocket-editor PUBLIC compiler_ffi)
