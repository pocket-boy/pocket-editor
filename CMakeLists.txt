cmake_minimum_required(VERSION 3.28)

project(pocket-editor VERSION 0.1.0)

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
add_executable(pocket-editor src/editor.cpp include/compiler.hpp)

# Include Pocket Compiler bindings for C++.
target_include_directories(pocket-editor PRIVATE include)

# Link to the actual SDL3 libraries.
target_link_libraries(pocket-editor PRIVATE SDL3::SDL3 SDL3_image::SDL3_image)

# Link against the Pocket Compiler.
target_link_libraries(pocket-editor PUBLIC compiler_ffi)
