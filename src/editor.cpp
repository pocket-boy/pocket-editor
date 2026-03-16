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
#include <vector>
#define SDL_HINT_FILE_DIALOG_DRIVER "zenity"
// Constant definition for default window width.
#define WIN_WIDTH (250 * 4)
// Constant definition for default window height.
#define WIN_HEIGHT (150 * 4)
static void SDLCALL fileopen_callback(void* userdata, const char* const* filelist, int filter);
static void SDLCALL filesaveas_callback(void* userdata, const char* const* filelist, int filter);
static void write_to_path(const char* const path, std::string text);
static std::string load_from_path(const char* const path);

// Semaphore held during callbacks and render loop, to synchonize execution of callback functions.
sem_t renderSemaphore;
std::vector<char*> openPaths;
std::vector<bool> modifiedPaths;
std::vector<std::string> contents;
int openIndex = -1;
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
  TextEditor::LanguageDefinition def;
  def.mAutoIndentation = true;
  def.mSingleLineComment = "#";

  def.mKeywords.insert("data");
  def.mKeywords.insert("unit");
  def.mKeywords.insert("u16");

  editor.SetLanguageDefinition(def);
  editor.SetPalette(TextEditor::GetLightPalette());
  editor.SetText("#Open a file in the menu to begin");
  editor.SetHandleKeyboardInputs(true);
  editor.SetColorizerEnable(true);
  SDL_StartTextInput(window);
  // Sentinel value for event loop.
  bool done = false;
  // Main event loop implementation.
  while (!done) {
    sem_wait(&renderSemaphore);
    // Handle for tracking SDL events within event loop.
    SDL_Event event;
    bool keyUpInFrame = false;
    // Handle event loops in a continuous stream per update.
    while (SDL_PollEvent(&event)) {
      // ImGui needs to be synchronised against SDL window events.
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_KEY_UP)
        keyUpInFrame = true;
      //Make Shift-Enter enter a new line, because I do this when typing brackets and IGTE doesn't do it by default.
      if(event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_RETURN && event.key.mod & (SDL_KMOD_LSHIFT|SDL_KMOD_RSHIFT)){
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
    ImGui::SetWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);

    // Have a file menu for save, open, and run
    if(ImGui::BeginMenuBar()){
      if(ImGui::BeginMenu("File")){
        if(ImGui::MenuItem("Open...", "Ctrl-O")){
          SDL_ShowOpenFileDialog(fileopen_callback, &editor, window, nullptr, 0, nullptr, false);
        }
        if(ImGui::MenuItem("Save", "Ctrl-S")){
          if(openIndex == -1){
            SDL_ShowSaveFileDialog(filesaveas_callback, &editor, window, nullptr, 0, nullptr);
          } else {
            write_to_path(openPaths[openIndex], editor.GetText());
            modifiedPaths[openIndex] = false;
          }
        }
        if(ImGui::MenuItem("Save As...", "Ctrl-Shift-S")){
          SDL_ShowSaveFileDialog(filesaveas_callback, &editor, window, nullptr, 0, nullptr);
        }
        ImGui::EndMenu();
      }
      if(ImGui::BeginMenu("Run")){
          if (ImGui::MenuItem("Run File", "F6")){
              //Integrate compiler/interpreter
          }
          ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    if (openIndex == -1){
        ImGui::Selectable("  ^ Open a file", false, ImGuiSelectableFlags_::ImGuiSelectableFlags_Disabled, ImVec2(150, 20));
    } else {
        int openAtRender = openIndex;
        for (int i = 0; i < openPaths.size(); ++i) {
            ImGui::SetCursorPosX(0);

            int lastSlash = -1;
            int pathLength = strlen(openPaths[i]);
            for (int j = 0; j < pathLength; ++j) {
                if(openPaths[i][j] == '/'){
                    lastSlash = j;
                }
            }
            bool modified = modifiedPaths[i];
            if (modified)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.3f, .4f, 1, 1));
            ImGui::PushID(openPaths[i]);
            if(ImGui::Selectable(openPaths[i] + lastSlash + 1, openAtRender == i, 0, ImVec2(150, 20))){
                std::string editorContent = editor.GetText();
                contents[openIndex] = editorContent.substr(0, editorContent.size() - 1);
                openIndex = i;
                editor.SetText(contents.size() == openPaths.size() ? contents[openIndex] : load_from_path(openPaths[i]));
            }
            ImGui::PopID();
            if (modified)
                ImGui::PopStyleColor();
        }
    }
    ImGui::SetCursorPos(ImVec2(150, 20));
    editor.Render("TextEditor", ImVec2(450,580));
    if(ImGui::IsItemFocused() && keyUpInFrame && openIndex != -1){
        modifiedPaths[openIndex] = true;
    }
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
  if(*filelist == nullptr || *filelist[0] == '\0'){
    std::cerr << "Someone canceled..." << std::endl;
    sem_post(&renderSemaphore);
    return;
  }

    if (openIndex != -1)
        contents[openIndex] = ((TextEditor*)userdata)->GetText();
  for(int i = 0; i < openPaths.size(); ++i){
      if (!strcmp(openPaths[i], filelist[0])){
          std::cout << "Open at pos " << i << std::endl;
          openIndex = i;
          ((TextEditor*)userdata)->SetText(load_from_path(openPaths[i]));
          sem_post(&renderSemaphore);
          return;
      }
  }
  std::string fileContent = load_from_path(filelist[0]);

  char* openedPath = (char*)malloc(sizeof(char) * (strlen(filelist[0]) + 1));
  strcpy(openedPath, filelist[0]);
  openPaths.push_back(openedPath);
  contents.push_back(fileContent);

  openIndex = openPaths.size() - 1;
  modifiedPaths.push_back(false);

  ((TextEditor*)userdata)->SetText(load_from_path(openedPath));
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
  std::string content = (*((TextEditor *)userdata)).GetText();
  write_to_path(filelist[0], content);

  char* openedPath = (char*)malloc(sizeof(char) * (strlen(filelist[0]) + 1));
  strcpy(openedPath, filelist[0]);
  openPaths.push_back(openedPath);

  if (openIndex != -1)
      contents[openIndex] = ((TextEditor*)userdata)->GetText();
  openIndex = openPaths.size() - 1;
  modifiedPaths.push_back(false);
  contents.push_back(content);
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
static std::string load_from_path(const char* const path){
    std::ifstream file(path);
    if(file){
        std::ostringstream file_content;
        file_content << file.rdbuf();
        return file_content.str();
    } else {
        std::cerr << "Can't open the file that is at \"" << path << "\".";
    }
    return "Error loading file";
}
