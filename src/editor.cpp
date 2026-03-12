#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_dialog.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"
#include <semaphore.h>
#include <fstream>
#include <fstream>
#include <iostream>
#include <stdlib.h>
#include "TextEditor.h"
#define SDL_HINT_FILE_DIALOG_DRIVER "zenity"
// Constant definition for default window width.
#define WIN_WIDTH (160 * 4)
// Constant definition for default window height.
#define WIN_HEIGHT (144 * 4)
static void SDLCALL fileopen_callback(void* userdata, const char* const* filelist, int filter);
static void SDLCALL filesaveas_callback(void* userdata, const char* const* filelist, int filter);
static void write_to_path(const char* const path, std::string text);
// Semaphore held during callbacks and render loop, to synchonize execution of callback functions.
sem_t renderSemaphore;
char* openpath;
int main(void) {
  //Initialize the semaphore before any code, non-process shared with a initial value of 1
  if(sem_init(&renderSemaphore, 0, 1)){
    std::cerr << "Sem issue";
    return 1;
  }
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

  TextEditor editor;
  auto lang = TextEditor::LanguageDefinition::CPlusPlus();
  editor.SetLanguageDefinition(lang);
  editor.SetPalette(TextEditor::GetLightPalette());
  editor.SetText("\x2f\x2f Open a file in the menu to begin");
  editor.SetHandleKeyboardInputs(true);
  SDL_StartTextInput(window);
  // Sentinel value for event loop.
  bool done = false;
  // Main event loop implementation.
  while (!done) {
    sem_wait(&renderSemaphore);
    // Handle for tracking SDL events within event loop.
    SDL_Event event;

    // Handle event loops in a continuous stream per update.
    while (SDL_PollEvent(&event)) {
      // ImGui needs to be synchronised against SDL window events.
      ImGui_ImplSDL3_ProcessEvent(&event);

      //Make Shift-Enter enter a new line, because I do this when typing brackets and IGTE doesn't do it by default.
      if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_RETURN && event.key.mod & (SDL_KMOD_LSHIFT|SDL_KMOD_RSHIFT)){
        /*SDL_Event manufacturedDown;
        SDL_Event manufacturedUp;
        SDL_memset(&manufacturedDown, 0, sizeof(manufacturedDown));
        SDL_memset(&manufacturedUp, 0, sizeof(manufacturedUp));
        manufacturedDown.type = SDL_EVENT_KEY_DOWN;
        manufacturedUp.type = SDL_EVENT_KEY_UP;
        manufacturedUp.key.key =
          manufacturedDown.key.key = SDLK_RETURN;
        SDL_PushEvent(&manufacturedDown);
        SDL_PushEvent(&manufacturedUp);*/
        editor.InsertText("\n");
      }
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
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Hello, World!", NULL, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
    ImGui::SetWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);

    // Have a file menu for save and open
    if(ImGui::BeginMenuBar()){
      if(ImGui::BeginMenu("File")){
        if(ImGui::MenuItem("Open...", "Ctrl-O")){
          SDL_ShowOpenFileDialog(fileopen_callback, &editor, window, nullptr, 0, nullptr, false);
        }
        if(ImGui::MenuItem("Save", "Ctrl-S")){
          if(openpath == nullptr){
            SDL_ShowSaveFileDialog(filesaveas_callback, &editor, window, nullptr, 0, nullptr);
          } else {
            write_to_path(openpath, editor.GetText());
          }
        }
        if(ImGui::MenuItem("Save As...", "Ctrl-Shift-S")){
          SDL_ShowSaveFileDialog(filesaveas_callback, &editor, window, nullptr, 0, nullptr);
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    // Display some standard text, called from the Rust compiler.
    editor.Render("TextEditor");
    // Signal to ImGui the end of the current window.
    ImGui::End();

    // Render through ImGui...
    ImGui::Render();
    // ...and then send ImGui's data to the SDL renderer.
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
    sem_post(&renderSemaphore);
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
static void SDLCALL fileopen_callback(void* userdata, const char* const* filelist, int filter){
  sem_wait(&renderSemaphore);
  if(filelist == nullptr){
    std::cerr << "SDL error when opening file prompt: " << SDL_GetError() << std::endl;
    sem_post(&renderSemaphore);
    return;
  }
  if(*filelist == nullptr || *filelist == ""){
    std::cerr << "Someone canceled..." << std::endl;
    sem_post(&renderSemaphore);
    return;
  }
  std::ifstream file(filelist[0]);
  if(file){
    std::ostringstream file_content;
    file_content << file.rdbuf();
    (*((TextEditor *)userdata)).SetText(file_content.str());
  } else {
    std::cerr << "Can't open the file that is at \"" << filelist[0] << "\".";
  }
  sem_post(&renderSemaphore);
  return;
}
static void SDLCALL filesaveas_callback(void* userdata, const char* const* filelist, int filter){
  sem_wait(&renderSemaphore);
  if(filelist == nullptr) {
    std::cerr << "SDL error when opening file prompt: " << SDL_GetError() << std::endl;
    sem_post(&renderSemaphore);
    return;
  }
  if(*filelist == nullptr || *filelist == ""){
    std::cerr << "Someone canceled..." << std::endl;
    sem_post(&renderSemaphore);
    return;
  }
  write_to_path(filelist[0], (*((TextEditor *)userdata)).GetText());
  free(openpath);
  openpath = (char*)malloc(sizeof(char) * (strlen(filelist[0]) + 1));
  strcpy(openpath, filelist[0]);
  sem_post(&renderSemaphore);
}
static void write_to_path(const char* const path, std::string text){
  std::ofstream file(path);
  if(file){
    file << text;
  } else {
    std::cerr << "Unable to write to the file that is at \"" << path << "\".";
  }
}
