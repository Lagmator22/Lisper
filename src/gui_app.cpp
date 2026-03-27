#include "gui_app.h"

#include "gui/background_transcriber.h"
#include "gui/layout.h"
#include "gui/state.h"
#include "gui/theme.h"

#include "interrupt.h"

#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_sdlrenderer2.h"
#include "imgui.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

constexpr float kMouseWheelScale = 0.32f;

bool initialize_window(SDL_Window **window, SDL_Renderer **renderer) {
  *window = SDL_CreateWindow("Lisper Studio", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, 1440, 920,
                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (*window == nullptr) {
    std::fprintf(stderr, "Failed to create window: %s\n", SDL_GetError());
    return false;
  }

  *renderer = SDL_CreateRenderer(
      *window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (*renderer == nullptr) {
    *renderer = SDL_CreateRenderer(
        *window, -1, SDL_RENDERER_SOFTWARE | SDL_RENDERER_PRESENTVSYNC);
  }
  if (*renderer == nullptr) {
    std::fprintf(stderr, "Failed to create renderer: %s\n", SDL_GetError());
    SDL_DestroyWindow(*window);
    *window = nullptr;
    return false;
  }

  return true;
}

void damp_mouse_wheel(SDL_Event *event) {
  if (event == nullptr || event->type != SDL_MOUSEWHEEL) {
    return;
  }

  const Sint32 original_x = event->wheel.x;
  const Sint32 original_y = event->wheel.y;

#if SDL_VERSION_ATLEAST(2, 0, 18)
  event->wheel.preciseX *= kMouseWheelScale;
  event->wheel.preciseY *= kMouseWheelScale;
#endif
  event->wheel.x = static_cast<Sint32>(static_cast<float>(event->wheel.x) *
                                       kMouseWheelScale);
  event->wheel.y = static_cast<Sint32>(static_cast<float>(event->wheel.y) *
                                       kMouseWheelScale);

  if (event->wheel.x == 0 && original_x != 0) {
    event->wheel.x = original_x > 0 ? 1 : -1;
  }
  if (event->wheel.y == 0 && original_y != 0) {
    event->wheel.y = original_y > 0 ? 1 : -1;
  }
}

float detect_content_scale(SDL_Window *window, SDL_Renderer *renderer) {
  int window_w = 0;
  int window_h = 0;
  int output_w = 0;
  int output_h = 0;
  SDL_GetWindowSize(window, &window_w, &window_h);
  SDL_GetRendererOutputSize(renderer, &output_w, &output_h);
  if (window_w <= 0 || window_h <= 0) {
    return 1.0f;
  }

  const float scale_x = static_cast<float>(output_w) / window_w;
  const float scale_y = static_cast<float>(output_h) / window_h;
  return std::max(1.0f, std::min(scale_x, scale_y));
}

} // namespace

int run_gui_app() {
  interrupt_state::reset();

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");
  SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");

  SDL_Window *window = nullptr;
  SDL_Renderer *renderer = nullptr;
  if (!initialize_window(&window, &renderer)) {
    SDL_Quit();
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;

  const float content_scale = detect_content_scale(window, renderer);
  SDL_RenderSetScale(renderer, content_scale, content_scale);

  const gui::Fonts fonts = gui::install_fonts(content_scale);
  io.FontGlobalScale = 1.0f / content_scale;
  gui::apply_style();

  ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
  ImGui_ImplSDLRenderer2_Init(renderer);

  gui::AppState state;
  gui::copy_string(state.language, "en");
  state.threads = gui::recommended_threads();
  gui::refresh_resolved_model(state);
  gui::append_log(state, "Lisper Studio is ready.");

  gui::BackgroundTranscriber job;
  bool done = false;

  while (!done) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      SDL_Event imgui_event = event;
      damp_mouse_wheel(&imgui_event);
      ImGui_ImplSDL2_ProcessEvent(&imgui_event);

      if (event.type == SDL_QUIT) {
        done = true;
      } else if (event.type == SDL_WINDOWEVENT &&
                 event.window.event == SDL_WINDOWEVENT_CLOSE &&
                 event.window.windowID == SDL_GetWindowID(window)) {
        done = true;
      } else if (event.type == SDL_DROPFILE) {
        const char *dropped = event.drop.file;
        if (dropped != nullptr) {
          gui::copy_string(state.input_path, fs::absolute(dropped).string());
          gui::append_log(state, "Loaded dropped file: " +
                                     gui::trim_copy(state.input_path.data()));
          SDL_free(event.drop.file);
        }
      }
    }

    if (done && job.running()) {
      job.cancel();
    }

    if (auto completed = job.consume_completed()) {
      state.status_line = completed->status;
      state.preview_text = completed->preview_text;
      state.final_output_path = completed->final_output_path;
      state.detected_language = completed->result.detected_language;
      state.duration_ms = completed->result.duration_ms;
      state.segment_count = static_cast<int>(completed->result.segments.size());
      state.last_run_success = completed->success;
      gui::append_log(state, completed->status);
      if (completed->cancelled) {
        gui::append_log(state, "Run ended after cancellation.");
      } else if (completed->success) {
        gui::append_log(state,
                        "Finished " +
                            std::string(gui::format_label(
                                gui::format_from_index(state.format_index))) +
                            " output.");
      }
    }

    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    gui::render_main_window(state, fonts, job);

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
  }

  job.cancel();
  job.wait();
  ImGui_ImplSDLRenderer2_Shutdown();
  ImGui_ImplSDL2_Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
