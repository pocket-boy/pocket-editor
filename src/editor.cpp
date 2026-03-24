#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_dialog.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_sdlrenderer3.h"
#include "imgui.h"
#include "compiler.hpp"
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
std::string lastCompResult = "";
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
  SDL_SetRenderVSync(renderer, 1);
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
  def.mCaseSensitive = true;
  def.mSingleLineComment = "#";
  def.mCommentStart = "/*";
  def.mCommentStart = "*/";

  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("(return|def|var|local|const|import|if|for|while|else|match|case|int|u8|u16|let|unit)", TextEditor::PaletteIndex::Keyword));
  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("[\\. \\t]([A-Za-z0-9_]+)\\(", TextEditor::PaletteIndex::MethodCall));

  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("([,;:\\(\\)<>]|->)", TextEditor::PaletteIndex::Punctuation));
  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("([A-Z][A-Z0-9_]*?)([\\.,;\\(\\) ]|$)", TextEditor::PaletteIndex::Constant));
  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("([A-Z][A-Za-z0-9_]*?)([\\.,;\\(\\) ]|$)", TextEditor::PaletteIndex::TypeName));
  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("([^A-Za-z_][0-9]+)", TextEditor::PaletteIndex::NumericalConstant));
  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("(render)", TextEditor::PaletteIndex::KnownIdentifier));
  def.mTokenRegexStrings.push_back(std::make_pair<std::string, TextEditor::PaletteIndex>("(\\\"(\\\\.|[^\\\"])*\\\")", TextEditor::PaletteIndex::String));
  def.mName = "PocketScript";

  editor.SetLanguageDefinition(def);
  TextEditor::Palette colors = { {
    0xff7f7f7f,	// Default
    0xffd69c56,	// Keyword
    0xff00ff00,	// Number
    0xff7070e0,	// String
    0xff70a0e0, // Char literal
    0xff6f6f6f, // Punctuation
    0xff408080,	// Preprocessor
    0xffaaaaaa, // Identifier
    0xffa3d653, // Known identifier
    0xffc040a0, // Preproc identifier
    0xff00ff00, // Comment (single line)
    0xffFF8F00, // Comment (multi line)
    0xff282828, // Background
    0xffe0e0e0, // Cursor
    0x80a06020, // Selection
    0x800020ff, // ErrorMarker
    0x40f08000, // Breakpoint
    0xff707000, // Line number
    0x40000000, // Current line fill
    0x40808080, // Current line fill (inactive)
    0x40a0a0a0, // Current line edge
    0xff996666, //Type Name
    0xff0088ff, //Constant
    0xff00ff00, //Numerical Constant
    0xff00aabb, //Method call
  } };
  editor.SetPalette(colors);
  editor.SetText("#Open a file in the menu to begin");
  editor.SetHandleKeyboardInputs(true);
  //editor.SetColorizerEnable(true);
  SDL_StartTextInput(window);

  CompilerHandle handle;
  std::unordered_map<std::string, std::string> compiledModules;

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
            contents[openIndex] = editor.GetText();
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
          handle.drop_module(openPaths[openIndex]);
          std::cout << "File name: " << openPaths[openIndex] << std::endl;
          std::cout << "File content: " << contents[openIndex] << std::endl;
          std::cout << "Load result: " << handle.load_module(openPaths[openIndex], contents[openIndex].substr(0, contents[openIndex].size() - 1).c_str()) << std::endl;
          ResultHandle result = handle.try_build();
          bool isErr;

          for (auto openPath : openPaths) {
            StringHandle resultStringHandle = result.module(openPaths[openIndex], &isErr);
            std::cout << "Results for file '" << openPath << "':" << std::endl;
            std::cout << "Error? " << (isErr ? "True" : "False") << std::endl;
            std::cout << (isErr ? "Error: " : "Parse result: ") << resultStringHandle.inner << std::endl;
          }
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
            ImGui::PushID(100 + i * 2);
            if(ImGui::Selectable(openPaths[i] + lastSlash + 1, openAtRender == i, 0, ImVec2(120, 20))){
                std::string editorContent = editor.GetText();
                contents[openIndex] = editorContent.substr(0, editorContent.size() - 1);
                openIndex = i;
                editor.SetText(contents.size() == openPaths.size() ? contents[openIndex] : load_from_path(openPaths[i]));
            }
            if (modified)
                ImGui::PopStyleColor();

            ImGui::PopID();
            ImGui::SetCursorPosX(130);
            ImGui::SetCursorPosY(30 + 20 * i);
            ImGui::PushID(100 + (i * 2) + 1);
            if (ImGui::Button("X", ImVec2(20, 20))) {
              openPaths.erase(openPaths.begin() + i);
              if (openIndex == i) {
                  openIndex--;
                  if (openIndex != -1)
                    editor.SetText(contents[openIndex]);
                  else
                    editor.SetText("#Open a file in the menu to begin");
              }
            }
            ImGui::PopID();

        }
    }
    ImGui::SetCursorPos(ImVec2(150, 20));

    //editor
    editor.Render("TextEditor", ImVec2(450,580));
    if(ImGui::IsItemFocused() && keyUpInFrame && openIndex != -1){
        modifiedPaths[openIndex] = true;
    }
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
