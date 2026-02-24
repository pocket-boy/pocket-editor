#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "compiler.hpp"
#include "imgui.h"
#include <iostream>

// Constant definition for default window width.
#define WIN_WIDTH (160 * 4)
// Constant definition for default window height.
#define WIN_HEIGHT (144 * 4)

int main(void) {
  // SDL windowing handle.
  SDL_Window *window;
  // SDL rendering handle.
  SDL_Renderer *renderer;

  // Attempt to initialise SDL systems, or error.
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    std::cerr << "Error: SDL_Init(): " << SDL_GetError() << "\n";
    return 1;
  }

  // Initialise SDL's windowing and rendering handles.
  SDL_CreateWindowAndRenderer("PocketBoy Editor", WIN_WIDTH, WIN_HEIGHT,
                              SDL_WINDOW_RESIZABLE, &window, &renderer);

  // Check for any errors with windowing and rendering initialisation.
  if (window == nullptr || renderer == nullptr) {
    std::cerr << "Error: SDL_CreateWindowAndRenderer(): " << SDL_GetError()
              << "\n";
    return 1;
  }

  // Set the default window position and display.
  SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(window);

  // Initialise the Dear ImGui systems and set configuration flags.
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();

  // Enable Keyboard controls.
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  // Enable Gamepad controls.
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

  // Synchronise ImGui with the SDL3 renderer.
  ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer3_Init(renderer);

  // Sentinel value for event loop.
  bool done = false;
  // Main event loop implementation.
  while (!done) {
    // Handle for tracking SDL events within event loop.
    SDL_Event event;

    // Handle event loops in a continuous stream per update.
    while (SDL_PollEvent(&event)) {
      // ImGui needs to be synchronised against SDL window events.
      ImGui_ImplSDL3_ProcessEvent(&event);
      // Check for any exit requests and set sentinel accordingly.
      if (event.type == SDL_EVENT_QUIT ||
          event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
              event.window.windowID == SDL_GetWindowID(window))
        done = true;
    }

    // Initialise a new frame within the ImGui windowing system.
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // Begin a new window within the SDl window.
    ImGui::Begin("Hello, World!");
    // Display some standard text, called from the Rust compiler.
    ImGui::Text("%s", greet());
    // Signal to ImGui the end of the current window.
    ImGui::End();

    // Render through ImGui...
    ImGui::Render();
    // ...and then send ImGui's data to the SDL renderer.
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  // Cleanup all relevant ImGui subsystems.
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  // Cleanup all relevant SDL subsystems.
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
